"""Versioned, read-only selection of whole frozen jobs; never generates cases."""
from __future__ import annotations
import argparse
from collections import Counter
import gzip
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'experiments/a2_larger'))
import common as frozen
import run as legacy

MASTER_SHA256 = '1cf1881337a649de17ab244fb70dd8f5fc421467e1e8a4efb433412d30e0378b'
AMENDMENT = ROOT / 'experiments/protocols/a2-scaling-larger-launch-amendment-1.json'
DEFAULT_INPUT = ROOT / 'results/a2-scaling-larger-v1/inputs'


def implementation_identity():
    return {p.relative_to(ROOT).as_posix(): frozen.file_hash(p)
            for p in sorted(Path(__file__).parent.glob('*.py'))}


def stage_order():
    # Preserve v1's explicit anchors-first rule, including in scoped launches.
    return [('anchors', 8)] + [('volume', n) for n in
                             sorted({g['n'] for g in frozen.PROTOCOL['geometries']})]


def case_stage(case):
    return ('anchors' if case['category'] == 'anchor' else 'volume', case['n'])


def load_frozen(input_root):
    """Verify identities and exact archived bytes; no rendering or oracle work."""
    input_root = Path(input_root)
    if frozen.file_hash(input_root / 'input-manifest.json') != MASTER_SHA256:
        raise ValueError('Master manifest differs from published freeze')
    manifest = json.loads((input_root / 'input-manifest.json').read_text())
    if not manifest['complete'] or manifest['design_slots'] != 72148:
        raise ValueError('Incomplete master input manifest')
    if manifest['source_files'] != frozen.source_identity():
        raise ValueError('Original collector/reference source changed')
    if any(manifest[k] != v for k, v in frozen.protocol_identity().items()):
        raise ValueError('Original protocol or solver binary changed')
    cases = []; locations = {}; inventory = {}; seen_case_ids = set()
    for entry in manifest['rigs'] + manifest['files']:
        frozen.verify_archive(input_root / entry['path'],
                              {k: v for k, v in entry.items() if k != 'path'})
    for entry in manifest['files']:
        ordinal = 0
        with gzip.open(input_root / entry['path'], 'rb') as stream:
            for line in stream:
                case = json.loads(line)
                if '_archive' in case:
                    continue
                rid = case['record_id']; cid = frozen.canonical(case['case_id'])
                if rid in locations or cid in seen_case_ids:
                    raise ValueError('Duplicate frozen case identity')
                if case_stage(case) not in stage_order():
                    raise ValueError('Unknown protocol stage')
                locations[rid] = dict(record_id=rid, case_id=case['case_id'],
                                      n=case['n'], job_id=case['job_id'],
                                      archive=entry['path'], ordinal=ordinal,
                                      raw_record_sha256=hashlib.sha256(line).hexdigest())
                ordinal += 1; seen_case_ids.add(cid); cases.append(case)
                key = frozen.canonical(case['job_id'])
                inventory.setdefault(key, []).append(case)
    if len(cases) != manifest['design_slots']:
        raise ValueError('Master population mismatch')
    declared = {frozen.canonical(j['job_id']): j for j in manifest['jobs']}
    if set(declared) != set(inventory):
        raise ValueError('Master job inventory mismatch')
    for key, cs in inventory.items():
        j = declared[key]
        if (len(cs) != j['case_slots'] or {c['n'] for c in cs} != {j['n']}
                or len({case_stage(c) for c in cs}) != 1
                or sum(4 * (2 + len(c['queries'])) for c in cs) != j['calls_per_epoch']):
            raise ValueError('Frozen job membership/count mismatch')
    return manifest, cases, locations


def make_selection(manifest, cases, locations, volumes=None, stages=None):
    if (volumes is None) == (stages is None):
        raise ValueError('Choose either declared volumes or complete stages')
    if volumes is not None:
        ns = set(volumes)
        declared_ns = {n for _, n in stage_order()}
        if not ns or not ns <= declared_ns:
            raise ValueError('Unknown or empty volume selection')
        selected_stages = [s for s in stage_order() if s[1] in ns]
    else:
        requested = {tuple(s) for s in stages}
        if not requested or not requested <= set(stage_order()):
            raise ValueError('Unknown or empty stage selection')
        selected_stages = [s for s in stage_order() if s in requested]
    selected = [c for c in cases if case_stage(c) in selected_stages]
    job_ids = {frozen.canonical(c['job_id']) for c in selected}
    jobs = [j for j in manifest['jobs'] if frozen.canonical(j['job_id']) in job_ids]
    if sum(j['case_slots'] for j in jobs) != len(selected):
        raise ValueError('Partial jobs are forbidden')
    entries = [locations[c['record_id']] for c in selected]
    needed = {e['archive'] for e in entries} | {c['rig'] for c in selected if 'rig' in c}
    descriptor = dict(schema='a2-larger-selection-v1',
                      master_manifest_sha256=MASTER_SHA256,
                      protocol_sha256=manifest['protocol_sha256'],
                      amendment_sha256=frozen.file_hash(AMENDMENT),
                      implementation_files=implementation_identity(),
                      stages=[list(s) for s in selected_stages],
                      volumes=sorted({c['n'] for c in selected}),
                      design_slots=len(selected),
                      slots_by_volume=dict(sorted(Counter(str(c['n']) for c in selected).items())),
                      categories=dict(sorted(Counter(c['category'] for c in selected).items())),
                      jobs=jobs, epochs=frozen.PROTOCOL['execution']['epochs'],
                      timed_calls=sum(j['calls_per_epoch'] for j in jobs) * frozen.PROTOCOL['execution']['epochs'],
                      required_archives=[e for e in manifest['files'] + manifest['rigs'] if e['path'] in needed],
                      index_semantics='Frozen archive order, then zero-based data-record ordinal; timing order is unchanged v1 call_plan.',
                      ids_sha256=frozen.digest([e['record_id'] for e in entries]),
                      collection_authorized=False)
    return descriptor, entries, selected


def publish_selection(directory, descriptor, entries):
    directory = Path(directory); directory.mkdir(parents=True, exist_ok=True)
    context = dict(kind='selection_index_only', master_manifest_sha256=MASTER_SHA256,
                   selection_descriptor_sha256=frozen.digest(descriptor))
    index = frozen.write_archive(directory / 'selected-design-ids.jsonl.gz', iter(entries), context, len(entries))
    document = {**descriptor, 'selected_index': index}
    path = directory / 'selection.json'
    if path.exists() and json.loads(path.read_text()) != document:
        raise ValueError('Refusing to overwrite a different frozen selection')
    frozen.atomic_json(path, document)
    return document


def verify_selection(directory, manifest, cases, locations):
    directory = Path(directory); document = json.loads((directory / 'selection.json').read_text())
    desc, entries, selected = make_selection(manifest, cases, locations, stages=document['stages'])
    index = document.get('selected_index')
    if document != {**desc, 'selected_index': index}:
        raise ValueError('Selection changed or is not an exact complete-stage subset')
    context = dict(kind='selection_index_only', master_manifest_sha256=MASTER_SHA256,
                   selection_descriptor_sha256=frozen.digest(desc))
    frozen.verify_archive(directory / 'selected-design-ids.jsonl.gz', index, context)
    if list(frozen.read_records(directory / 'selected-design-ids.jsonl.gz')) != entries:
        raise ValueError('Selected IDs no longer resolve to the frozen input records')
    return document, selected


class Scope:
    """A second boundary checked before *every* G/F/P/Q worker dispatch."""
    def __init__(self, selection, cases):
        self.selection = selection
        self.cases = {c['record_id']: frozen.digest(c) for c in cases}
        self.jobs = {frozen.canonical(j['job_id']) for j in selection['jobs']}
        self.volumes = set(selection['volumes'])

    def check_case(self, case):
        if (case['n'] not in self.volumes or case['record_id'] not in self.cases
                or frozen.canonical(case['job_id']) not in self.jobs
                or frozen.digest(case) != self.cases[case['record_id']]):
            raise ValueError('Case outside frozen batch scope or changed input')

    def check_record(self, record):
        if (record['case_record_id'] not in self.cases
                or frozen.canonical(record['job_id']) not in self.jobs):
            raise ValueError('Resumed record outside frozen batch scope')


def schedule(selection, selected):
    jobs = {}
    for c in selected:
        jobs.setdefault(frozen.canonical(c['job_id']), []).append(c)
    for raw_stage in selection['stages']:
        stage = tuple(raw_stage)
        keys = [k for k, cs in jobs.items() if case_stage(cs[0]) == stage]
        for epoch in range(selection['epochs']):
            for key in sorted(keys, key=lambda k: (frozen.H('job_order', epoch, json.loads(k)), k)):
                yield stage, epoch, key, jobs[key]


def dry_run(selection, selected):
    # Enumerates IDs, not API calls. Worker and ReferenceBank are never constructed.
    scope = Scope(selection, selected); rows = []; total = 0
    for stage, epoch, key, cs in schedule(selection, selected):
        h = hashlib.sha256(); count = 0
        for c in cs:
            scope.check_case(c)
        for c, mode, op, query in legacy.call_plan(cs, epoch):
            if c['record_id'] not in scope.cases:
                raise ValueError('Dry-run crossed batch boundary')
            h.update((frozen.canonical(legacy.call_id(c, mode, op, query, epoch)) + '\n').encode('ascii'))
            count += 1
        rows.append(dict(stage=list(stage), epoch=epoch, job_id=json.loads(key),
                         calls=count, ordered_call_ids_sha256=h.hexdigest()))
        total += count
    if total != selection['timed_calls']:
        raise ValueError('Dry-run call inventory mismatch')
    return dict(kind='nonexecuting_dispatch_inventory', solver_calls=0, reference_jobs=0,
                timing_epochs_executed=0, planned_calls=total,
                design_slots=selection['design_slots'], jobs=rows,
                collection_authorized=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=DEFAULT_INPUT)
    parser.add_argument('--output', type=Path, required=True)
    choose = parser.add_mutually_exclusive_group(required=True)
    choose.add_argument('--volumes', type=int, nargs='+')
    choose.add_argument('--stages', nargs='+', help='anchors:8 or volume:N; complete stages only')
    args = parser.parse_args()
    stages = None if args.stages is None else [(s.split(':')[0], int(s.split(':')[1])) for s in args.stages]
    manifest, cases, locations = load_frozen(args.input)
    desc, entries, selected = make_selection(manifest, cases, locations, args.volumes, stages)
    document = publish_selection(args.output, desc, entries)
    frozen.atomic_json(args.output / 'dry-run.json', dry_run(document, selected))
    print(frozen.canonical({'design_slots': document['design_slots'], 'planned_calls': document['timed_calls'], 'solver_calls': 0}))


if __name__ == '__main__':
    main()
