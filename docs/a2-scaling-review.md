# Scientific review of the reconstructed 8-cell A2 dataset

Working review checkpoint; **not the final scaling report**. Input is the dataset
published at `69032371f396802ddc5880f4cc65d8805eb60585`, tree
`cc80882392543fbb0874164d8bfe0968bcede161`. The
[adoption record](a2-scaling-adoption.md) remains authoritative: this is the
**canonical reconstructed exploratory 8-cell A2 scaling dataset**, not the
recovered 410,953-case run, not proven equivalent to it, and not a verified
correction or superset. Its protocol was explicitly reconstructed after the
original artifacts were lost. No conclusion depends on the historical count.

Labels below distinguish **M: directly measured**, **I: supported interpretation**,
and **U: unresolved by this experiment**. The conclusions concern only the fixed
2×2×2 volume, the declared masks/cameras/domains, and the accepted A2 solver.

## Evidence and measurement definitions

**M.** Reading committed archive bytes reproduced all five preserved numerical
tables: 11,912 rows and 300,448 numeric cells, within their saved nine-significant-
digit rounding. No preserved numerical value needed correction. All 15 archive
parts, the combined archive, and 373 archived files passed size/hash checks.
The [review evidence](../results/a2-scaling-review/README.md) includes the
data-only recomputation script, expanded tables, and integrity record. No solver,
renderer, benchmark, correctness gate, sanitizer, or chart generator ran.

The population is **374,771 8×8 camera-family cases + 112,960 16×16 camera-family
cases + 3,456 restricted/perturbed cases + 12 structured controls = 491,199**.
There are 12,624,840 saved timed calls, including 836,064 direct support queries.
These are stratified cases/calls, not 491,199 distinct geometries. The camera
population represents 818,808 truth/placement/suite/palette memberships.

Primary camera summaries retain two separate populations. Within each
placement/suite, **truth weighting** assigns weight equal to family size;
**family weighting** gives each distinct image family one vote. Placements and
the six suites then receive equal weight at fixed k. Explicit overall primary
summaries below additionally give each k and palette equal weight. Neither
weighting is the simple pool of all saved families across strata.

Feasibility/projection latency is the **median of three saved calls per case**;
reported means average those medians. New k-level median/p95 values use the
normalized mixture of case weights, not averages of stratum percentiles.
Separate columns retain averages over all three trials and individual-call
maxima. Query timings have one trial per literal. Oracle enumeration, reference
work, root diagnostics, witness audits, and output are outside API timing;
propagation/search, counters and budget checks are inside. Whole-process RSS
includes exhaustive/reference data and is not solver memory. Existing validation
records were inspected, not rerun; agreeing with saved reference columns is not
a new independent correctness gate.

## Primary results by k

**M.** Mean full-projection branches, 8×8, **both** options. Mixed is the
`split` oak-bottom/stone-top palette; same is oak-only. Only camera cases enter.
Times are primary truth-weighted mean microseconds.

| k | Mixed truth | Mixed family | Same truth | Same family | Mixed time µs | Same time µs |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0 | 0 | 0 | 0 | 13.530 | 13.970 |
| 1 | 0 | 0 | 0 | 0 | 12.521 | 12.767 |
| 2 | 0.001736 | 0.000687 | 0.010055 | 0.003141 | 11.511 | 12.691 |
| 3 | 0.093750 | 0.073376 | 0.162375 | 0.092836 | 11.340 | 12.776 |
| 4 | 0.004630 | 0.001585 | 0.036458 | 0.011059 | 10.699 | 12.426 |
| 5 | 0.479681 | 0.339841 | 0.686686 | 0.403905 | 11.937 | 14.535 |
| 6 | 0.770576 | 0.579809 | 1.128587 | 0.700312 | 12.728 | 16.714 |
| 7 | 1.297258 | 1.070074 | 1.960086 | 1.289959 | 14.360 | 19.585 |
| 8 | 1.919753 | 1.691413 | 2.986105 | 2.029430 | 16.144 | 23.241 |

**M.** Feasibility requires less work than full projection: at k=8, truth-weighted
mean branches are 0.400371 mixed and 0.656887 same (family: 0.394650/0.510520).
The corresponding primary truth runtime/work distributions are:

| Palette/API, k=8 | Mean µs | Median µs | p95 µs | Mean nodes | Mean branches |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mixed feasibility | 6.833 | 6.791 | 9.905 | 1.830 | 0.400 |
| Same feasibility | 7.883 | 7.551 | 12.529 | 2.183 | 0.657 |
| Mixed projection | 16.144 | 12.408 | 48.714 | 7.102 | 1.920 |
| Same projection | 23.241 | 17.266 | 56.956 | 9.689 | 2.986 |

All k, both primary weights, both APIs, four configurations, node/branch
distributions, supported-literal counts and root diagnostics are in
[by-k-primary.csv](../results/a2-scaling-review/by-k-primary.csv).
Configuration matters even at k=0: search-only projection averages 2.378255
truth-weighted branches in both palettes, versus zero with fixed-hit/both.
Search-only k=8 averages are 3.230173 mixed and 4.319845 same. Thus “zero search
at low k” describes the optimized settings, not every configuration.

**M/I.** Work is not monotone in k. At k=3, the four tested masks 7, 25, 44 and
224 have mixed-palette truth-weighted projection means 0, 0.375, 0 and 0; the
same-palette means are 0, 0.613812, 0.025656 and 0.010031. The k=4 dip appears
under both primary weights. Arrangement and visibility therefore matter at fixed
k; four selected masks per intermediate k do not identify an arrangement-free
effect of k. Masks at adjacent k are not a complete nested comparison.

## Secondary occupancy sensitivity — separate from primary results

**M.** Under uniform legal worlds, a selected cell is occupied with probability
2/3 and another cell with probability 1/2. Expected occupancy is **4+k/6**,
rising from 4 to 5⅓ cells. The secondary weights give every cell occupancy 1/2,
and each selected slab half probability 1/2 conditional on occupancy. Expected
occupancy is then 4 at every k.

The collector's saved family numerator is the sum, over existing family truths,
of `2^(k - occupied_incomparable_cells)`, with denominator `2^(8+k)`.
Source inspection verifies that recipe; rereading the saved numerators verifies
complete mass in every camera stratum. Reweighting uses exactly the existing
per-family measurements. It creates no cases, calls or new observations.

| k | Primary expected occupancy | Balanced mixed projection branches | Balanced same projection branches |
| ---: | ---: | ---: | ---: |
| 0 | 4.000 | 0 | 0 |
| 1 | 4.167 | 0 | 0 |
| 2 | 4.333 | 0.001465 | 0.007487 |
| 3 | 4.500 | 0.053304 | 0.093587 |
| 4 | 4.667 | 0.003581 | 0.024231 |
| 5 | 4.833 | 0.259928 | 0.367355 |
| 6 | 5.000 | 0.408656 | 0.581985 |
| 7 | 5.167 | 0.658562 | 0.960594 |
| 8 | 5.333 | 0.940908 | 1.392619 |

**I.** At k=8, **49.0% mixed / 46.6% same** of the primary branch mean remains
after balancing; growth above the zero-branch k=0 baseline survives, along with
the k=4 dip. This demonstrates sensitivity to truth composition, not that density
causes exactly the removed fraction. In particular, balanced projection runtime
is 12.615/15.938 µs at k=8 versus 13.530/13.970 at k=0: surviving branch growth
does not imply uniformly rising wall time. The saved chart title “Density explains
part of the branch growth” should be read as a sensitivity result, not a causal
decomposition. Its bytes were preserved; no charts were regenerated.

## Palette, visibility and ambiguity

**M/I.** The clean palettes share geometry, masks, legal truth populations,
domains, cameras and restriction seeds. Erasing stone/oak distinctions merges
images, reducing 8×8 families from **207,278 mixed to 167,493 same**; at 16×16
the counts are 58,016/54,944. Thus primary truth weighting compares the same
world population; raw family-weighted means compare different image-family
populations. Both palettes retain geometry/occlusion coupling. Labels add useful
information in several suites, but do not remove that geometric coupling.

**M.** At k=8, 8×8, both options, primary truth weights:

| Suite | Mixed branches | Same branches | Mixed unique-world fraction | Same unique-world fraction |
| --- | ---: | ---: | ---: | ---: |
| axis_1 | 10.074074 | 10.074074 | 0.000152 | 0.000152 |
| axis_2 | 1.404664 | 6.980643 | 0.128182 | 0.067215 |
| axis_3 | 0.020119 | 0.110349 | 0.597013 | 0.498247 |
| axis_6 | 0 | 0.110349 | 0.713763 | 0.498247 |
| oblique_1 | 0.019662 | 0.600061 | 0.337906 | 0.159427 |
| oblique_2 | 0 | 0.041152 | 0.976528 | 0.498857 |

Across suites, k=8 mean supported-literal counts are 10.215 mixed and 11.216
same (8 means a singleton). Six-axis cell identifiability is 93.873%/83.631%,
distinct from the unique-world fractions above. Extra views need not improve
wall time: same-material axis_3 and axis_6 have the same family-size, support
and branch summaries, but projection means are 11.170 versus 18.128 µs.

**M.** Every one of the **24,002 tested 16×16 six-axis camera families** at
k=0,4,8 is a singleton. Feasibility/projection use one node and zero branches
in every mode; all 24,384 sampled direct-query calls for those scenes also
use zero branches. Across all six 16×16 suites, both-option truth-weighted
projection branches are zero for mixed at k=0,4,8, and 0/0.004244/0.010339
for same. This is a resolution control on eight cells, not a larger volume.

**M/I.** With both options, all 285,880 singleton 8×8 families require zero
branches; only 9,851 of 88,891 ambiguous families branch. At 16×16, all 94,414
singleton families use zero branches; 64 of 18,546 ambiguous families branch.
Ambiguity is therefore not sufficient to force search. Root residual components
reach eight cells, but the worst projection has four two-cell components.
Root summaries precede conditioning and are not descendant search traces.
Visibility/ambiguity are essential explanatory dimensions alongside k; this
experiment does not estimate a causal ranking of their importance.

Observation-label cycling can introduce impossible stone observations into the
oak-only palette. All restricted/perturbed cases remain separate robustness
controls; none enter the clean palette or primary camera comparisons.

## Optimization comparison

**M.** Descriptive totals over all **374,771 saved 8×8 camera families**, each
counted once across strata. This pool is not either primary equal-stratum average.
Work counts use one deterministic execution per case; time sums use case medians.

| Configuration | Projection branches | Projection nodes | Fixed-hit calls | Component solves | Sum of case medians, s |
| --- | ---: | ---: | ---: | ---: | ---: |
| search | 97,453 | 616,443 | 0 | 0 | 3.963700 |
| decomposition | 97,453 | 656,931 | 0 | 40,488 | 3.951813 |
| fixed-hit | 47,739 | 596,314 | 57,716 | 0 | 3.946784 |
| both | 47,883 | 621,866 | 60,624 | 25,542 | 3.952743 |

Fixed-hit reduces pooled projection branches **51.0%**, both **50.9%**, while
the median-time sums decrease only **0.43%/0.28%**. Fixed-hit reduces branches
in 23,083 families, leaves 350,568 equal, and increases them in 1,120. Adding
decomposition to fixed-hit adds 144 branches across 72 cases. The fixed-hit
option also changes which variables are eligible for branching; these results
do not isolate only the shortcut routine. Projection uses witness reuse and
multiple support searches, so fewer branches need not imply fewer entries.

**M.** The primary comparisons, additionally averaging k and palette equally,
give a different magnitude:

| Primary population | Configuration | Mean branches | Mean nodes | Mean µs |
| --- | --- | ---: | ---: | ---: |
| Truth | search | 2.187985 | 4.486513 | 14.833185 |
| Truth | decomposition | 2.187985 | 5.983327 | 14.265635 |
| Truth | fixed-hit | 0.638971 | 3.599793 | 13.948901 |
| Truth | both | 0.640985 | 4.156610 | 14.081918 |
| Family | search | 1.075224 | 2.801325 | 10.516412 |
| Family | decomposition | 1.075224 | 3.448904 | 10.285343 |
| Family | fixed-hit | 0.460064 | 2.500284 | 10.287918 |
| Family | both | 0.460413 | 2.847591 | 10.345106 |

Both versus search reduces primary truth branches 70.7% and mean time 5.1%;
the family reductions are 57.2% and 1.6%. In the descriptive feasibility pool,
fixed-hit/both reduce branches from 48,691 to 18,320; mean case-median times
change from 6.520 µs to 6.368/6.377 µs. Full fixed-hit usage, rejection and
component-entry statistics are retained in the evidence tables.

**M/I.** Decomposition alone leaves branches unchanged **case by case** for
feasibility/projection and all sampled queries in both camera resolutions.
Its added 8×8 projection nodes equal its 40,488 component solves. It still helps
the deliberately disconnected path-plus-triangle UNSAT fixture in both palettes:
feasibility drops from **10 nodes/9 branches to 8 nodes/5 branches**; saved
search/decomposition medians are 15.133/11.387 µs mixed and 14.993/11.117 µs same.
Two satisfiable paths retain four branches and gain node/time overhead instead.
This supports the intended mechanism in a targeted fixture, not general benefit
from decomposition or disconnection alone.

**U.** A reliable general speedup is not established. These are instrumented
microsecond calls on one recorded host/run, with three interleaved trials and
no independent run replicates. Pooling all three trials instead of their medians
changes runtime magnitudes; those values are separately preserved. Branch,
component-entry and wall-clock effects must not be conflated.

## Status, sampled queries, worst cases and budgets

**M.** Camera families are SAT by construction. Perturbations contain 1,039 SAT
and 2,417 UNSAT cases; **all 2,417 UNSAT cases are root-GAC contradictions**.
Structured fixtures contain eight SAT and four UNSAT cases, the latter retaining
nonempty root domains. Consequently this experiment says little about general
hard UNSAT behavior. The physical triangle retains global infeasibility despite
local consistency; the physical path retains two worlds and unsupported air
values after GAC.

Direct queries sample every 97th lexicographic camera family separately per
palette, plus all controls, with all 24 cell/state literals. The 836,064 saved
calls include 158,588 literals excluded by input domains. Query UNSAT means
unsupported literal, not necessarily an UNSAT scene. This deterministic sample
is not a truth- or family-population estimator and is not matched across palettes.
For both options, the 8×8 camera sample contains 34,128 supported calls (median
5.769 µs, p95 9.444, maximum 6 nodes/4 branches) and 62,880 unsupported calls
(median 1.102 µs, p95 3.285, zero branches). At 16×16 these counts are
9,857/18,919, medians 6.941/1.102 µs and p95 20.001/4.597 µs; maxima are
2 nodes/1 branch for supported and 1 node/0 branches for unsupported calls.
The saved by-k query tables separate category, palette and literal/scene status.

**M.** Global maximum node and branch counts, each maximized separately:

| API | Search or fixed-hit max nodes / max branches | Decomposition or both max nodes / max branches |
| --- | ---: | ---: |
| Feasibility | 10 / 9 | 13 / 8 |
| Full projection | 45 / 32 | 61 / 32 |
| Sampled direct query | 10 / 9 | 8 / 5 |

The worst branch-count projection with both options is an 8×8 axis_1 case at
k=8, mask 255, case 255, representative truth 3320, in both palettes: **61 nodes,
32 branches, 16 worlds, 16 supported literals, four two-cell residual components**.
Case medians are 57.186 µs mixed and 57.708 µs same. These 32 branches aggregate
several support searches, not one irreducible eight-cell search.

The slowest individual call is instead **8.135623 ms**: search-only projection
in `r8_m090_split`, k=4, mask 90, oblique_2, case 4700, truth 13. It is a
singleton, with one node, zero branches and a three-trial median of 11.667 µs.
The tail therefore does not track maximal combinatorial work; its precise timing
cause is unmeasured. Case-median and single-trial maxima are both preserved.

**M.** There are **zero UNRESOLVED outcomes in all 12,624,840 saved calls**.
Budgets were 1,000,000 search-node entries and 60 seconds per call, with explicit
UNRESOLVED on exhaustion. Observer/post-return checks are not a hard process
kill; the aborting attempted entry can be counted. Observed work and latency
are far below these limits, so this experiment does not measure behavior near
budget exhaustion.

## Conclusions accepted at this checkpoint

- **M:** The measured optimized branch means rise overall toward k=8 but are
  nonmonotone; arrangement and view suite change work at fixed k.
- **M/I:** The occupancy sensitivity retains roughly half the k=8 primary branch
  mean; it qualifies, rather than causally decomposes, the primary k trend.
- **M/I:** Material labels reduce ambiguity and work in several matched-truth
  comparisons; the palettes have different family populations.
- **M:** Fixed-hit yields substantial aggregate branch reductions with small,
  weighting-dependent recorded runtime differences. Decomposition saves no
  measured camera branches but helps the disconnected UNSAT control.
- **M/I:** The tested 16×16 six-axis scenes are unique and branch-free. Ambiguity,
  visibility and residual coupling are indispensable alongside raw k.
- **U:** Exponential complexity, arbitrary-volume scaling, general decomposition
  benefit and practical Minecraft-scale performance remain unestablished.

Four intermediate-k masks, six synthetic view suites, finite perturbations and
one host/run do not establish those broader claims. Marginal supported literals
are not independent choices of whole worlds. Cameras/grid are known and exact;
there is no noise, lighting, texture, transparency or unknown-camera experiment.
The lost original protocol cannot supply additional evidence or a matching
410,953-case subset. No rerun solely to recover that count is justified.

For the subsequent experiment, predeclare a small increase in volume and a
factorial design separating volume, k/fraction, occupancy, arrangement,
visibility and palette. Use matched truths, fixed-occupancy strata alongside
the original legal-world population, and both connected hard SAT/UNSAT and
disconnected controls. Retain all four modes, budgets and UNRESOLVED accounting;
measure components/entries as well as branches, keep oracle cost separate, and
use independent timing repetitions. Preserve exhaustive reference coverage
where affordable. This is a recommendation only: no larger-volume work has
started, and the accepted solver and frozen reference remain unchanged.
