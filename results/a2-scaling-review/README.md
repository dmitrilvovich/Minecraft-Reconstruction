# Data-only scientific review evidence

The [working scientific review](../../docs/a2-scaling-review.md) governs the
interpretation of these tables. Input is exclusively the committed reconstructed
exploratory dataset at `69032371f396802ddc5880f4cc65d8805eb60585` (tree
`cc80882392543fbb0874164d8bfe0968bcede161`). It is not the lost 410,953-case run;
the [adoption caveats](../../docs/a2-scaling-adoption.md) continue to apply.

The [review script](../../experiments/review_a2_scaling.py) reads the saved NPZ
measurement columns through `git show`, without unpacked temporary CSVs. It
imports no solver or renderer and creates no observations or charts. It checks
archive/file hashes, population accounting, saved status/reference-column
agreement, repeat counters, paired configuration keys, and previous summary
arithmetic. These are checks of published data, not new correctness experiments.

## Outputs and populations

| File | Meaning |
| --- | --- |
| `by-k-primary.csv` | Camera cases, all four modes and both APIs; separate truth and family weighting within placement/suite, then equal placement/suite weights at k. |
| `by-k-occupancy-sensitivity.csv` | Same cases/measurements, only the secondary occupancy-balanced weighting; never part of the primary average. |
| `visibility.csv` | Both options, camera means by resolution/palette/k/suite, equal placements; weighting explicitly identified. |
| `arrangements.csv` | Both options, camera means by resolution/palette/k/mask, equal suites; weighting explicitly identified. |
| `configuration-pairs.csv` | Per-job paired counts of lower/equal/higher work and time, with sums for each mode, API, category and answer. Raw family/call counts across strata, not primary weights. |
| `pooled-operation-status.csv` | Descriptive unweighted pools of cases or sampled literals, split by resolution, category, mode, operation, literal/API answer and scene SAT status. `ALL` is an additional palette aggregate, not extra cases. |
| `sampled-queries-by-k.csv` | The same descriptive direct-query distributions with k retained, each palette separately. Deterministic query samples are not population estimators. |
| `query-input-exclusions.csv` | Query calls and input-excluded literals by job/mode. Each selected case has all 24 literals. |
| `ambiguity.csv` | Both options: counts/work sums by job/suite and unique/ambiguous family band. Repeated rows across APIs represent the same cases. |
| `structured.csv` | All 12 structured cases × four modes × two APIs; timing column correctly named `median_us`. |
| `control-populations.csv` | SAT/UNSAT and root-consistency counts for restricted/perturbed and structured cases, separate from camera families. |
| `worst-per-job.csv` | One maximizing row per job/mode/API/criterion: nodes, branches, case-median time or raw individual-call time. Ties choose the first saved row; this is not a complete list of tied maxima. |
| `whole-process-memory.csv` | Saved per-job RSS including oracle/reference data, not solver-only memory. |
| `review-integrity.json` | Input identities/hashes, preserved-summary comparisons, population checks, environment, script/output hashes and 16×16 six-axis evidence. |

`split` means mixed oak/stone and `same` means oak-only. Numeric configuration
results come from the four pre-existing settings: search, decomposition,
fixed-hit and both; all retain GAC. `k` and `mask` in structured jobs are job
metadata, not another camera k stratum. Do not mix structured rows into k trends.

For feasibility/projection, `mean_us` averages per-case medians of three calls;
`median_us` and `p95_us` describe that distribution. `mean_trial_us` includes all
three trials, `max_us` is the maximum case median, and `max_raw_us` is the largest
individual trial. Work counters agree across repeats and are counted once per
case. For direct queries, each measurement is its single saved call.

The by-k quantiles use the normalized mixture of primary or secondary case
weights. Pooled descriptive tables instead use NumPy's unweighted median/linear
quantile convention; they do not have the primary stratum weights. The preserved
by-k table had no k-level percentiles. The old `structured.csv` column `mean_us`
contained one case's median of three trials; the new label clarifies that without
changing its values.

`answer=UNSAT` on a direct query means an unsupported literal; `case_sat`
separately identifies whole-scene feasibility. Root component, shape-variable
and support fields describe the original case, not each conditioned query or a
descendant search trace. `mean_component` averages the largest root component;
`max_component` is its maximum. Fixed-hit call/check/rejection counters are
different quantities and are not interchangeable success counts. Projection
nodes/branches aggregate its several support searches and component entries.

Family size is the number of feasible worlds; supported-literal counts are
marginal ambiguity and do not imply independent choices. Different palettes
induce different image-family populations. Perturbations can introduce impossible
stone observations into oak-only cases, so they remain separate robustness data.

## Recompute these summaries only

With Git, Python, NumPy and pandas, from a checkout containing the published
input tree:

```sh
python experiments/review_a2_scaling.py --output build/a2-review-recomputed
```

This command only reads committed data and writes summary CSV/JSON files.
The recorded environment and script hash are in `review-integrity.json`.
Saved original summaries, charts, raw archives and validation records remain
unchanged. Publication uses `[skip ci]` so the repository's push-triggered solver
checks do not run during this review-only checkpoint. No new timing experiment
or scientific case is part of this evidence.
