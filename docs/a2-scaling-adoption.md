# Adoption of the reconstructed exploratory A2 dataset

On September 29, 2026 (America/Toronto), the project adopted the surviving
491,199-case run as the canonical **reconstructed exploratory 8-cell A2 scaling
dataset**. This is an adoption/publication checkpoint, not scientific acceptance
of the existing analysis.

> The canonical 491,199-case dataset is a reconstructed exploratory 8-cell A2
> scaling dataset. It is not the recovered original 410,953-case run, is not
> proven equivalent to that run, and is not a verified correction or superset
> of it. Its protocol was explicitly reconstructed after the original artifacts
> were lost. Its exact protocol and provenance are preserved. No claim depends
> on reproducing the historical 410,953-case count.

## Population

| Category | Cases |
| --- | ---: |
| 8x8 camera-family cases | 374,771 |
| 16x16 camera-family cases | 112,960 |
| Restricted/perturbed cases | 3,456 |
| Structured controls | 12 |
| **Total** | **491,199** |

These are cases across declared experimental strata, not globally distinct
worlds. Solver configurations, timed repetitions, and direct support queries
are not additional cases in this total. The 72 camera jobs and two structured
jobs are fully described in the preserved
[protocol](../results/a2-scaling-8cell/protocol.json), with matching
[provenance](../results/a2-scaling-8cell/provenance.json) and
[collection marker](../results/a2-scaling-8cell/collection.json).

## Reconciliation and limitations of continuity

The original raw dataset and matching provenance are unrecoverable. Original
mask values, exact suite selection, seeds, perturbation recipe, timing/query
sampling, and aggregation policy are unavailable. The exact cause of the
80,246-case count difference therefore cannot be established.

The replacement uses its own explicit deterministic choices in `protocol.json`:
30 placement masks, paired mixed-material and same-material palettes, six view
suites, 8x8 sampling and 16x16 controls at k=0,4,8, restrictions/perturbations,
structured SAT/UNSAT controls, four accepted solver configurations, and
1,000,000-node / 60-second per-call budgets with explicit `UNRESOLVED` on
exhaustion. Matching these broad design categories does not establish equality
of the original and replacement populations.

The saved collector includes every declared job and camera family. Its source
and protocol are unchanged between the surviving collector and evidence
commits. This supports internal reproducibility; it is not original
preregistration and does not prove the reconstructed design was uninfluenced by
knowledge of earlier results. The historical count is not a selection target.
There is no defensible "matching 410,953-case subset," and a new run solely to
recover that historical count is not justified.

The accepted A2 solver and branching policy, A0/A1 implementations, tests,
frozen Python reference, and accepted correctness evidence remain unchanged
from `e73b710a42bca1cd0c7a69b9b9c37db2595a1cb1`.

## Caveats retained for the separate analysis checkpoint

- Truth-weighted summaries weight image families by their truth populations;
  family-weighted summaries give each image family one vote. Placements and
  suites are then weighted equally within k. Family populations differ between
  palettes because equal materials merge observations. The original aggregation
  policy cannot be verified.
- Occupancy-balanced weighting is a secondary post-hoc reweighting of existing
  measurements. It caused no new solver execution and is not a new experiment.
- Observation perturbations can introduce impossible stone observations into
  the oak-only palette. Perturbation cases remain robustness controls and must
  not be treated as clean material-relabeling comparisons. Matching truth IDs,
  domains, and seeds alone does not make those mutated observations equivalent.
- Camera image families and every-97th-family direct-query samples are selected
  separately within each palette. They are not one-to-one paired workloads.

## Publication and review status

The collector, build/run scripts, protocol, provenance, manifests, 15 raw archive
parts, archived job logs, integrity records, and previously completed validation
evidence are preserved. The archive contains 373 recorded files, including the
lossless representation of 148 raw CSV tables; its manifests retain the original
CSV and gzip hashes. The [artifact index](../results/a2-scaling-8cell/README.md)
identifies the contents and the additional publication integrity record.

**Existing analysis outputs are not yet scientifically reviewed.** Existing
analysis JSON, CSV summaries, logs, and PNG/SVG charts are retained byte-for-byte.
The earlier [scaling note](a2-scaling-8cell.md) is a preserved preliminary account;
its interpretations are not accepted by this adoption. This record governs
dataset identity and review status wherever older wording suggests otherwise.

No solver cases, benchmarks, correctness gates, sanitizers, new scientific
analysis, chart generation, or larger-volume experiments are performed for this
adoption/publication checkpoint. Its checks are file/hash, manifest, population
accounting, source-preservation, and publication checks only. The publication
commit uses `[skip ci]` to avoid launching solver tests on push.

The next checkpoint is scientific review of the saved analysis and its
weighting, comparisons, uncertainty, and claims, followed by an appropriately
scoped report. This publication does not perform that review.
