# N=4/6 collection launch — blocked

**No experimental case, root diagnostic, reference audit or timing epoch ran.**
This launch attempt used the frozen checkpoint at
`f8f055a8491828154d4654ef6f9a1caa611bef51`. The detailed
[launch record](launch-attempt-01.json) preserves identities, resource readings,
input archive hashes and unattempted coverage.

## Required launch stop

The frozen timing host is AMD EPYC 9V74, node `ad6b63c043ba`.
The available host is Intel Xeon Platinum 8573C, node `8984c830f591`.
This is a material host mismatch. Collection stopped before any solver execution;
the host identity was not changed or waived.

The binary, collector/reference source, compiler/build identity, protocol and
input-manifest hashes match the freeze. CPU-0 affinity was set successfully and
restored. Free disk was 28.44 GiB, satisfying the 20 GiB reserve; the enclosing
memory limit was 8 GiB with approximately 6.35 GiB of headroom. These are launch
resource readings, not inference-worker measurements.

The local mirror commit differs from the API-published commit, but its complete
clean tree is identical to the published checkpoint. Both commit IDs and the
shared tree hash are recorded. No checkout or history was modified.

## Pending batch, counted from frozen inputs

| Volume | Design slots | Calls per epoch | Planned calls over five epochs |
| --- | ---: | ---: | ---: |
| N=4 | 3,888 | 36,288 | 181,440 |
| N=6 | 12,384 | 111,168 | 555,840 |
| Total | 16,272 | 147,456 | 737,280 |

All 11 required input archives passed compressed/uncompressed size/hash and
completion-record checks. Input records agree with the frozen job inventory.
No inputs were generated, altered or selected using solver performance.

Completed epochs: 0/5. Attempted/completed timed calls: 0. SAT/UNSAT/UNRESOLVED
counts are all zero because nothing was attempted; unattempted work is not
UNRESOLVED. Mandatory exhaustive references and witness checks have not started.
There are no collection-output archives or inference-worker resource maxima.
No integrity failure occurred; runtime/architecture thresholds could not be
evaluated. Solver and reference execution time are zero. The recorded launch
integrity check took approximately 1.05 seconds, separate from experimental timing.

## Additional launch-scope issue

The frozen `experiments/a2_larger/run.py` entry point unconditionally schedules
N=8 physical anchors first, then all N=4,6,8,10,12 stages. Its CLI exposes no
volume selector and its final count check expects the whole study. It therefore
cannot be invoked as written for this N=4/6-only authorization. It was not
invoked, filtered or monkeypatched, and no substitute launcher was introduced.

Before collection, restore the frozen timing host and establish a reviewed
N=4/6-only launch path that preserves the frozen experimental behavior. Using a
different timing host would require an explicitly authorized prospective
re-freeze. No protocol, collector, binary, solver, reference, case selection,
budget, branching policy or timing rule changed in this checkpoint. N=8 is not
cleared: the N=4/6 batch remains entirely unattempted.
