"""Nonexperimental anchor/deployment validation; never construct the real worker."""
from __future__ import annotations
import argparse
import ast
import copy
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

import host
from host import ROOT, SPEC, frozen
from selection import DEFAULT_INPUT, Scope, dry_run, legacy, load_frozen, verify_selection
from guards import ScopedWorker, bind_batch, check_prerequisites, expected_context, verify_saved_job
from pre_collection import prepare_host_record, write_new


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cpu', type=int, default=0)
    args = parser.parse_args(); checks = []
    if args.output.exists(): raise ValueError('Do not overwrite validation evidence')

    def check(name, condition):
        if not condition: raise AssertionError(name)
        checks.append({'check': name, 'result': 'PASS'})

    def reject(name, action):
        try: action()
        except (ValueError, RuntimeError, OSError):
            checks.append({'check': name, 'result': 'PASS'})
        else: raise AssertionError('Missing rejection: ' + name)

    def forbidden(*args, **kwargs):
        raise AssertionError('A real experimental worker was requested')

    with patch.object(legacy, 'Worker', forbidden):
        manifest, cases, locations = load_frozen(DEFAULT_INPUT)
        directory = ROOT / SPEC['anchor_selection']
        selection, anchors = verify_selection(directory, manifest, cases, locations)
        check('exact master anchor ID set', {c['record_id'] for c in anchors} ==
              {c['record_id'] for c in cases if c['category'] == 'anchor'})
        check('four N=8 anchors and zero non-anchors', len(anchors) == 4 and all(
            c['category'] == 'anchor' and c['n'] == 8 and c['job_id'] == ['anchor', 8] for c in anchors))
        check('two fixtures paired across both palettes', {frozen.canonical(c['case_id']) for c in anchors} ==
              {frozen.canonical([['anchor', fixture], palette]) for fixture in ('path', 'triangle') for palette in frozen.PALETTES})
        check('original anchor archive only; no regenerated case records', len(selection['required_archives']) == 1 and
              selection['required_archives'][0]['path'] == 'cases/anchors.jsonl.gz')
        dry = dry_run(selection, anchors)
        check('dry run matches published ordered-call hashes', dry == json.loads((directory / 'dry-run.json').read_text()))
        check('five planned epochs, 640 planned calls, zero execution', len(dry['jobs']) == 5 and
              dry['planned_calls'] == 640 and dry['solver_calls'] == 0 and
              all(j['job_id'] == ['anchor', 8] and j['calls'] == 128 for j in dry['jobs']))
        scope = Scope(selection, anchors)
        for category, n in (('camera', 4), ('camera', 6), ('camera', 8), ('structured', 8), ('camera', 10), ('camera', 12)):
            c = next(c for c in cases if c['category'] == category and c['n'] == n)
            reject('anchor scope rejects ' + category + ' N=' + str(n), lambda c=c: scope.check_case(c))
        altered = copy.deepcopy(anchors[0]); altered['domains'][0] ^= 1
        reject('altered anchor bytes rejected', lambda: scope.check_case(altered))
        foreign = next(c for c in cases if c['category'] == 'camera' and c['n'] == 8)
        reject('resume rejects non-anchor N=8 record', lambda: scope.check_record(
            {'case_record_id': foreign['record_id'], 'job_id': foreign['job_id']}))
        class NoOp:
            calls = 0
            def __init__(self, *a): pass
            def call(self, *a): self.calls += 1
            def close(self): pass
        with patch.object(legacy, 'Worker', NoOp):
            worker = ScopedWorker('unused', args.cpu, scope)
            for op in ('G', 'F', 'P', 'Q'):
                reject('no-op dispatch guard rejects non-anchor ' + op, lambda op=op: worker.call(foreign, [], 0, op))
            check('zero no-op transport calls', worker.worker.calls == 0)
        with tempfile.TemporaryDirectory(prefix='a2-portable-validation-') as tmp:
            tmp = Path(tmp)
            check('anchors have no earlier protocol prerequisites', check_prerequisites(tmp, {}, selection, cases) == [('anchors', 8)])
            n46 = json.loads((ROOT / 'results/a2-scaling-larger-launch-amendment-1/n04-n06/selection.json').read_text())
            reject('N4/6 still blocked before anchors', lambda: check_prerequisites(tmp, {}, n46, cases))
            batch = bind_batch(tmp, selection)
            check('anchor resume namespace stable', bind_batch(tmp, selection) == batch)
            frozen.atomic_json(batch / 'selection.json', {**selection, 'stages': [['volume', 8]]})
            reject('modified resume scope rejected', lambda: bind_batch(tmp, selection))
            with patch.object(frozen, 'BINARY', tmp / 'worker'):
                first = host.restore_binary(); before = frozen.BINARY.stat().st_mtime_ns
                second = host.restore_binary()
                check('bootstrap restores exact ELF and is idempotent', first['restored'] and not second['restored'] and
                      first['sha256'] == SPEC['binary_sha256'] and frozen.BINARY.stat().st_mtime_ns == before)
                frozen.BINARY.write_bytes(b'infrastructure-corrupt')
                reject('bootstrap refuses to overwrite mismatching binary', host.restore_binary)
                check('invalid existing binary preserved', frozen.BINARY.read_bytes() == b'infrastructure-corrupt')
            synthetic = [dict(record_id='infrastructure-' + p, case_id=['infrastructure', p],
                              slot_id=['infrastructure'], palette=p, n=8, category='infrastructure',
                              job_id=['infrastructure'], queries=[]) for p in frozen.PALETTES]
            context = expected_context({'kind': 'infrastructure-only'}, ['infrastructure'], synthetic, 0)
            rows = [dict(record_id=legacy.call_id(c, m, op, q, 0), case_record_id=c['record_id'],
                         job_id=c['job_id'], epoch=0, mode=frozen.MODES[m], operation=op, query=q)
                    for c, m, op, q in legacy.call_plan(synthetic, 0)]
            archive = tmp / 'synthetic.jsonl.gz'
            frozen.write_archive(archive, iter(rows), context, len(rows))
            verify_saved_job(archive, {'kind': 'infrastructure-only'}, synthetic, 0)
            def no_rows():
                raise AssertionError('Resume attempted a synthetic rerun')
                yield
            frozen.write_archive(archive, no_rows(), context, len(rows))
            Path(str(archive) + '.complete.json').unlink()
            frozen.write_archive(archive, no_rows(), context, len(rows))
            check('sealed and orphan resume do not execute rows', True)
            reject('resumed archive cannot change identity', lambda: verify_saved_job(archive, {'kind': 'different'}, synthetic, 0))
            initial_environment = host.capture_environment(args.cpu, tmp)
            record = {'schema': 'a2-larger-stable-collection-host-v1', 'status': 'frozen_for_collection',
                      'stable_execution_strategy': 'reserved_physical_host', 'stable_asset_identity': 'synthetic-not-a-reservation',
                      'reservation_evidence': 'infrastructure validation only; never published as a collection identity',
                      'study_output_root': str(tmp), 'environment': initial_environment,
                      'capability_checks': {'watchdog_process_kill_reap': 'PASS'}}
            path = tmp / 'synthetic-host.json'; frozen.atomic_json(path, record)
            sample_report = {'status': 'PASS', 'checked_at_utc': 'synthetic-not-a-real-freeze',
                             'repository': {'checkpoint': '0' * 40}, 'resources': {'study_output_root': str(tmp)},
                             'environment': initial_environment, 'capabilities': record['capability_checks'],
                             'selection': {'selection_sha256': 'synthetic'}}
            prepared = prepare_host_record(sample_report, 'reserved_physical_host', 'synthetic-asset', 'test only')
            check('prepared record requires publication and does not authorize execution',
                  prepared['publication_required_before_execution'] and not prepared['experimental_execution_authorized'] and
                  prepared['environment'] == initial_environment and prepared['solver_calls'] == 0)
            reject('host freeze cannot omit reservation identity', lambda: prepare_host_record(sample_report, 'reserved_physical_host', '', ''))
            prepared_path = tmp / 'prepared-synthetic-host.json'; write_new(prepared_path, prepared)
            reject('existing prepared host record cannot be replaced', lambda: write_new(prepared_path, prepared))
            # These publication/env seams are mocked ONLY in this synthetic test.
            # No entry point exposes a bypass, and no worker is constructed.
            checkpoint = '0' * 40
            with patch.object(host, 'require_committed', lambda *a: None), patch.object(host, 'command', lambda *a: checkpoint):
                with patch.object(host, 'capture_environment', lambda *a: initial_environment):
                    check('identical stable identity accepted by comparison', host.require_environment(path, args.cpu, checkpoint)[0] == initial_environment)
                for key in ('machine_identifier_hashes', 'topology', 'kernel', 'binary_runtime', 'compiler_sha256', 'deployment_sources'):
                    changed = copy.deepcopy(initial_environment); changed[key] = {'changed': True}
                    with patch.object(host, 'capture_environment', lambda *a, changed=changed: changed):
                        reject('changed ' + key + ' stops before worker', lambda: host.require_environment(path, args.cpu, checkpoint))
            with patch.object(host, 'effective_cgroups', lambda: ([], 1024)):
                reject('insufficient effective RAM blocks launch', lambda: host.resources(tmp))
            usage = host.shutil.disk_usage(tmp)
            with patch.object(host.shutil, 'disk_usage', lambda p: type(usage)(usage.total, usage.total - 1024, 1024)):
                reject('less than 20 GiB disk blocks launch', lambda: host.resources(tmp))
        old = ast.parse((ROOT / 'experiments/a2_larger_launch/collect.py').read_text())
        new = ast.parse((ROOT / 'experiments/a2_larger_deploy/collect.py').read_text())
        def stage_loop(tree):
            func = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'collect')
            return ast.dump(next(n for n in func.body if isinstance(n, ast.For) and isinstance(n.target, ast.Tuple)), include_attributes=False)
        check('experimental job/timing/reference/stop body unchanged', stage_loop(old) == stage_loop(new))
        check('all original input archive compressed hashes unchanged', all(
            frozen.file_hash(DEFAULT_INPUT / e['path']) == e['sha256'] for e in manifest['files'] + manifest['rigs']))
        check('original source/protocol/binary identities unchanged', manifest['source_files'] == frozen.source_identity() and
              all(manifest[k] == v for k, v in frozen.protocol_identity().items()))
    report = dict(schema='a2-larger-anchor-deployment-validation-v1', result='PASS', checks=checks,
                  design_slots=4, planned_timed_calls=640, solver_calls=0, reference_jobs=0,
                  timing_epochs_executed=0, no_op_transport_calls=0,
                  synthetic_records='Temporary infrastructure-only IDs, never experimental evidence',
                  stable_collection_host_selected=False)
    frozen.atomic_json(args.output, report)
    print(frozen.canonical({'result': 'PASS', 'checks': len(checks), 'anchor_design_ids': 4, 'solver_calls': 0}))


if __name__ == '__main__':
    main()
