# Milestone 2D1: A1 benchmark evidence

One complete, integrity-checked dataset is recorded in
[`results/milestone-2d1-benchmarks/`](../results/milestone-2d1-benchmarks/).
This is a benchmark checkpoint, **not the final Milestone 2 acceptance record**.
The milestone record and `cpp-milestone-2` tag remain pending.

The base is `f4bb6210c376dcd258321fea825d338e566fb691`, including the README-only
commit after accepted correctness gate `7b93b8e`. No production solver, geometry,
renderer, camera, block-state implementation, or frozen Python file changed.
The accepted test-only AABB corpus moved, without behavioral changes, into
`mcr_experiments` so tests and benchmarks share the same camera/family definitions.
The C++ inference library still has no dependency on this exhaustive corpus.

## Method

The model is the accepted 2 x 2 x 2 volume with air, stone cube, oak cube and oak
bottom slab; observations are background/stone/oak first-hit labels from the
established 8 x 8 pixel exact cameras. Axis suites use successive prefixes of
the six axis cameras; oblique suites use one camera and the opposite pair.
All 65,536 truth worlds participate in each suite's complete family partition.
All 195,620 distinct feasible observation families are measured, plus the 512
accepted restricted-domain/altered-observation fixtures (91 feasible, 421
infeasible). These fixtures are not an exhaustive set of all possible restrictions.

As in Milestone 1, each input gets one untimed validation/warmup projection and
five timed complete `exact_supports` calls. Every timed call must reproduce the
validated feasibility, supported domains, witness vector and all work counters.
The 980,660 A1 timing samples are all retained; no samples are trimmed. Quantiles
select sorted index `floor(p*(n-1))`. The principal timing distributions weight
observation classes equally and use each class's median of five calls. The JSON
also contains distributions over individual repetitions, per-case minimum/maximum
summaries, work counters and candidate counts. A0 uses its unchanged Milestone 1
collector: 29,032 classes, five calls each, saved per-class medians (not all five
individual A0 samples). A0 runs immediately before A1 on the same host.

The timed operation starts from the original input domains. It includes domain
copying, solver allocations, complete projection, witness construction, counters
and internal invariant checks. It excludes problem compilation, rendering,
exhaustive enumeration, pruning-audit callbacks, independent witness validation,
CSV output and destruction of the returned result. Standalone GAC and standalone
envelope feasibility run as **separate untimed diagnostics from the same input**.
GAC is not a prepass inside the accepted A1 `exact_supports` implementation.
GAC and envelope candidate counts therefore are not consecutive pipeline stages.

Each warmup's supported domains equal the independent C++ AABB family union.
Every returned witness belongs to that complete family and respects the input;
the union of witness states covers every supported literal. There are 344,634
verified projection witnesses, plus a separately checked envelope witness for
each feasible input. Offline analysis compares every row to the cached frozen
Python fixture, including family sizes, restrictions, supports, full-camera GAC
domains, genuine-freedom counts and true-slab-count weights. The fixture hash
matches the accepted 2C export. All eight aggregate identifiability results and
all 72 slab strata also match the earlier Python research results.

Identifiability weights the 65,536 truth worlds uniformly, not observation classes.
A cell is identified only when its exact supported domain is a singleton. A cell
is freely variable only if replacing it by any input-allowed state in every
feasible context preserves feasibility; a full marginal alone does not suffice.
Geometry identification and cube/slab ambiguity are also saved. For infeasible
inputs these population fractions are undefined and are not reported.

## Recorded results

Timing columns are microseconds across per-class medians. Identification and
unique-world fractions use the uniform truth-world weighting just described.

| Suite | Classes | Median us | p95 us | p99 us | Identified cells | Unique world |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| axis_1 | 625 | 4.146 | 6.030 | 6.880 | 12.50% | 0.0244% |
| axis_2 | 5,625 | 2.795 | 5.748 | 7.041 | 36.91% | 2.82% |
| axis_3 | 24,705 | 2.754 | 5.889 | 7.752 | 69.25% | 21.75% |
| axis_4 | 32,738 | 3.224 | 6.380 | 8.543 | 78.09% | 31.32% |
| axis_5 | 38,514 | 3.435 | 6.680 | 9.213 | 82.46% | 41.69% |
| axis_6 | 45,270 | 3.856 | 6.961 | 9.804 | 87.06% | 54.32% |
| oblique_1 | 11,353 | 3.855 | 7.311 | 8.443 | 66.94% | 4.68% |
| oblique_2 | 36,790 | 4.317 | 7.261 | 9.094 | 85.85% | 31.98% |

Restricted inputs have median times of 3.055 us when feasible and 0.210 us when
infeasible (p95 5.118 and 0.470 us). The longest camera-class median is 30.396 us;
the longest individual repetition is 1,748.727 us. Shared-host scheduling may
contribute to such tails; no specific cause was measured. All samples remain in
the raw data. The back-to-back clock median is 20 ns, with no subtraction.

Every recorded propagation, envelope and complete-support call uses **zero
search branches and zero search nodes**. Across one projection per input there
are 1,482,211 conditioned support queries, 1,333,288 of them infeasible, and
1,678,343 envelope calls including the root calls. Conditioned queries test
assumptions; they do not branch recursively over assignments. Query counts have
per-suite medians of 7–11 and a maximum of 17 for the camera corpus. Work-counter
totals in the summary count one call per input, not all five timing repetitions.
Deletion counters include repeated conditioned calls and must not be interpreted
as unique states removed from the original scene.

Each unrestricted scene starts with 32 candidate states. For example, axis_6 has
mean counts 8.334 after standalone GAC, 15.660 after root envelope feasibility,
and 8.334 after exact support projection. Root feasibility retains unsupported
alternatives; complete conditioned queries are doing useful work. Standalone GAC
happened to match exact supports on every unrestricted camera family in this
corpus. This empirical observation neither proves GAC complete for arbitrary
inputs nor changes the accepted solver. Restricted contradictions are handled
by the complete kernel. No optimization was made from these observations.

More views do not necessarily increase runtime: reduced ambiguity can lower the
number of conditioned queries. They also do not uniquely identify every truth:
even six axis views leave 12.94% of cells ambiguous on this finite population.
The one-view full marginal/freedom distinction is visible: axis_1 has zero
completely free cells despite substantial ambiguity. Later observations can
establish an occluder and thereby make a hidden cell free; freedom is not a
monotone function of view count.

## Environment, memory and comparisons

The recorded host is Linux 6.18.44, x86_64, AMD EPYC 9V74; nine logical CPUs are
visible with an eight-CPU-equivalent cgroup quota. The collectors are single
threaded, with no CPU pinning or governor control. GCC 13.3.0, CMake 4.4.3 and
Ninja 1.13.2 build Release with `-O3 -DNDEBUG`. Python 3.12.14 and NumPy 2.3.5 are
used only for offline reference/analysis work. Exact environment details are in
`environment.json`; timings are machine- and run-specific.

The A1 C++ process took 9.395 seconds overall. Of this, summed timed projections
took 4.133 s; corpus setup 0.062 s; family partition/fixture reading 0.282 s;
oracle metrics 0.490 s; problem setup 0.758 s; and diagnostics/warmup/validation
2.914 s. These scopes do not exhaust wall time: output, destruction and other
loop overhead remain. Python analysis/compression is outside this process and
these times. No pruning-audit callbacks are included in the benchmark.

A1 peak RSS is **21,952 KiB (21.44 MiB)** for the whole C++ process, including the
exhaustive corpus, family maps, allocator and output buffers. Its start high-water
mark was 11,648 KiB and after corpus construction 13,120 KiB. These are high-water
marks, not additive allocation measurements; subtracting them does not give
solver memory. A0 peak RSS is 11,648 KiB. Neither value is solver-only memory or
a memory-scaling result. Raw output was staged on `/dev/shm`; tmpfs/kernel memory
and the later Python analysis process are not included in C++ process RSS.

Current A0 median complete-support times range from 2.193 us (axis_1) to 8.232 us
(oblique_2). They use a different state space, class population and algorithm
from A1, so their ratios are not an isolated geometry or language cost. A cleaner
ambiguity comparison restricts A1 truth worlds to the 6,561 zero-slab worlds while
still permitting slabs during inference. With six axis views, identified cells
then fall from A0's 93.87% to A1's 91.04%; with the oblique pair, from 99.15% to
92.96%. Adding possible states reduces certainty even when the truth has no slabs.

The earlier Python nested solver also recorded zero branches, whereas its generic
search baseline used up to 51 on tested A1 cases. That was avoidable generic
search, not intrinsic hardness of this nested model. Historical Python timings
and tested-case counts are retained for context in `a1/summary.json`; sampling,
implementation, repetitions and environment differ. No controlled Python/C++
speedup or claim about larger volumes, incomparable geometry, or real images
follows from this tiny known-camera experiment.

## Validation and output integrity

The focused Release checks passed: benchmark smoke, A1 propagation, feasibility,
requested support, projection, and A0 inference. The first invocation could not
start the projection executable because a preserved build artifact was empty;
relinking that target restored it and the test passed. Initial/recovery logs are
retained. After output handling changed, the final smoke check forced file
rollover after every row and passed. The final ASan/UBSan collector run passed
16 camera families and all 512 restricted fixtures, also with one-row files.
LeakSanitizer was disabled in the traced runtime; no leak-check result is claimed.
No source correctness bug was found. The unchanged production/reference hashes
retain the accepted 2C gate; that expensive gate was not unnecessarily rerun.

Attempts 1 and 2 reported successful collection but their saved largest CSV was
truncated. Attempt 3 split the CSVs but two parts were still truncated. Entire
attempts were excluded for file integrity, not for timing values. Their logs,
metadata and explicit failure counts/hashes are kept under `discarded-attempt-*`;
damaged raw files are not included as benchmark evidence. The underlying storage
failure was not diagnosed, so splitting alone is not claimed to fix it.

The successful fourth collection used fresh memory-backed temporary storage.
CSV parts hold at most 10,000 rows and are written to temporary names, closed,
checked and renamed. The offline checker requires every expected family and
restricted case exactly once, 61 columns per row, all five timing samples,
frozen-reference agreement, valid statistics and correct time accounting.
Only then are deterministic gzip archives produced, decompressed and checked
against raw SHA-256 hashes. Archives and summaries were copied to the repository,
hashed again, staged in Git, and read back from the index before temporary data
was removed. This produced 25 complete A1 archives plus one A0 archive (8,004,566
compressed bytes total). Both compressed and uncompressed hashes are saved.
The interrupted run's complete data survived; resume required integrity checks,
not another timing run or correctness gate.

## Reproduction

From the repository root, first check saved evidence without inference:

```sh
python3 experiments/verify_benchmark_artifacts.py results/milestone-2d1-benchmarks
```

To collect new measurements, use fresh directories; on Linux the following keeps
unpublished raw data on tmpfs. Do not run builds/tests concurrently with timings.
The fixture exporter verifies a cached accepted fixture or regenerates it when
absent. The cache is an oracle input, not part of the timed solver.

```sh
cmake --preset release
cmake --build --preset release --parallel 2
python3 tests/oracle/export_a1_acceptance.py build/release/oracle_a1
bench_tmp=$(mktemp -d /dev/shm/mcr-a1-benchmark.XXXXXX)
build/release/mcr_a0_benchmark --output "$bench_tmp/a0" --repetitions 5
build/release/mcr_a1_benchmark --output "$bench_tmp/a1" --repetitions 5 \
  --restricted-fixture build/release/oracle_a1/acceptance_cases.bin
python3 experiments/analyze_a1_benchmark.py --raw "$bench_tmp/a1" \
  --oracle build/release/oracle_a1 --a0 "$bench_tmp/a0" --output "$bench_tmp/checked"
```

The analyzer emits all compressed A1 data, raw/compressed hash manifests and
summaries only after reference agreement. Preserve its output and run/environment
logs, then verify the destination files before removing temporary storage.
The unchanged A0 collector emits `cases.csv` and `summary.json`; the recorded A0
CSV was gzip-compressed losslessly with an analogous hash manifest.

To replay frozen-reference analysis of the saved A1 data without new timings,
decompress the 25 archives to a fresh directory, copy `a1/run.json` alongside
them, and pass that directory as `--raw`, the accepted fixture directory as
`--oracle`, and the recorded `a0` directory as `--a0`. The resulting `summary.json`
must equal the saved summary. `checkpoint.json` ties source/configuration hashes
and evidence files to this checkpoint; `integrity-resume.json` records the
successful post-interruption archive check.

Only the final evidence review, final Milestone 2 record and milestone tag remain
for closeout. No A2 implementation or scaling study is part of this checkpoint.
