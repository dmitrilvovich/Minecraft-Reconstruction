"""Fail-closed environment, prerequisite, dispatch and resume boundaries."""
from __future__ import annotations
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zlib

from selection import (AMENDMENT, ROOT, Scope, case_stage, frozen,
                       implementation_identity, legacy, stage_order)


def capture_environment(cpu):
    result = legacy.host_identity()
    result.update(pinned_cpu=cpu, numpy=importlib.metadata.version('numpy'),
                  zlib_compile=zlib.ZLIB_VERSION, zlib_runtime=zlib.ZLIB_RUNTIME_VERSION,
                  python_executable_sha256=frozen.file_hash(Path(os.sys.executable).resolve()),
                  timing_environment={k: os.environ.get(k) for k in
                                      ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'OMP_NUM_THREADS',
                                       'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS')})
    return result


def require_committed(path, checkpoint):
    path = Path(path).resolve(); relative = path.relative_to(ROOT).as_posix()
    if not re.fullmatch('[0-9a-f]{40}', checkpoint):
        raise ValueError('Published authorization commit required')
    content = subprocess.check_output(['git', 'show', checkpoint + ':' + relative], cwd=ROOT)
    if content != path.read_bytes():
        raise ValueError('Launch artifact differs from published authorization commit: ' + relative)


def require_environment(path, cpu, authorization):
    document = json.loads(Path(path).read_text())
    if document.get('status') != 'frozen_for_collection':
        raise RuntimeError('Collection timing environment is not selected/frozen; no worker may start')
    if document.get('stable_execution_strategy') not in ('reserved_physical_host', 'dedicated_non_migrating_vm'):
        raise RuntimeError('A stable reserved execution environment is required')
    if not document.get('stable_asset_identity') or not document.get('reservation_evidence'):
        raise RuntimeError('Missing stable host identity/reservation evidence')
    require_committed(path, authorization)
    for relative in implementation_identity():
        require_committed(ROOT / relative, authorization)
    require_committed(AMENDMENT, authorization)
    current = capture_environment(cpu)
    build = json.loads((ROOT / 'results/a2-scaling-larger-v1/build-identity.json').read_text())
    for key in ('compiler_sha256', 'compiler_version', 'build_flags', 'binary_sha256'):
        if current[key] != build['host'][key]:
            raise RuntimeError('Original build/compiler identity changed: ' + key)
    if current != document['environment']:
        raise RuntimeError('Collection environment changed; pause before any worker call')
    if cpu not in current['allowed_cpus'] or current['source_subtrees'] != frozen.PROTOCOL['freeze']['git_subtrees']:
        raise RuntimeError('CPU affinity or accepted source identity mismatch')
    # Inspect and restore affinity without starting a process or solver call.
    old_affinity = os.sched_getaffinity(0)
    try:
        os.sched_setaffinity(0, {cpu})
        if os.sched_getaffinity(0) != {cpu}:
            raise RuntimeError('CPU pinning unavailable')
    finally:
        os.sched_setaffinity(0, old_affinity)
    available = next(int(x.split()[1]) * 1024 for x in Path('/proc/meminfo').read_text().splitlines()
                     if x.startswith('MemAvailable:'))
    maximum = Path('/sys/fs/cgroup/memory.max').read_text().strip()
    used = int(Path('/sys/fs/cgroup/memory.current').read_text())
    if maximum != 'max':
        available = min(available, int(maximum) - used)
    if available < frozen.PROTOCOL['budgets']['inference_worker_memory_bytes']:
        raise RuntimeError('Less than the declared worker ceiling is available in RAM')
    return current, document


def expected_context(identity, job_id, cs, epoch):
    keys = hashlib.sha256(); count = 0
    for c, mode, op, query in legacy.call_plan(cs, epoch):
        keys.update((frozen.canonical(legacy.call_id(c, mode, op, query, epoch)) + '\n').encode('ascii'))
        count += 1
    return {**identity, 'epoch': epoch, 'job_id': job_id, 'expected_calls': count,
            'expected_keys_sha256': keys.hexdigest()}


def verify_saved_job(path, identity, cs, epoch):
    context = expected_context(identity, cs[0]['job_id'], cs, epoch)
    frozen.verify_archive(path, expected_context=context)
    marker = Path(str(path) + '.complete.json')
    if marker.exists():
        frozen.verify_archive(path, json.loads(marker.read_text()), context)
    if sum(1 for _ in frozen.read_records(path)) != context['expected_calls']:
        raise ValueError('Saved job call count mismatch')
    # Exact ordered call IDs are verified in the archive context. Also reject
    # foreign case IDs, job labels, epochs or API metadata in a resumed row.
    expected = iter(legacy.call_plan(cs, epoch))
    for row in frozen.read_records(path):
        c, mode, op, query = next(expected)
        if (row['case_record_id'] != c['record_id'] or row['job_id'] != c['job_id']
                or row['epoch'] != epoch or row['mode'] != frozen.MODES[mode]
                or row['operation'] != op or row['query'] != query):
            raise ValueError('Saved job dispatch metadata mismatch')


def check_prerequisites(output, identity, selection, all_cases):
    """Earlier stages must already be sealed; never execute them implicitly."""
    output = Path(output); chosen = {tuple(s) for s in selection['stages']}
    last = max(stage_order().index(s) for s in chosen)
    considered = stage_order()[:last + 1]
    jobs = {}
    for c in all_cases:
        jobs.setdefault(frozen.canonical(c['job_id']), []).append(c)
    missing = []
    for key, cs in jobs.items():
        stage = case_stage(cs[0])
        if stage not in considered:
            continue
        for epoch in range(selection['epochs']):
            path = output / 'jobs' / (frozen.digest([epoch, json.loads(key)]) + '.jsonl.gz')
            partials = [Path(str(path) + '.partial.jsonl'), Path(str(path) + '.partial.gz')]
            if any(p.exists() for p in partials):
                raise RuntimeError('Interrupted evidence requires review; no automatic rerun')
            if path.exists():
                verify_saved_job(path, identity, cs, epoch)
            elif Path(str(path) + '.complete.json').exists():
                raise ValueError('Completion marker without its archive')
            elif stage not in chosen:
                missing.append({'stage': list(stage), 'epoch': epoch, 'job_id': cs[0]['job_id']})
    if missing:
        raise RuntimeError('Prior protocol stages incomplete; refusing implicit out-of-batch execution: '
                           + frozen.canonical(missing))
    return considered


def bind_batch(output, selection):
    path = Path(output) / 'batches' / frozen.digest(selection) / 'selection.json'
    if path.exists() and json.loads(path.read_text()) != selection:
        raise ValueError('Resume selection differs')
    frozen.atomic_json(path, selection)
    return path.parent


class ScopedWorker:
    def __init__(self, scratch, cpu, scope):
        self.scope = scope
        self.worker = legacy.Worker(scratch, cpu)

    @property
    def proc(self):
        return self.worker.proc

    def call(self, case, factors, mode, op, query=None):
        self.scope.check_case(case)
        return self.worker.call(case, factors, mode, op, query)

    def close(self):
        self.worker.close()
