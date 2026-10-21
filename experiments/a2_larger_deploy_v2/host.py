"""Deployment v2: immutable build provenance, frozen execution identity, WSL stop.

Reuse the preserved v1 archive, ELF, resource and capability checks. No compiler
or solver build is performed. No experimental worker is invoked by this module.
"""
from __future__ import annotations
import hashlib
import importlib.metadata
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
_loader = importlib.util.spec_from_file_location(
    '_a2_preserved_deployment_v1', ROOT / 'experiments/a2_larger_deploy/host.py')
previous = importlib.util.module_from_spec(_loader)
_loader.loader.exec_module(previous)
from selection import AMENDMENT, DEFAULT_INPUT, frozen, implementation_identity as launch_identity
from guards import require_committed

SPEC_PATH = Path(__file__).with_name('spec.json')
SPEC = json.loads(SPEC_PATH.read_text())
BUILD_PATH = previous.BUILD_PATH
command = previous.command
resources = previous.resources
restore_binary = previous.restore_binary
probe_capabilities = previous.probe_capabilities
dependency_map = previous.dependency_map
binary_runtime = previous.binary_runtime
cpu_topology = previous.cpu_topology


def deployment_identity():
    return {**previous.deployment_identity(), **{
        p.relative_to(ROOT).as_posix(): frozen.file_hash(p)
        for p in sorted(Path(__file__).parent.iterdir()) if p.suffix in ('.py', '.json')}}


def verify_repository(checkpoint, selection_path):
    result = previous.verify_repository(checkpoint, selection_path)
    subprocess.run(['git', 'merge-base', '--is-ancestor', SPEC['base_checkpoint'], checkpoint],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for relative in deployment_identity(): require_committed(ROOT / relative, checkpoint)
    result['deployment_policy'] = SPEC['schema']
    result['deployment_files_checked'] = len(deployment_identity())
    build_provenance()
    return result


def build_provenance():
    """The build record, not the collection machine's g++, defines provenance."""
    if frozen.file_hash(BUILD_PATH) != SPEC['build_identity_sha256']:
        raise ValueError('Immutable original build provenance changed')
    build = json.loads(BUILD_PATH.read_text())
    if (build['host']['compiler_sha256'] != SPEC['build_compiler_sha256'] or
            build['binary_sha256'] != SPEC['binary_sha256']):
        raise ValueError('Original compiler/binary provenance mismatch')
    if frozen.file_hash(frozen.BINARY) != SPEC['binary_sha256']:
        raise ValueError('Frozen solver binary changed; rebuilding is prohibited')
    return dict(record_sha256=SPEC['build_identity_sha256'],
                compiler_path=build['host']['compiler'],
                compiler_version=build['host']['compiler_version'],
                compiler_sha256=build['host']['compiler_sha256'],
                build_flags=build['host']['build_flags'], binary_sha256=build['binary_sha256'])


def installed_compiler():
    """Inventory only. No equality requirement with the original build driver."""
    found = shutil.which('g++')
    if found is None: return {'present': False}
    path = Path(found).resolve()
    # Preserve invocation spelling because GCC's version line includes argv[0].
    version = command('g++', '--version').splitlines()[0]
    if not version.strip(): raise RuntimeError('Installed g++ version is unavailable')
    return dict(present=True, invoked_as='g++', path=str(path),
                version=version, sha256=frozen.file_hash(path))


def placement_observation():
    """Guest-visible metadata is not evidence of a fixed host physical core."""
    release = platform.release(); version = platform.version()
    signals = {'kernel_release': release, 'kernel_version': version,
               'WSL_DISTRO_NAME_present': bool(os.environ.get('WSL_DISTRO_NAME')),
               'WSL_INTEROP_present': bool(os.environ.get('WSL_INTEROP'))}
    detector = shutil.which('systemd-detect-virt')
    virtualization = 'unavailable'
    if detector:
        r = subprocess.run([detector], text=True, capture_output=True, timeout=5, check=False)
        if r.returncode not in (0, 1): raise RuntimeError('Virtualization inspection failed')
        virtualization = r.stdout.strip() or 'unknown'
    is_wsl = (any(s in (release + ' ' + version).lower() for s in ('microsoft', 'wsl')) or
              signals['WSL_DISTRO_NAME_present'] or signals['WSL_INTEROP_present'] or
              virtualization == 'wsl')
    return dict(wsl_detected=is_wsl, signals=signals, virtualization=virtualization,
                guest_affinity_proves_physical_core=False)


def timing_eligibility(environment):
    if environment['placement_observation']['wsl_detected']:
        return dict(status='BLOCKED', reason='WSL physical-core placement is unverified; '
                    'taskset pins a guest CPU, not a proven fixed host P/E core. '
                    'Use a reviewed stable native Linux host or host-pinned dedicated VM.')
    return dict(status='REQUIRES_PLACEMENT_REVIEW', reason='Metadata checks do not prove '
                'reservation or physical placement; publish reviewed placement evidence before collection.')


def capture_environment(cpu, output):
    # Same runtime/resource identity fields and enforcement as preserved v1,
    # except compiler roles are separated and placement metadata is added.
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise RuntimeError('This frozen binary requires Linux x86_64')
    if list(sys.version_info[:2]) != previous.SPEC['python_major_minor']:
        raise RuntimeError('Deployment requires CPython 3.12; do not substitute silently')
    if importlib.metadata.version('numpy') != previous.SPEC['numpy_version']:
        raise RuntimeError('Install the frozen NumPy 2.3.5 reference dependency')
    allowed = sorted(os.sched_getaffinity(0))
    if cpu not in allowed: raise RuntimeError('Selected CPU is outside allowed affinity')
    provenance = build_provenance()
    compiler = installed_compiler()
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
                build_provenance=provenance, installed_compiler=compiler,
                placement_observation=placement_observation(), binary_sha256=frozen.file_hash(frozen.BINARY),
                binary_runtime=binary_runtime(), protocol_sha256=frozen.file_hash(frozen.PROTOCOL_PATH),
                manifest_sha256=frozen.file_hash(DEFAULT_INPUT / 'input-manifest.json'),
                original_sources=frozen.source_identity(), launch_sources=launch_identity(),
                deployment_sources=deployment_identity(), amendment_sha256=frozen.file_hash(AMENDMENT),
                protected_subtrees={k: command('git', 'rev-parse', 'HEAD:' + k).strip()
                                    for k in frozen.PROTOCOL['freeze']['git_subtrees']},
                timing_environment={k: os.environ.get(k) for k in
                                    ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS')})


def require_environment(path, cpu, authorization):
    document = json.loads(Path(path).read_text())
    if document.get('schema') != 'a2-larger-stable-collection-host-v2' or document.get('status') != 'frozen_for_collection':
        raise RuntimeError('A prepared and published v2 stable-host record is required')
    if document.get('stable_execution_strategy') not in ('reserved_physical_host', 'dedicated_non_migrating_vm'):
        raise RuntimeError('Stable host reservation required')
    if not all(document.get(k) for k in ('stable_asset_identity', 'reservation_evidence', 'physical_placement_evidence')):
        raise RuntimeError('Missing stable host/reservation/physical-placement evidence')
    require_committed(path, authorization)
    if command('git', 'rev-parse', 'HEAD').strip() != authorization:
        raise RuntimeError('Checkout differs from the current collection authorization checkpoint')
    for relative in deployment_identity(): require_committed(ROOT / relative, authorization)
    for relative in launch_identity(): require_committed(ROOT / relative, authorization)
    require_committed(AMENDMENT, authorization)
    current = capture_environment(cpu, document['study_output_root'])
    if timing_eligibility(current)['status'] == 'BLOCKED':
        raise RuntimeError(timing_eligibility(current)['reason'])
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
