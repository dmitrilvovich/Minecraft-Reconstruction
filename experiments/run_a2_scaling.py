#!/usr/bin/env python3
"""Fixed eight-cell replacement collection; atomic, checked, resumable shards."""
import argparse
import csv
import gzip
import hashlib
import itertools
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = 'e73b710a42bca1cd0c7a69b9b9c37db2595a1cb1'
SUITES = ['axis_1', 'axis_2', 'axis_3', 'axis_6', 'oblique_1', 'oblique_2']
MODES = ['search', 'decomposition', 'fixed_hit', 'both']


def digest(path):
    with open(path, 'rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def atomic_json(path, obj):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(obj, indent=2) + '\n')
    tmp.replace(path)


def placements():
    # Choices depend only on grid geometry, before observations or measurements.
    def spread(mask):
        positions = [((v >> 2) & 1, (v >> 1) & 1, v & 1) for v in range(8) if mask & (1 << v)]
        return sum(sum((a-b)**2 for a, b in zip(x, y)) for x, y in itertools.combinations(positions, 2))
    out = [{'k': 0, 'arrangement': 0, 'mask': 0}]
    for k in range(1, 8):
        candidates = [m for m in range(256) if m.bit_count() == k]
        selected = [min(candidates), max(candidates)]
        while len(selected) < 4:
            selected.append(max((m for m in candidates if m not in selected),
                                key=lambda m: (min((m ^ s).bit_count() for s in selected), spread(m), -m)))
        out.extend({'k': k, 'arrangement': a, 'mask': m} for a, m in enumerate(selected))
    return out + [{'k': 8, 'arrangement': 0, 'mask': 255}]


def protocol():
    ps = placements()
    jobs = []
    for i, p in enumerate(ps):
        for resolution in ([8, 16] if p['k'] in (0, 4, 8) else [8]):
            for palette in (['split', 'same'] if i % 2 == 0 else ['same', 'split']):
                jobs.append(dict(p, resolution=resolution, palette=palette,
                                 name=f"r{resolution}_m{p['mask']:03d}_{palette}", controls=False))
    for palette in ['split', 'same']:
        jobs.append(dict(k=8, arrangement=0, mask=255, resolution=8, palette=palette,
                         name='structured_' + palette, controls=True))
    return dict(schema='MCRA2SCALING-RECONSTRUCTED-1', accepted_commit=BASE,
                recovery='No interrupted scaling workspace or raw data accessible from this branch. This is a replacement run, not the historical 410953-case dataset.',
                placements=ps, suites=SUITES, jobs=jobs, repetitions=3,
                node_budget=1_000_000, seconds_budget=60,
                raw_camera_rays={'8': 512, '16': 2048},
                domain_rule='mask bit set: air/bottom/top (7); unset: air/bottom (3)',
                seed='mt19937(20260929 + mask*1009 + resolution*97 + suite_index); palette independent',
                population='Every legal truth, grouped by complete distinct image family for each suite',
                resolution_controls='All six suites at every k=0,4,8 placement, both palettes',
                perturbations='Eight per suite/job: 2 truth-preserving restrictions, 2 arbitrary restrictions, altered observation, contradictory duplicate, empty domain, excluded truth state',
                structured='physical path, physical triangle, two paths, path plus triangle, fixed-hit, fixed-hit plus path; both palettes; all on <=8 cells',
                direct_queries='All 24 literals in every 97th lexicographic camera family (starting with first), and every perturbation/control; once per mode',
                python_spots='First camera family per suite/job, perturbation j=(suite_index+mask)%8, all structured controls',
                weighting={'truth': 'family size / number of legal truths; equal placements and suites within k',
                           'family': 'one vote per distinct image family within each placement/suite; equal placements and suites within k',
                           'occupancy_balanced': 'secondary sensitivity analysis: independent P(occupied)=1/2 at every cell; at incomparable cells P(bottom|occupied)=P(top|occupied)=1/2. Exact family numerator sum(2^(k - occupied_incomparable_cells)), denominator 2^(8+k).'},
                clock='steady_clock; solver APIs only, including budget observer and diagnostic counters; oracle, validation, residual analysis, I/O excluded',
                order='serial jobs; palette order alternates by placement; modes rotate by case/repetition; no concurrent solver jobs',
                unresolved='Budget exhaustion yields UNRESOLVED, never UNSAT; a post-return deadline check also rejects late results. Node counter can include one aborted attempted entry.',
                memory='Whole-process RSS includes the oracle. No solver-only memory claim.')


def validate_raw(prefix, job, spec):
    meta = json.loads(prefix.with_suffix('.json').read_text())
    with prefix.with_suffix('.cases.csv').open() as f:
        cases = [{k: (v if k in ('kind', 'suite') else int(v)) for k, v in r.items()} for r in csv.DictReader(f)]
    assert len(cases) == meta['cases'] and [c['case_id'] for c in cases] == list(range(len(cases)))
    if job['controls']:
        assert len(cases) == 6 and all(c['kind'] == 'structured' for c in cases)
    else:
        for suite in SUITES:
            camera = [c for c in cases if c['suite'] == suite and c['kind'] == 'camera']
            assert sum(c['family_size'] for c in camera) == 3**job['k'] * 2**(8-job['k'])
            assert sum(c['balanced_numerator'] for c in camera) == 2**(8+job['k'])
            assert {c['kind'] for c in cases if c['suite'] == suite and c['kind'] != 'camera'} == {f'perturb_{j}' for j in range(8)}
    rows = witnesses = unresolved = queries = 0
    with prefix.with_suffix('.runs.csv').open() as f:
        for case_id, group in itertools.groupby(csv.DictReader(f), key=lambda r: int(r['case_id'])):
            c = cases[case_id]
            expected = {(m, op, rep, -1, -1) for m in MODES for op in ('feasibility', 'projection') for rep in range(spec['repetitions'])}
            if c['queries_sampled']:
                expected |= {(m, 'query', 0, v, s) for m in MODES for v in range(8) for s in range(3)}
            seen = set()
            deterministic = {}
            for r in group:
                key = (r['mode'], r['operation'], int(r['repetition']), int(r['cell']), int(r['state']))
                assert key in expected and key not in seen
                seen.add(key)
                assert r['status'] in ('SAT', 'UNSAT', 'UNRESOLVED')
                sat = c['family_size'] > 0 if key[1] != 'query' else bool(c['supports'] & (1 << (3*key[3]+key[4])))
                assert r['status'] == 'UNRESOLVED' or (r['status'] == 'SAT') == sat
                if r['status'] != 'UNRESOLVED':
                    metrics = tuple(r[x] for x in r if x not in ('ns', 'repetition'))
                    dkey = (key[0], key[1], key[3], key[4])
                    assert deterministic.setdefault(dkey, metrics) == metrics
                rows += 1
                queries += key[1] == 'query'
                witnesses += int(r['witnesses'])
                unresolved += r['status'] == 'UNRESOLVED'
            assert seen == expected
    assert (rows, queries, witnesses, unresolved) == (meta['runs'], meta['direct_queries'], meta['verified_witnesses'], meta['unresolved'])
    assert rows == len(cases)*4*2*spec['repetitions'] + sum(c['queries_sampled'] for c in cases)*96
    return meta


def archive_file(src, dest):
    temporary = dest.with_suffix(dest.suffix + '.tmp')
    with src.open('rb') as r, temporary.open('wb') as out:
        with gzip.GzipFile(filename='', fileobj=out, mode='wb', mtime=0, compresslevel=6) as z:
            shutil.copyfileobj(r, z)
    with gzip.open(temporary, 'rb') as f:
        recovered = hashlib.file_digest(f, 'sha256').hexdigest()
    assert recovered == digest(src)
    temporary.replace(dest)
    return dict(sha256=digest(dest), uncompressed_sha256=recovered,
                bytes=dest.stat().st_size, uncompressed_bytes=src.stat().st_size)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT/'results/a2-scaling-8cell')
    parser.add_argument('--executable', type=Path, default=ROOT/'build/a2-scaling/mcr_a2_scaling')
    parser.add_argument('--protocol-only', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    spec = protocol()
    path = out/'protocol.json'
    if path.exists():
        assert json.loads(path.read_text()) == spec, 'Protocol changed; refuse to mix runs'
    else:
        atomic_json(path, spec)
    if args.protocol_only:
        print(json.dumps({'placements': spec['placements'], 'jobs': len(spec['jobs'])}))
        return
    source_paths = ['experiments/a2_scaling.cpp', 'experiments/run_a2_scaling.py']
    provenance = dict(executable_sha256=digest(args.executable), protocol_sha256=digest(path),
                      source_sha256={p: digest(ROOT/p) for p in source_paths},
                      platform=platform.platform(), python=platform.python_version(),
                      compiler=subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
                      cpu=next((l.split(':',1)[1].strip() for l in Path('/proc/cpuinfo').read_text().splitlines() if l.startswith('model name')), 'unknown'),
                      accepted_commit=BASE)
    prov_path = out/'provenance.json'
    if prov_path.exists():
        old = json.loads(prov_path.read_text())
        for key in ['executable_sha256', 'protocol_sha256', 'source_sha256']:
            assert old[key] == provenance[key], 'Measurement inputs changed; refusing to resume'
    else:
        atomic_json(prov_path, provenance)
    accepted = json.loads((ROOT/'results/a2-correctness/python-a2-metadata.json').read_text())['fixtures_sha256']
    for palette in ['split', 'same']:
        assert digest(ROOT/f'build/oracle_a2/{palette}_images.bin') == accepted[f'{palette}_images.bin']
    for index, job in enumerate(spec['jobs']):
        done = out/(job['name']+'.json')
        if done.exists():
            saved = json.loads(done.read_text())
            assert saved['job'] == job and saved['provenance_sha256'] == digest(prov_path)
            for filename, checks in saved['files'].items():
                assert digest(out/filename) == checks['sha256']
                with gzip.open(out/filename, 'rb') as f:
                    assert hashlib.file_digest(f, 'sha256').hexdigest() == checks['uncompressed_sha256']
            print('REUSE', job['name'], flush=True)
            continue
        with tempfile.TemporaryDirectory(prefix='mcr-a2-', dir='/dev/shm') as temporary:
            prefix = Path(temporary)/'data'
            cmd = [str(args.executable.resolve()), '--mask', str(job['mask']), '--palette', job['palette'],
                   '--resolution', str(job['resolution']), '--repetitions', str(spec['repetitions']),
                   '--nodes', str(spec['node_budget']), '--seconds', str(spec['seconds_budget']), '--output', str(prefix)]
            if job['controls']:
                cmd += ['--controls']
            elif job['resolution'] == 8:
                cmd += ['--images', str(ROOT/f"build/oracle_a2/{job['palette']}_images.bin")]
            start = time.time()
            completed = subprocess.run(cmd, text=True, capture_output=True)
            (out/(job['name']+'.log')).write_text(completed.stdout+completed.stderr)
            completed.check_returncode()
            meta = validate_raw(prefix, job, spec)
            files = {}
            for suffix in ['cases.csv', 'runs.csv', 'spots.jsonl']:
                filename = job['name']+'.'+suffix+'.gz'
                files[filename] = archive_file(prefix.with_suffix('.'+suffix), out/filename)
            atomic_json(done, dict(job=job, counts=meta, files=files, elapsed_seconds=time.time()-start,
                                   provenance_sha256=digest(prov_path)))
            print(f"PASS {index+1}/{len(spec['jobs'])} {job['name']} {meta}", flush=True)
    saved = [json.loads((out/(j['name']+'.json')).read_text()) for j in spec['jobs']]
    totals = {key: sum(s['counts'][key] for s in saved) for key in ['cases','runs','direct_queries','verified_witnesses','unresolved']}
    atomic_json(out/'collection.json', dict(status='COMPLETE', jobs=len(saved), totals=totals,
                                          protocol_sha256=digest(path), provenance_sha256=digest(prov_path)))
    print(json.dumps(totals), flush=True)


if __name__ == '__main__':
    main()
