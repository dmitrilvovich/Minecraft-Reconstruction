#!/usr/bin/env python3
"""Targeted checks of the collected inputs using the unchanged frozen solver."""
import argparse
import gzip
import json
from pathlib import Path
import sys
import time
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT/'reference/python'))
import phase_a as oracle


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('collection', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    spec = json.loads((args.collection/'protocol.json').read_text())
    worlds = np.array([[(i // (3**v)) % 3 for v in range(8)] for i in range(6561)], dtype=np.uint8)
    totals = dict(cases=0, direct_queries=0, witnesses=0, audited_prunes=0, empty_domain_guards=0)
    start = time.perf_counter()
    for job in spec['jobs']:
        with gzip.open(args.collection/(job['name']+'.spots.jsonl.gz'), 'rt') as f:
            for line in f:
                case = json.loads(line)
                factors = [oracle.RayFactor(tuple(s[0] for s in r['steps']),
                    tuple((0, 2 if s[1] else 0, (2 if job['palette']=='same' else 1) if s[2] else 0) for s in r['steps']))
                    for r in case['factors']]
                labels = [r['target'] for r in case['factors']]
                keep = np.ones(6561, dtype=bool)
                for r, label in zip(factors, labels):
                    keep &= oracle.chain_render(r, worlds) == label
                for v, mask in enumerate(case['domains']):
                    keep &= ((1 << worlds[:, v]) & mask) != 0
                family = worlds[keep]
                assert len(family) == case['family_size']
                supported = oracle.oracle_support(family) if len(family) else [0]*8
                assert supported == case['supported']
                audit = oracle.Audit(family)
                stats = oracle.Stats()

                def solve(domains):
                    # Accepted adapter guard for the frozen out-of-ray empty-domain limitation.
                    if not all(domains):
                        totals['empty_domain_guards'] += 1
                        return None
                    return oracle.solve(domains, factors, labels, stats, audit)

                def witness(w, domains):
                    assert any(np.array_equal(w, row) for row in family)
                    assert all(m & (1 << int(s)) for m, s in zip(domains, w))
                    totals['witnesses'] += 1

                w = solve(case['domains'])
                assert (w is not None) == bool(len(family))
                if w is not None:
                    witness(w, case['domains'])
                for v in range(8):
                    for s in range(3):
                        domains = case['domains'].copy()
                        domains[v] &= 1 << s
                        w = solve(domains)
                        assert (w is not None) == bool(supported[v] & (1 << s))
                        if w is not None:
                            witness(w, domains)
                            assert w[v] == s
                        totals['direct_queries'] += 1
                totals['cases'] += 1
                totals['audited_prunes'] += audit.steps
        print(job['name'], totals['cases'], flush=True)
    args.output.write_text(json.dumps(dict(status='PASS', counts=totals, seconds=time.perf_counter()-start,
        scope='Targeted collected camera, perturbation, and structured cases; no full acceptance rerun'), indent=2)+'\n')


if __name__ == '__main__':
    main()
