"""Nonexperimental selection/guard checks. Never creates a real worker or oracle."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile
from unittest.mock import patch

from selection import (DEFAULT_INPUT, Scope, case_stage, dry_run, frozen,
                       legacy, load_frozen, make_selection, publish_selection,
                       schedule, verify_selection)
from guards import (ScopedWorker, bind_batch, check_prerequisites, expected_context,
                    require_environment, verify_saved_job)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--input', type=Path, default=DEFAULT_INPUT)
    args = parser.parse_args(); evidence = {}; checks = []

    def check(name, condition):
        if not condition:
            raise AssertionError(name)
        checks.append({'check': name, 'result': 'PASS'})

    def rejected(name, action, contains=None):
        try:
            action()
        except (ValueError, RuntimeError) as exc:
            if contains and contains not in str(exc):
                raise AssertionError((name, str(exc))) from exc
            checks.append({'check': name, 'result': 'PASS', 'rejection': str(exc)})
        else:
            raise AssertionError('Missing rejection: ' + name)

    def forbidden(*args, **kwargs):
        raise AssertionError('Validation attempted to create a real worker')

    with patch.object(legacy, 'Worker', forbidden):
        manifest, cases, locations = load_frozen(args.input)
        desc, entries, selected = make_selection(manifest, cases, locations, volumes=[4, 6])
        check('exact volume counts', desc['slots_by_volume'] == {'4': 3888, '6': 12384})
        check('exact selection and call inventory', desc['design_slots'] == 16272 and desc['timed_calls'] == 737280)
        check('exact set predicate, not just count', {c['record_id'] for c in selected} ==
              {c['record_id'] for c in cases if c['n'] in (4, 6)})
        check('no N>=8 or anchors in selection', all(c['n'] in (4, 6) and c['category'] != 'anchor' for c in selected))
        check('39 complete jobs, 11 source archives', len(desc['jobs']) == 39 and len(desc['required_archives']) == 11)
        check('N=6 owns 144 structured slots; N=4 none',
              sum(c['category'] == 'structured' for c in selected if c['n'] == 6) == 144 and
              not any(c['category'] == 'structured' for c in selected if c['n'] == 4))
        anchor_desc, _, anchors = make_selection(manifest, cases, locations, stages=[('anchors', 8)])
        check('all four anchors remain N=8', anchor_desc['design_slots'] == 4 and all(c['n'] == 8 for c in anchors))
        check('index references original row bytes and IDs', all(
            locations[c['record_id']] == e and e['case_id'] == c['case_id'] for c, e in zip(selected, entries)))
        document = publish_selection(args.output, desc, entries)
        verified, selected_again = verify_selection(args.output, manifest, cases, locations)
        check('selection archive and frozen records round-trip', verified == document and selected_again == selected)
        dry = dry_run(document, selected)
        check('all five planned epochs and 195 jobs, zero execution',
              len(dry['jobs']) == 195 and {j['epoch'] for j in dry['jobs']} == set(range(5)) and dry['solver_calls'] == 0)
        inventories = {frozen.canonical(j['job_id']): j for j in manifest['jobs']}
        all_jobs = {}
        for c in cases:
            all_jobs.setdefault(frozen.canonical(c['job_id']), []).append(c)
        # A selected job contains the original complete record list. Thus the
        # unchanged legacy call_plan receives identical j indices in each epoch.
        check('whole-job case ordering and query obligations unchanged', all(
            cs == all_jobs[key] for _, _, key, cs in schedule(document, selected)))
        check('all planned job call counts equal original manifest', all(
            j['calls'] == inventories[frozen.canonical(j['job_id'])]['calls_per_epoch'] for j in dry['jobs']))
        check('job order is exact original-stage restriction', all(
            [j['job_id'] for j in dry['jobs'] if j['stage'] == stage and j['epoch'] == epoch] ==
            [json.loads(k) for k in sorted(
                [k for k, cs in all_jobs.items() if list(case_stage(cs[0])) == stage],
                key=lambda k: (frozen.H('job_order', epoch, json.loads(k)), k))]
            for stage in document['stages'] for epoch in range(5)))
        frozen.atomic_json(args.output / 'dry-run.json', dry)
        scope = Scope(document, selected)
        foreign = next(c for c in cases if c['n'] == 8)
        for n in (8, 10, 12):
            rejected('dispatch rejects N=' + str(n), lambda n=n: scope.check_case(next(c for c in cases if c['n'] == n)))
        changed = copy.deepcopy(selected[0]); changed['domains'][0] ^= 1
        rejected('dispatch rejects mutated selected input', lambda: scope.check_case(changed))
        changed_n = copy.deepcopy(foreign); changed_n['n'] = 4
        rejected('forged volume cannot admit foreign ID', lambda: scope.check_case(changed_n))
        rejected('resume rejects foreign case/job', lambda: scope.check_record(
            {'case_record_id': foreign['record_id'], 'job_id': foreign['job_id']}))
        class NoOpWorker:
            calls = 0
            def __init__(self, *a): pass
            def call(self, *a): self.calls += 1; return {'infrastructure_only': True}
            def close(self): pass
        with patch.object(legacy, 'Worker', NoOpWorker):
            worker = ScopedWorker('unused', 0, scope)
            for op in ('G', 'F', 'P', 'Q'):
                rejected('worker boundary rejects out-of-batch ' + op,
                         lambda op=op: worker.call(foreign, [], 0, op))
            check('foreign dispatch never reaches no-op transport', worker.worker.calls == 0)
            worker.close()
        with tempfile.TemporaryDirectory(prefix='a2-launch-validation-') as tmp:
            tmp = Path(tmp)
            rejected('missing anchors cannot be executed by N4/6 launch',
                     lambda: check_prerequisites(tmp, {}, document, cases), 'Prior protocol stages incomplete')
            env = tmp / 'environment.json'; frozen.atomic_json(env, {'status': 'unselected'})
            rejected('unselected host blocks before worker', lambda: require_environment(env, 0, '0' * 40), 'not selected/frozen')
            batch = bind_batch(tmp, document)
            check('same selection resumes same batch namespace', bind_batch(tmp, document) == batch)
            altered = {**document, 'volumes': [4, 6, 8]}
            frozen.atomic_json(batch / 'selection.json', altered)
            rejected('changed stored resume scope rejected', lambda: bind_batch(tmp, document))
            # Small synthetic records live only in this temporary directory, use
            # nonexperimental IDs, and contain no real measurements or witnesses.
            synthetic = [dict(record_id='infrastructure-' + p, case_id=['infrastructure', p],
                              slot_id=['infrastructure'], palette=p, n=4,
                              category='infrastructure', job_id=['infrastructure'], queries=[])
                         for p in frozen.PALETTES]
            context = expected_context({'kind': 'infrastructure-only'}, ['infrastructure'], synthetic, 0)
            rows = [dict(record_id=legacy.call_id(c, m, op, q, 0), case_record_id=c['record_id'],
                         job_id=c['job_id'], epoch=0, mode=frozen.MODES[m], operation=op, query=q)
                    for c, m, op, q in legacy.call_plan(synthetic, 0)]
            archive = tmp / 'synthetic.jsonl.gz'
            frozen.write_archive(archive, iter(rows), context, len(rows))
            verify_saved_job(archive, {'kind': 'infrastructure-only'}, synthetic, 0)
            check('synthetic resume validates archive and exact dispatch metadata', True)
            def never_rows():
                raise AssertionError('Resume attempted to rerun rows')
                yield
            before = archive.read_bytes()
            frozen.write_archive(archive, never_rows(), context, len(rows))
            check('sealed resume does not rerun even a synthetic row', archive.read_bytes() == before)
            rejected('foreign resume context rejected', lambda: verify_saved_job(archive, {'kind': 'foreign'}, synthetic, 0))
            Path(str(archive) + '.complete.json').unlink()
            frozen.write_archive(archive, never_rows(), context, len(rows))
            check('sealed orphan recovers marker without rerun', archive.read_bytes() == before)
            with archive.open('ab') as out: out.write(b'corrupt')
            try:
                frozen.verify_archive(archive)
            except (ValueError, OSError, EOFError):
                checks.append({'check': 'corrupt archive rejected', 'result': 'PASS'})
            else: raise AssertionError('Corruption not detected')
            partial = tmp / 'interrupted.jsonl.gz'
            Path(str(partial) + '.partial.jsonl').write_text('infrastructure only\n')
            rejected('unsealed partial blocks automatic rerun',
                     lambda: frozen.write_archive(partial, never_rows(), context, len(rows)), 'Interrupted artifact preserved')
        # Lightweight post-validation byte hashes (no second archive/manifest generation).
        check('all 40 input archive bytes unchanged after validation', all(
            frozen.file_hash(args.input / e['path']) == e['sha256'] for e in manifest['files'] + manifest['rigs']))
        check('master manifest unchanged', frozen.file_hash(args.input / 'input-manifest.json') == document['master_manifest_sha256'])
        check('solver, protocol and original source unchanged',
              all(manifest[k] == v for k, v in frozen.protocol_identity().items()) and
              manifest['source_files'] == frozen.source_identity())
    evidence.update(schema='a2-larger-launch-validation-v1', result='PASS', checks=checks,
                    selection_sha256=frozen.file_hash(args.output / 'selection.json'),
                    design_slots=16272, planned_timed_calls=737280,
                    solver_calls=0, reference_jobs=0, experimental_cases_executed=0,
                    timing_epochs_executed=0, infrastructure_no_op_solver_calls=0,
                    collection_ready=False,
                    blockers=['stable collection environment not selected/frozen',
                              'anchors-first N=8 prerequisite conflicts with requested first N=4/6-only batch'])
    frozen.atomic_json(args.output / 'validation.json', evidence)
    print(frozen.canonical({'result': 'PASS', 'checks': len(checks), 'solver_calls': 0,
                            'design_slots': 16272, 'planned_timed_calls': 737280, 'collection_ready': False}))


if __name__ == '__main__':
    main()
