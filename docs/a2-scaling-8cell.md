# A2 eight-cell scaling: final report

**The fixed eight-cell exploratory scaling experiment is complete.** This report
synthesizes the [scientific review](a2-scaling-review.md) published at
`5025ac42b62bd5d3469b858021ad8801d3e65654`. It introduces no new measurements,
calculations, tables derived from data, or charts; the numerical results below
are reproduced from that review.

> The canonical 491,199-case dataset is a reconstructed exploratory 8-cell A2
> scaling dataset. It is not the recovered original 410,953-case run, is not
> proven equivalent to that run, and is not a verified correction or superset
> of it. Its protocol was explicitly reconstructed after the original artifacts
> were lost. Its exact protocol and provenance are preserved. No claim depends
> on reproducing the historical 410,953-case count.

The [adoption record](a2-scaling-adoption.md) and
[recovery history](a2-scaling-recovery.md) document this distinction. The original
mask values, exact suite selection, seeds, perturbation recipe, timing/query
sampling and aggregation policy remain unavailable. The reconstructed design
is not original preregistration, nor proof that its choices were uninfluenced
by knowledge of earlier results. There is no defensible matching 410,953-case
subset, and no justification for a new run solely to recover that count.

## Research question and scope

> On a fixed eight-cell known-camera problem, how does exact inference behave
> as incomparable block geometry is introduced, and what factors besides
> incomparable-cell count affect search?

The reconstructed corpus shows that search depends on more than the number of
cells admitting incomparable shapes. Arrangement, visibility, ambiguity, material
information and the sampled occupancy distribution all matter to the observed
work. Fixed-hit substantially reduces aggregate branches, while decomposition
demonstrates its benefit in a targeted disconnected control. Neither result
establishes a general runtime improvement or useful scaling to larger volumes.

This is a controlled exploratory experiment on a **2×2×2 volume** with known,
exact cameras and synthetic first-hit observations. It is not evidence of
practical Minecraft-scale performance.

The progression from A0 to A2 explains why search is being studied. A0 uses fixed
occupied geometry. A1 introduces nested geometry and admits a complete
zero-search envelope method under the established model assumptions. A2 adds
bottom and top slabs, neither containing the other; their union need not be a
legal state. The A1 completeness argument therefore stops applying. Generalized
arc consistency (GAC) remains useful but is no longer complete, and the accepted
A2 implementation uses exact residual search where propagation and shortcuts
do not settle the problem. The physical path and triangle fixtures demonstrate
this gap in both palettes; they do not prove that exponential search is inherent.
See the [A1 acceptance report](milestone-2.md) and
[A2 foundation](a2-foundation.md) for the underlying arguments.

## Corpus and measurement

The canonical reconstructed dataset, published at
`69032371f396802ddc5880f4cc65d8805eb60585`, contains:

| Category | Cases |
| --- | ---: |
| 8×8 camera-family cases | 374,771 |
| 16×16 camera-family cases | 112,960 |
| Restricted/perturbed cases | 3,456 |
| Structured controls | 12 |
| **Total** | **491,199** |

These are cases across declared strata, not globally distinct geometries. At k
selected cells, domains allow air, oak bottom slab and a top slab; other cells
allow air or oak bottom slab. The top slab is stone in the mixed palette and
oak in the same-material palette. The 30 masks comprise the unique k=0 and k=8
masks and four arrangements at each intermediate k. Every placement enumerates
its legal truths and groups identical complete observations into image families.

Six suites use one, two, three or six axis cameras, or one or two oblique cameras.
All masks use 8×8 sampling; 16×16 controls cover the six placements at k=0,4,8.
All four solver settings retain GAC: search, decomposition, fixed-hit and both.
Feasibility asks whether any complete world satisfies the observations; full
projection returns every supported cell/state literal. Marginal supports are
not independent choices of complete worlds.

The primary analysis keeps two weightings separate:

- **Truth-weighted:** weight each image family by its number of legal truths,
  equivalent to uniform sampling over legal worlds within a placement/suite.
- **Observation-family-weighted:** give each distinct image family one vote
  within that placement/suite.

Both then give placements and suites equal weight at fixed k. Any overall
primary comparison cited below additionally weights k and palette equally.
The pooled optimization totals instead count every saved camera family once
across strata; they are a separate descriptive population.

Feasibility and projection timings use each case's median of three saved calls
per configuration; reported means average those medians. Direct support queries
time each of the 24 cell/state literals once per configuration on every 97th
camera family and all controls. This deterministic query sample is selected
separately per palette and is not a
truth- or family-population estimator. Query UNSAT means an unsupported literal,
not necessarily an infeasible scene.

Oracle/reference construction, witness audits, root diagnostics and output are
outside solver timing; propagation/search, counters and budget checks are inside.
Whole-process memory includes oracle/reference data and is not solver-only
memory. The preserved [protocol](../results/a2-scaling-8cell/protocol.json),
[provenance](../results/a2-scaling-8cell/provenance.json) and
[artifact index](../results/a2-scaling-8cell/README.md) specify the exact collection.

## Incomparable-cell count and arrangement

The measured **primary truth-weighted 8×8 full-projection branch means with
both optimizations** are:

| k | Mixed material | Same material |
| ---: | ---: | ---: |
| 0 | 0 | 0 |
| 1 | 0 | 0 |
| 2 | 0.001736 | 0.010055 |
| 3 | 0.093750 | 0.162375 |
| 4 | 0.004630 | 0.036458 |
| 5 | 0.479681 | 0.686686 |
| 6 | 0.770576 | 1.128587 |
| 7 | 1.297258 | 1.960086 |
| 8 | 1.919753 | 2.986105 |

Work rises overall toward k=8 but is **not monotone in k**. At k=3, masks 7, 25,
44 and 224 have mixed-palette means of 0, 0.375, 0 and 0; same-material means
are 0, 0.613812, 0.025656 and 0.010031. The k=4 dip persists under both primary
weightings. Thus arrangement changes difficulty at fixed k, and the chosen masks
do not provide a complete nested comparison between adjacent k values.

The observation-family-weighted k=8 projection means are 1.691413 mixed and
2.029430 same-material. They answer a different weighting question from the
truth-weighted means above. All k values and configurations remain in the
[primary evidence table](../results/a2-scaling-review/by-k-primary.csv).

Configuration also matters at low k. Search-only projection averages 2.378255
truth-weighted branches at k=0 in both palettes, whereas fixed-hit/both require
none. The zero-branch entries above are not claims about every solver setting.

At k=8, full projection costs more than finding one feasible world in the
reviewed summaries. With both optimizations and primary truth weights:

| Palette and API | Mean µs | Median µs | p95 µs | Mean nodes | Mean branches |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mixed feasibility | 6.833 | 6.791 | 9.905 | 1.830 | 0.400 |
| Same feasibility | 7.883 | 7.551 | 12.529 | 2.183 | 0.657 |
| Mixed projection | 16.144 | 12.408 | 48.714 | 7.102 | 1.920 |
| Same projection | 23.241 | 17.266 | 56.956 | 9.689 | 2.986 |

## Occupancy: a secondary sensitivity analysis

Uniform legal truths change the expected scene density as k increases: selected
cells have occupancy probability 2/3, other cells 1/2. Primary expected occupancy
is **4+k/6**, rising from **4 to 5⅓ cells**. Consequently the primary k trend is
not a clean isolated effect of incomparable-cell count.

The already-reviewed occupancy-balanced sensitivity analysis holds expected
occupancy at four cells by assigning occupancy probability 1/2 at every cell,
with equal slab-half probabilities conditional on occupancy at selected cells.
It is purely a **secondary post-hoc reweighting of existing measurements**, with
no additional cases or solver execution.

At k=8, its projection branch means with both optimizations are **0.940908 mixed**
and **1.392619 same-material**, retaining **49.0% and 46.6%** of the corresponding
primary means. Branch growth above the optimized k=0 baseline survives, as does
the k=4 dip. This supports sensitivity to the occupancy distribution and shows
that balancing does not remove the observed pattern. It does **not** establish
that occupancy causally explains the removed fraction, or isolate k's causal
effect. These secondary numbers are not substituted into the primary conclusions.
The [sensitivity evidence](../results/a2-scaling-review/by-k-occupancy-sensitivity.csv)
remains separate.

## Geometry, material information and image families

The same-material control removes the stone/oak distinction while preserving
the incomparable slab geometry, legal truth population, masks and cameras.
The physical path retains correlated shape choices and globally unsupported
values after local propagation; the triangle remains globally infeasible despite
nonempty GAC domains. Both occur with identical oak labels. Incomparable geometry
therefore creates genuine global coupling without requiring material-color
differences; the accepted solver's residual search addresses that incompleteness.

Material labels nevertheless supply useful information. For example, at k=8
under primary truth weights and both optimizations, axis_2 projection means are
1.404664 branches mixed versus 6.980643 same-material. With one axis view, both
palettes instead average 10.074074 branches. The label benefit depends on what
the cameras reveal, and does not eliminate geometric coupling.

Relabeling stone as oak merges image families: the 8×8 corpus has **207,278 mixed**
families versus **167,493 same-material** families. Truth-weighted comparisons
refer to the matched legal-world population; raw family-weighted comparisons
refer to different populations of images. That distinction must accompany any
cross-palette family average.

Restricted/perturbed cases are separate robustness controls. In particular,
observation-label cycling can introduce impossible stone observations into the
oak-only palette. Those cases are excluded from clean palette comparisons and
primary camera summaries.

## Visibility and ambiguity are central to search behavior

All **24,002 tested 16×16 six-axis families at k=0,4,8** are singleton families.
Feasibility and projection use one node and zero branches in every configuration;
all **24,384 sampled direct support calls** for those scenes are also branch-free.
These are stronger observations of the same eight-cell volume, not larger scenes.

With both optimizations, all **285,880 singleton 8×8 families** are branch-free.
Only **9,851 of 88,891 ambiguous families** require branching, so most ambiguous
families also require none. Ambiguity is therefore not sufficient to force
search. Conversely, stronger visibility can eliminate search entirely in the
tested six-axis 16×16 scenes, even when every cell admits incomparable geometry.

More observations need not reduce runtime. At k=8, the same-material axis_3 and
axis_6 summaries have identical ambiguity and branch work, but mean projection
times of 11.170 and 18.128 µs. Root residual-component size also does not alone
predict work: some roots reach eight cells, while the worst projection has four
two-cell components. Root diagnostics are not full descendant search traces.

The supported interpretation is that visibility, arrangement and observation
ambiguity are essential explanatory variables alongside k. Their causal ranking
is unresolved. Nothing here implies that increasing image resolution alone will
solve larger reconstruction problems.

## Fixed-hit and decomposition

Across the **374,771 pooled 8×8 camera families**, each counted once across
strata, the reviewed full-projection totals are:

| Configuration | Branches | Nodes | Sum of case-median runtimes, s |
| --- | ---: | ---: | ---: |
| Search | 97,453 | 616,443 | 3.963700 |
| Decomposition | 97,453 | 656,931 | 3.951813 |
| Fixed-hit | 47,739 | 596,314 | 3.946784 |
| Both | 47,883 | 621,866 | 3.952743 |

Fixed-hit removes **51.0%** of pooled projection branches; both removes **50.9%**.
The corresponding sums of case-median runtimes decrease only **0.43% and 0.28%**,
both less than 0.5%. In the overall primary comparison with k and palette equally
weighted, both versus search reduces branches/time by 70.7%/5.1% under truth
weights and 57.2%/1.6% under family weights. Weighting affects the magnitude;
the recorded timing differences remain modest relative to branch reductions.

**Fixed-hit is effective at reducing search work in this corpus, but a reliable
general wall-clock speedup is not established.** Its benefit is aggregate, not
universal: fixed-hit increases projection branches in 1,120 camera families.
The option also changes branch-variable eligibility, so its measured effect is
not an isolated evaluation of the shortcut routine alone. Three interleaved
trials on one recorded host/run do not provide independent timing replication.

Decomposition alone saves **no branches case by case** in the measured camera
corpus or sampled camera queries at either resolution. Its added 8×8 projection
nodes equal its 40,488 component solves. Both options together can have more
nodes than search alone despite fewer branches; projection includes component
entries and several support searches with witness reuse.

The intentionally disconnected path-plus-triangle UNSAT control demonstrates
decomposition's mechanism: in both palettes, feasibility falls from **10 nodes /
9 branches** to **8 nodes / 5 branches**, with lower saved case-median times.
Two satisfiable paths retain four branches and incur node/time overhead instead.
Decomposition is therefore demonstrated on targeted separable cases, not accepted
as a generally beneficial optimization. Branch reduction, node reduction and
wall-clock improvement are distinct outcomes.

## Worst cases, status and budgets

The worst observed full projection reaches **61 nodes / 32 branches**, with
both optimizations at **k=8, mask 255, one-axis view** in both palettes. The
representative truth is 3320; the family has 16 worlds, 16 supported literals
and four two-cell residual components. The branch count aggregates several
support searches rather than one irreducible eight-cell search.

The slowest individual saved call is **8.136 ms**, in a different, branch-free
case: mixed-material search-only projection at k=4, mask 90, oblique_2. Its
three-trial median is 11.667 µs. The highest-search call and slowest call being
different is another reason not to treat branches as a direct runtime proxy;
the precise cause of the timing tail was not measured.

There are **0 UNRESOLVED outcomes in 12,624,840 saved calls**. Each call had a
limit of **1,000,000 search-node entries or 60 seconds**, with exhaustion
represented explicitly as `UNRESOLVED`, never as infeasible. Observer and
post-return deadline checks are not a hard process kill; the attempted entry
that triggers an abort can be counted. The observed calls remain far below
the limits and do not characterize behavior near exhaustion.

Camera families are SAT by construction. Of the restricted/perturbed cases,
1,039 are SAT and 2,417 UNSAT; all those UNSAT cases are rejected by root GAC.
The structured controls contain eight SAT and four UNSAT cases, with the latter
retaining nonempty local domains. Hard UNSAT behavior is therefore represented
only by targeted tiny controls, not a broad camera-derived UNSAT population.

## What the experiment establishes

**Directly measured:** branch, node and time distributions on the declared
reconstructed eight-cell corpus; palette differences under explicit weights;
optimization work counts; visibility/resolution controls; the separate
occupancy-balanced sensitivity results; and zero unresolved calls. The
[detailed review and evidence](../results/a2-scaling-review/README.md) retain
the full distributions, query qualifications and integrity records.

**Supported interpretations:** incomparable geometry can leave global coupling
after GAC; material information can reduce ambiguity and search; the occupancy
distribution affects the weighted k trend without eliminating its growth under
balancing; arrangement and visibility materially affect observed search; and
fixed-hit substantially reduces aggregate search work in this corpus. The
occupancy sensitivity is not a causal decomposition, and none of these statements
ranks the factors' causal importance.

**Still unresolved:** this experiment does not establish exponential complexity,
practical Minecraft-scale performance, useful scaling beyond eight cells, a clean
causal effect of k, a reliable general runtime benefit from fixed-hit, general
usefulness of decomposition, or which factor ultimately dominates at larger
volumes. Four masks per intermediate k, six synthetic suites, finite perturbations
and one host/run cannot answer those questions. The model assumes exact known
cameras/grid and opaque synthetic first-hit observations; it does not test noise,
lighting, textures, transparency or unknown cameras.

These conclusions belong solely to the canonical **reconstructed exploratory
dataset**. They neither validate the lost original protocol nor recover its
reported population. The scientific review reproduced the preserved numerical
tables within saved rounding without numerical corrections; this final report
only synthesizes that completed review. Production code, tests, frozen Python,
raw evidence, analysis tables and charts remain unchanged. No solver execution,
benchmark, analysis rerun, correctness gate or sanitizer ran for this report.

## Next experiment

The next study should increase volume modestly while controlling separately for
occupancy, incomparable-cell count, spatial arrangement/connectivity,
visibility/view count, palette/material information, and SAT versus UNSAT
structure. It should include connected and disconnected structured cases and
independent timing repetitions, with explicit budgets and unresolved accounting
and oracle costs kept separate. The protocol should be declared before collection.
That larger-volume experiment has not been designed in detail or begun here.
