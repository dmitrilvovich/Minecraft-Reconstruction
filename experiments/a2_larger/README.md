# Larger-volume A2 experiment infrastructure

See the [preregistered protocol](../../docs/a2-scaling-larger-protocol.md) and
[implementation/preflight record](../../docs/a2-scaling-larger-preflight.md).
The accepted solver and frozen Python are linked/imported unchanged.

Entry points have separate purposes:

- `bash experiments/a2_larger/build.sh`: compile the frozen measurement worker.
- `generate.py --output DIRECTORY`: prepare exact inputs, with no inference calls.
- `preflight.py --output NEW_DIRECTORY --cpu CPU`: the fixed bounded preflight,
  not a pilot or timing epoch. The published preflight is already complete.
- `run.py`: the future collection entry point. Requires explicit input/output,
  pinned CPU and `--collection-authorized-checkpoint`; it was not invoked here.

Input archives are indexed in
`results/a2-scaling-larger-v1/inputs/input-manifest.json`. Each archive's
`.complete.json` records its compressed and uncompressed hashes, record count
and provenance. `common.verify_archive` verifies both representations before
`common.read_records` returns data records without header/footer markers.

The protocol's original status flags describe its preregistration checkpoint;
they are intentionally not rewritten. The generated input manifest and preflight
record describe implementation readiness. Only separate authorization permits
the next collection checkpoint. Preflight and main results must never be pooled.

Failure recovery is conservative: completed archives are verified and reused;
unsealed partials retain their evidence and stop automatic scheduling. Missing
outputs are not fabricated and are never converted into UNSAT. A changed source,
protocol, executable or environment fails resume identity checks. Root diagnostics,
references, preparation, I/O and solver costs retain separate records.
