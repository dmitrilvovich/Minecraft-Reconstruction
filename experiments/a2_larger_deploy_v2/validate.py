"""Bounded metadata/guard validation only; no experimental input evaluation."""
from __future__ import annotations
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

import host
from host import ROOT, SPEC, frozen
from selection import legacy
from pre_collection import prepare_host_record, check_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists(): raise ValueError('Preserve prior validation evidence')
    checks = []

    def check(name, condition):
        if not condition: raise AssertionError(name)
        checks.append({'check': name, 'result': 'PASS'})

    def reject(name, action):
        try: action()
        except (ValueError, RuntimeError, OSError):
            checks.append({'check': name, 'result': 'PASS'})
        else: raise AssertionError('Expected rejection: ' + name)

    def forbidden(*a, **kw):
        raise AssertionError('Experimental worker requested by infrastructure test')

    # Immutable inputs are hashed only. No load_frozen/generation/oracle work.
    manifest_path = host.DEFAULT_INPUT / 'input-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    originals = set(manifest['source_files']) | {
        'experiments/protocols/a2-scaling-larger-v1.json',
        'results/a2-scaling-larger-v1/build-identity.json',
        'results/a2-scaling-larger-v1/worker-linux-x86_64.gz',
        manifest_path.relative_to(ROOT).as_posix()}
    originals |= {(host.DEFAULT_INPUT / e['path']).relative_to(ROOT).as_posix()
                  for e in manifest['files'] + manifest['rigs']}
    before = {p: frozen.file_hash(ROOT / p) for p in sorted(originals)}
    cpu = min(os.sched_getaffinity(0))
    with patch.object(legacy, 'Worker', forbidden), tempfile.TemporaryDirectory(prefix='a2-deploy-v2-validation-') as tmp:
        tmp = Path(tmp)
        restored = host.restore_binary()
        provenance = host.build_provenance()
        check('original compiler and binary build provenance retained',
              provenance['compiler_sha256'] == SPEC['build_compiler_sha256'] and
              restored['sha256'] == SPEC['binary_sha256'])
        with patch.object(host.shutil, 'which', return_value=None), patch.object(host, 'command', forbidden):
            check('absent execution compiler accepted without invocation', host.installed_compiler() == {'present': False})
        fixture = tmp / 'g++-metadata-fixture'; fixture.write_text('infrastructure fixture, never executed\n')
        with patch.object(host.shutil, 'which', return_value=str(fixture)), patch.object(
                host, 'command', return_value='g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\n'):
            observed = host.installed_compiler()
            check('different installed compiler hash accepted and recorded',
                  observed['sha256'] == frozen.file_hash(fixture) and
                  observed['sha256'] != provenance['compiler_sha256'] and observed['present'])
        with patch.object(host.shutil, 'which', return_value=str(tmp / 'missing')), patch.object(
                host, 'command', return_value='synthetic version\n'):
            reject('present but unreadable compiler blocks', host.installed_compiler)
        actual_environment = host.capture_environment(cpu, tmp)
        reported_compiler = dict(present=True, invoked_as='g++', path='/usr/bin/x86_64-linux-gnu-g++-13',
            version='g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0',
            sha256='1353e9bdd29a7295c7226bf6c63abccce056d8cac31f112e5cdbecc3f28c2769')
        with patch.object(host, 'installed_compiler', return_value=reported_compiler):
            different = host.capture_environment(cpu, tmp)
        check('actual capture path accepts reported execution compiler difference',
              different['installed_compiler'] == reported_compiler and different['build_provenance'] == provenance)
        with patch.object(host, 'installed_compiler', return_value={'present': False}):
            absent = host.capture_environment(cpu, tmp)
        check('actual capture path accepts absence but preserves build record',
              absent['installed_compiler'] == {'present': False} and absent['build_provenance'] == provenance)
        with patch.object(host, 'BUILD_PATH', fixture):
            reject('changed build record rejected', host.build_provenance)
        with patch.object(frozen, 'BINARY', fixture):
            reject('replacement binary rejected', host.build_provenance)
        # No claim that this transient workspace is a native reserved host.
        synthetic = copy.deepcopy(different)
        synthetic['placement_observation'] = dict(wsl_detected=False, signals={}, virtualization='none',
                                                 guest_affinity_proves_physical_core=False)
        capabilities = {'watchdog_process_kill_reap': 'PASS'}
        report = dict(status='PASS', checked_at_utc='synthetic-only', repository={'checkpoint': '0' * 40},
                      resources={'study_output_root': str(tmp)}, environment=synthetic,
                      capabilities=capabilities, selection={'selection_sha256': 'synthetic'})
        prepared = prepare_host_record(report, 'reserved_physical_host', 'synthetic-asset',
                                       'synthetic reservation', 'synthetic placement; not a real host approval')
        check('prospective record includes both compiler roles and needs publication',
              prepared['environment']['build_provenance'] == provenance and
              prepared['environment']['installed_compiler'] == reported_compiler and
              prepared['publication_required_before_execution'] and not prepared['experimental_execution_authorized'])
        reject('missing physical-placement evidence blocks freeze', lambda: prepare_host_record(
            report, 'reserved_physical_host', 'synthetic', 'synthetic', ''))
        path = tmp / 'synthetic-host.json'; frozen.atomic_json(path, prepared)
        checkpoint = '0' * 40
        with patch.object(host, 'require_committed', lambda *a: None), patch.object(host, 'command', return_value=checkpoint):
            with patch.object(host, 'capture_environment', return_value=synthetic):
                check('unchanged frozen execution compiler differing from build compiler accepted',
                      host.require_environment(path, cpu, checkpoint)[0] == synthetic)
            for field in ('sha256', 'version', 'path', 'present'):
                changed = copy.deepcopy(synthetic); changed['installed_compiler'][field] = 'changed'
                with patch.object(host, 'capture_environment', return_value=changed):
                    reject('post-freeze installed compiler ' + field + ' drift rejected',
                           lambda: host.require_environment(path, cpu, checkpoint))
            for key in ('build_provenance', 'binary_sha256', 'binary_runtime', 'python_executable_sha256',
                        'python_libraries', 'numpy_core_sha256', 'numpy_libraries', 'kernel', 'topology',
                        'machine_identifier_hashes', 'allowed_cpus', 'pinned_cpu', 'cgroup_limits',
                        'protocol_sha256', 'manifest_sha256', 'original_sources', 'launch_sources',
                        'deployment_sources', 'timing_environment'):
                changed = copy.deepcopy(synthetic); changed[key] = {'changed': True}
                with patch.object(host, 'capture_environment', return_value=changed):
                    reject('post-freeze ' + key + ' drift rejected', lambda: host.require_environment(path, cpu, checkpoint))
            wsl = copy.deepcopy(synthetic); wsl['placement_observation']['wsl_detected'] = True
            wsl_report = {**report, 'environment': wsl}
            reject('WSL cannot prepare host record even with attestation', lambda: prepare_host_record(
                wsl_report, 'dedicated_non_migrating_vm', 'synthetic', 'synthetic', 'taskset alone'))
            frozen.atomic_json(path, {**prepared, 'environment': wsl})
            with patch.object(host, 'capture_environment', return_value=wsl):
                reject('even exact-matching forged WSL freeze cannot pass collection guard',
                       lambda: host.require_environment(path, cpu, checkpoint))
            absent_frozen = copy.deepcopy(synthetic); absent_frozen['installed_compiler'] = {'present': False}
            frozen.atomic_json(path, {**prepared, 'environment': absent_frozen})
            with patch.object(host, 'capture_environment', return_value=absent_frozen):
                check('compiler absent at freeze remains acceptable',
                      host.require_environment(path, cpu, checkpoint)[0] == absent_frozen)
            with patch.object(host, 'capture_environment', return_value=synthetic):
                reject('installing compiler after absent freeze rejects drift', lambda: host.require_environment(path, cpu, checkpoint))
        with patch.object(host.platform, 'release', return_value='6.6.87.2-microsoft-standard-WSL2'), patch.object(
                host.shutil, 'which', return_value=None), patch.dict(os.environ, {}, clear=True):
            check('WSL kernel detected without interop environment variables', host.placement_observation()['wsl_detected'])
        with patch.object(host.platform, 'release', return_value='generic'), patch.object(host.platform, 'version', return_value='generic'), patch.object(
                host.shutil, 'which', return_value=None), patch.dict(os.environ, {'WSL_DISTRO_NAME': 'Ubuntu'}, clear=True):
            check('WSL environment signal independently detected', host.placement_observation()['wsl_detected'])
        # Exercise whole inspection report flow with metadata seams only;
        # no archive decoding, OS probes, oracle or solver invocation here.
        import pre_collection as pre
        selection = json.loads((ROOT / SPEC['anchor_selection'] / 'selection.json').read_text())
        from contextlib import ExitStack
        with ExitStack() as stack:
            for name, value in dict(verify_repository={'checkpoint': checkpoint}, restore_binary=restored,
                    load_frozen=(manifest, [], {}), verify_selection=(selection, []),
                    resources=report['resources'], capture_environment=wsl, probe_capabilities=capabilities).items():
                stack.enter_context(patch.object(pre, name, return_value=value))
            blocked = check_host(checkpoint, cpu, tmp, ROOT / SPEC['anchor_selection'])
        check('WSL report retains completed infrastructure checks but blocks timing qualification',
              blocked['status'] == 'BLOCKED' and blocked['infrastructure_status'] == 'PASS' and
              blocked['environment']['installed_compiler'] == reported_compiler and blocked['solver_calls'] == 0)
        with patch.object(host.previous, 'effective_cgroups', return_value=([], 1024)):
            reject('original effective-RAM threshold retained', lambda: host.resources(tmp))
        usage = host.shutil.disk_usage(tmp)
        with patch.object(host.shutil, 'disk_usage', return_value=type(usage)(usage.total, usage.total - 1024, 1024)):
            reject('original 20-GiB reserve threshold retained', lambda: host.resources(tmp))
        check('complete collection orchestration is byte-identical to v1 deployment',
              (ROOT / 'experiments/a2_larger_deploy/collect.py').read_bytes() == Path(__file__).with_name('collect.py').read_bytes())
        check('ELF/resource/capability/restore implementations reused unchanged',
              all(getattr(host, k) is getattr(host.previous, k) for k in
                  ('binary_runtime', 'dependency_map', 'resources', 'probe_capabilities', 'restore_binary')))
    after = {p: frozen.file_hash(ROOT / p) for p in sorted(originals)}
    check('all original sources/protocol/build/binary/master/40 archive hashes unchanged', before == after)
    check('master manifest and protocol retain declared identities',
          frozen.file_hash(manifest_path) == SPEC['master_input_manifest_sha256'] and
          frozen.file_hash(frozen.PROTOCOL_PATH) == SPEC['scientific_protocol_sha256'])
    check('all 40 archived input compressed hashes match master records', all(
          frozen.file_hash(host.DEFAULT_INPUT / e['path']) == e['sha256'] for e in manifest['files'] + manifest['rigs']))
    result = dict(schema='a2-larger-deployment-v2-validation', result='PASS',
                  checked_at_utc=datetime.now(timezone.utc).isoformat(),
                  base_checkpoint=SPEC['base_checkpoint'], checks=checks,
                  deployment_sources=host.deployment_identity(), original_artifact_hashes=after,
                  actual_metadata_host=actual_environment['kernel'],
                  actual_metadata_compiler=actual_environment['installed_compiler'],
                  compiler_mismatch_fixture='User-reported SHA/version injected into metadata capture; not a test executed on the user laptop',
                  host_guard_fixtures='Temporary synthetic host records and mocked publication checks; never actual host approval',
                  experimental_solver_calls=0, reference_jobs=0, timing_epochs=0,
                  preflight_reruns=0, stable_collection_host_selected=False)
    frozen.atomic_json(args.output, result)
    print(frozen.canonical({'result': 'PASS', 'checks': len(checks), 'experimental_solver_calls': 0}))


if __name__ == '__main__':
    main()
