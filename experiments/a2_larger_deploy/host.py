"""Portable deployment checks and stable collection identity; no solver execution."""
from __future__ import annotations
import gzip
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import re
import selectors
import shutil
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'experiments/a2_larger_launch'))
from selection import (AMENDMENT, DEFAULT_INPUT, MASTER_SHA256, frozen,
                       implementation_identity as launch_identity)
from guards import require_committed

SPEC_PATH = Path(__file__).with_name('spec.json')
SPEC = json.loads(SPEC_PATH.read_text())
BUILD_PATH = ROOT / 'results/a2-scaling-larger-v1/build-identity.json'


def command(*args, timeout=20):
    return subprocess.check_output(list(map(str, args)), cwd=ROOT, text=True,
                                   stderr=subprocess.STDOUT, timeout=timeout)


def deployment_identity():
    return {p.relative_to(ROOT).as_posix(): frozen.file_hash(p)
            for p in sorted(Path(__file__).parent.iterdir()) if p.suffix in ('.py', '.json')}


def restore_binary():
    """Materialize the published bytes, never build or execute them."""
    build = json.loads(BUILD_PATH.read_text())
    source = ROOT / 'results/a2-scaling-larger-v1' / build['binary_file']
    if frozen.file_hash(source) != SPEC['binary_archive_sha256']:
        raise ValueError('Published binary archive hash mismatch')
    data = gzip.decompress(source.read_bytes())
    if len(data) != build['binary_bytes'] or hashlib.sha256(data).hexdigest() != SPEC['binary_sha256']:
        raise ValueError('Uncompressed binary size/hash mismatch')
    path = frozen.BINARY
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError('Existing binary differs; refusing replacement or rebuild')
        if not os.access(path, os.X_OK):
            raise ValueError('Existing verified binary is not executable; review permissions')
        return {'restored': False, 'sha256': SPEC['binary_sha256'], 'bytes': len(data)}
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='restore-worker-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write(data); out.flush(); os.fsync(out.fileno()); os.fchmod(out.fileno(), 0o755)
        # Never race another bootstrap or overwrite a binary installed meanwhile.
        os.link(temporary, path); frozen.sync_dir(path.parent)
    finally:
        Path(temporary).unlink(missing_ok=True)
    return {'restored': True, 'sha256': frozen.file_hash(path), 'bytes': len(data)}


def verify_repository(checkpoint, selection_path):
    if not re.fullmatch('[0-9a-f]{40}', checkpoint):
        raise ValueError('Supply the exact intended published checkpoint hash')
    if command('git', 'rev-parse', 'HEAD').strip() != checkpoint:
        raise ValueError('Checkout HEAD differs from the requested checkpoint')
    subprocess.run(['git', 'merge-base', '--is-ancestor', SPEC['base_checkpoint'], checkpoint],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    subprocess.run(['git', 'diff', '--quiet', 'HEAD', '--'], cwd=ROOT, check=True)
    files = set(deployment_identity()) | set(launch_identity()) | set(frozen.source_identity())
    files |= {AMENDMENT.relative_to(ROOT).as_posix(), frozen.PROTOCOL_PATH.relative_to(ROOT).as_posix(),
              BUILD_PATH.relative_to(ROOT).as_posix(),
              'results/a2-scaling-larger-v1/worker-linux-x86_64.gz',
              'results/a2-scaling-larger-v1/inputs/input-manifest.json',
              'results/a2-scaling-larger-v1/inputs/preparation-receipt.json'}
    files |= {str((Path(selection_path) / name).resolve().relative_to(ROOT))
              for name in ('selection.json', 'selected-design-ids.jsonl.gz')}
    for relative in sorted(files):
        require_committed(ROOT / relative, checkpoint)
    subtrees = {name: command('git', 'rev-parse', checkpoint + ':' + name).strip()
                for name in frozen.PROTOCOL['freeze']['git_subtrees']}
    if subtrees != frozen.PROTOCOL['freeze']['git_subtrees']:
        raise ValueError('Accepted solver/reference subtree differs')
    return {'checkpoint': checkpoint, 'checked_files': len(files), 'protected_subtrees': subtrees}


def effective_cgroups():
    memberships = [line[3:] for line in Path('/proc/self/cgroup').read_text().splitlines() if line.startswith('0::')]
    mounts = [line for line in Path('/proc/self/mountinfo').read_text().splitlines() if ' - cgroup2 ' in line]
    if len(memberships) != 1 or not mounts:
        raise RuntimeError('Unified cgroup v2 visibility is required')
    membership = memberships[0]; directory = None; mountpoint = None
    for line in mounts:
        fields = line.split(); mount_root = fields[3]; mounted = Path(fields[4])
        if membership == '/' or membership == mount_root:
            candidate = mounted
        elif mount_root == '/':
            candidate = mounted / membership.lstrip('/')
        elif membership.startswith(mount_root.rstrip('/') + '/'):
            candidate = mounted / membership[len(mount_root):].lstrip('/')
        else:
            continue
        if candidate.is_dir() and (candidate == mounted or mounted in candidate.parents):
            directory = candidate; mountpoint = mounted; break
    if directory is None:
        raise RuntimeError('Cannot resolve the effective cgroup; do not guess its limits')
    limits = []; headrooms = []
    while True:
        row = {'path': str(directory)}
        for name in ('memory.max', 'memory.swap.max', 'cpu.max', 'cpuset.cpus.effective', 'cpuset.mems.effective'):
            p = directory / name
            if p.exists(): row[name] = p.read_text().strip()
        if 'memory.max' in row and row['memory.max'] != 'max':
            usage = int((directory / 'memory.current').read_text())
            headrooms.append(max(0, int(row['memory.max']) - usage))
        limits.append(row)
        if directory == mountpoint: break
        directory = directory.parent
    return limits, min(headrooms) if headrooms else None


def resources(output):
    output = Path(output).resolve()
    if not output.is_dir(): raise ValueError('Create the intended durable study output directory first')
    info = {line.split(':')[0]: int(line.split()[1]) * 1024
            for line in Path('/proc/meminfo').read_text().splitlines()
            if line.startswith(('MemTotal:', 'MemAvailable:'))}
    limits, headroom = effective_cgroups()
    available = min(info['MemAvailable'], headroom) if headroom is not None else info['MemAvailable']
    disk = shutil.disk_usage(output)
    result = dict(study_output_root=str(output), ram_total_bytes=info['MemTotal'],
                  available_ram_bytes=available, disk_total_bytes=disk.total,
                  disk_free_bytes=disk.free, cgroup_limits=limits,
                  worker_ceiling_bytes=frozen.PROTOCOL['budgets']['inference_worker_memory_bytes'],
                  required_disk_reserve_bytes=20 * 1024**3)
    if available < result['worker_ceiling_bytes']:
        raise RuntimeError('Less than 4 GiB effective available RAM: ' + frozen.canonical(result))
    if disk.free < result['required_disk_reserve_bytes']:
        raise RuntimeError('Less than the declared 20 GiB free disk reserve: ' + frozen.canonical(result))
    if not os.access(output, os.W_OK | os.X_OK): raise RuntimeError('Study directory is not writable')
    return result


def dependency_map(path):
    """ldd traces dependencies of a verified ELF; no program main/API is run."""
    output = command('ldd', path)
    if 'not found' in output or 'not a dynamic executable' in output:
        raise RuntimeError('Unresolved runtime dependency: ' + output)
    paths = set(re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)\s+\(', output, flags=re.M))
    if not paths: raise RuntimeError('No runtime dependencies could be inspected')
    return {p: {'resolved_path': str(Path(p).resolve()), 'sha256': frozen.file_hash(Path(p).resolve())}
            for p in sorted(paths)}


def binary_runtime():
    if frozen.file_hash(frozen.BINARY) != SPEC['binary_sha256']:
        raise ValueError('Unverified binary; no dependency inspection permitted')
    header = command('readelf', '-h', frozen.BINARY)
    if 'ELF64' not in header or 'Advanced Micro Devices X86-64' not in header:
        raise RuntimeError('Expected ELF64 x86-64 binary')
    program = command('readelf', '-lW', frozen.BINARY)
    match = re.search(r'Requesting program interpreter: ([^\]]+)', program)
    if not match or match.group(1) != SPEC['binary_metadata']['interpreter'] or not Path(match.group(1)).is_file():
        raise RuntimeError('Required glibc ELF loader is unavailable')
    versions = command('readelf', '--version-info', frozen.BINARY)
    needed = {}; current = None
    for line in versions.splitlines():
        file_match = re.search(r'File: (\S+)', line)
        if file_match: current = file_match.group(1); needed[current] = []
        name = re.search(r'Name: (\S+)', line)
        if name and current: needed[current].append(name.group(1))
    libraries = dependency_map(frozen.BINARY)
    for soname, required in needed.items():
        path = next((p for p in libraries if Path(p).name == soname), None)
        if path is None: raise RuntimeError('Missing required library ' + soname)
        definitions = command('readelf', '--version-info', path)
        provided = set(re.findall(r'Name: (\S+)', definitions))
        if set(required) - provided:
            raise RuntimeError('Required symbol versions missing from ' + soname)
    return {'interpreter': match.group(1), 'required_symbol_versions': needed,
            'libraries': libraries, 'isa_notes': command('readelf', '-n', frozen.BINARY)}


def cpu_topology():
    rows = []
    for path in sorted(Path('/sys/devices/system/cpu').glob('cpu[0-9]*'), key=lambda p: int(p.name[3:])):
        row = {'cpu': int(path.name[3:])}
        for name in ('physical_package_id', 'die_id', 'core_id', 'thread_siblings_list'):
            p = path / 'topology' / name
            if p.exists(): row[name] = p.read_text().strip()
        online = path / 'online'; row['online'] = online.read_text().strip() if online.exists() else '1'
        governor = path / 'cpufreq/scaling_governor'
        if governor.exists(): row['scaling_governor'] = governor.read_text().strip()
        rows.append(row)
    return rows


def capture_environment(cpu, output):
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise RuntimeError('This frozen binary requires Linux x86_64')
    if list(sys.version_info[:2]) != SPEC['python_major_minor']:
        raise RuntimeError('Deployment requires CPython 3.12; do not substitute silently')
    if importlib.metadata.version('numpy') != SPEC['numpy_version']:
        raise RuntimeError('Install the frozen NumPy 2.3.5 reference dependency')
    allowed = sorted(os.sched_getaffinity(0))
    if cpu not in allowed: raise RuntimeError('Selected CPU is outside allowed affinity')
    compiler = Path(shutil.which('g++') or '/missing-g++').resolve()
    build = json.loads(BUILD_PATH.read_text())
    compiler_hash = frozen.file_hash(compiler)
    # Preserve the frozen invocation spelling: GCC includes argv[0] in this line.
    compiler_version = command('g++', '--version').splitlines()[0]
    if compiler_hash != SPEC['build_compiler_sha256'] or compiler_version != build['host']['compiler_version']:
        raise RuntimeError('Original compiler identity unavailable; do not rebuild or silently substitute it')
    cpuinfo = Path('/proc/cpuinfo').read_text()
    characteristics = {k: sorted(set(re.findall(r'^' + re.escape(k) + r'\s*:\s*(.+)$', cpuinfo, re.M)))
                       for k in ('model name', 'vendor_id', 'cpu family', 'model', 'stepping', 'microcode', 'flags')}
    machine_ids = {}
    for name in ('/etc/machine-id', '/sys/class/dmi/id/product_uuid'):
        p = Path(name)
        try: value = p.read_bytes().strip()
        except (FileNotFoundError, PermissionError): continue
        if value: machine_ids[name] = hashlib.sha256(value).hexdigest()
    if '/etc/machine-id' not in machine_ids: raise RuntimeError('Persistent machine identifier unavailable')
    state = resources(output)
    import numpy
    extension = next((Path(numpy.__file__).parent / '_core').glob('_multiarray_umath*.so'))
    return dict(kernel=platform.uname()._asdict(), os_release=Path('/etc/os-release').read_text(),
                machine_identifier_hashes=machine_ids, cpu=characteristics, topology=cpu_topology(),
                allowed_cpus=allowed, pinned_cpu=cpu, ram_total_bytes=state['ram_total_bytes'],
                cgroup_limits=state['cgroup_limits'], study_output_root=state['study_output_root'],
                python=platform.python_version(), python_executable=str(Path(sys.executable).resolve()),
                python_executable_sha256=frozen.file_hash(Path(sys.executable).resolve()),
                python_libraries=dependency_map(Path(sys.executable).resolve()), numpy=numpy.__version__,
                numpy_core_sha256=frozen.file_hash(extension), numpy_libraries=dependency_map(extension),
                zlib_compile=zlib.ZLIB_VERSION, zlib_runtime=zlib.ZLIB_RUNTIME_VERSION,
                compiler=str(compiler), compiler_version=compiler_version, compiler_sha256=compiler_hash,
                build_flags=build['host']['build_flags'], binary_sha256=frozen.file_hash(frozen.BINARY),
                binary_runtime=binary_runtime(), protocol_sha256=frozen.file_hash(frozen.PROTOCOL_PATH),
                manifest_sha256=frozen.file_hash(DEFAULT_INPUT / 'input-manifest.json'),
                original_sources=frozen.source_identity(), launch_sources=launch_identity(),
                deployment_sources=deployment_identity(), amendment_sha256=frozen.file_hash(AMENDMENT),
                protected_subtrees={k: command('git', 'rev-parse', 'HEAD:' + k).strip()
                                    for k in frozen.PROTOCOL['freeze']['git_subtrees']},
                timing_environment={k: os.environ.get(k) for k in
                                    ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS')})


def probe_capabilities(cpu):
    probe = Path(__file__).with_name('capability_probe.py')
    limits = json.loads(command(sys.executable, probe, 'limits', '--cpu', cpu, timeout=5))
    child = subprocess.Popen([sys.executable, str(probe), 'watchdog', '--cpu', str(cpu)],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    selector = selectors.DefaultSelector()
    try:
        selector.register(child.stdout, selectors.EVENT_READ)
        if not selector.select(3): raise RuntimeError('Synthetic watchdog child did not become ready')
        ready = json.loads(child.stdout.readline())
        if not ready['ready']: raise RuntimeError('Invalid synthetic watchdog handshake')
        try:
            child.wait(timeout=0.2)
        except subprocess.TimeoutExpired:
            child.kill(); child.wait(timeout=2)
        else:
            raise RuntimeError('Synthetic watchdog child exited before termination')
        if child.returncode != -9: raise RuntimeError('Cannot kill/reap timed-out child')
    finally:
        if child.poll() is None: child.kill(); child.wait(timeout=2)
        selector.close(); child.stdout.close(); child.stderr.close()
    return {'kind': 'infrastructure_only', 'limits': limits,
            'watchdog_process_kill_reap': 'PASS', 'synthetic_watchdog_seconds': 0.2,
            'actual_protocol_watchdog_seconds_unchanged': 65,
            'monotonic_clock': vars(time.get_clock_info('monotonic')),
            'solver_calls': 0, 'experimental_inputs': 0}


def require_environment(path, cpu, authorization):
    document = json.loads(Path(path).read_text())
    if document.get('schema') != 'a2-larger-stable-collection-host-v1' or document.get('status') != 'frozen_for_collection':
        raise RuntimeError('A prepared and published stable-host record is required')
    if document.get('stable_execution_strategy') not in ('reserved_physical_host', 'dedicated_non_migrating_vm'):
        raise RuntimeError('Stable host reservation required')
    if not document.get('stable_asset_identity') or not document.get('reservation_evidence'):
        raise RuntimeError('Missing stable host/reservation identity')
    require_committed(path, authorization)
    if command('git', 'rev-parse', 'HEAD').strip() != authorization:
        raise RuntimeError('Checkout differs from the current collection authorization checkpoint')
    for relative in deployment_identity(): require_committed(ROOT / relative, authorization)
    for relative in launch_identity(): require_committed(ROOT / relative, authorization)
    require_committed(AMENDMENT, authorization)
    current = capture_environment(cpu, document['study_output_root'])
    if current != document['environment']:
        raise RuntimeError('Frozen machine/runtime/collector identity changed; stop before worker execution')
    if current['protected_subtrees'] != frozen.PROTOCOL['freeze']['git_subtrees']:
        raise ValueError('Accepted solver/reference identity changed')
    if document['capability_checks']['watchdog_process_kill_reap'] != 'PASS':
        raise RuntimeError('Missing infrastructure capability evidence')
    affinity = os.sched_getaffinity(0)
    try:
        os.sched_setaffinity(0, {cpu})
        if os.sched_getaffinity(0) != {cpu}: raise RuntimeError('CPU affinity unavailable')
    finally: os.sched_setaffinity(0, affinity)
    return current, document
