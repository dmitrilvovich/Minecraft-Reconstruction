# A2 larger-volume scaling protocol v1

**Preregistered design only; collection has not started.** This protocol follows
the [eight-cell report](a2-scaling-8cell.md) and [scientific review](a2-scaling-review.md)
at `81c3f38e07d59e981ce9273d287d3cc225cb47b0`. The accompanying
[machine-readable specification](../experiments/protocols/a2-scaling-larger-v1.json)
fixes the generator rules, seeds, counts, ordering, references and thresholds.
Neither a collector nor experimental inputs are produced in this checkpoint.

The existing 491,199-case dataset remains the canonical **reconstructed
exploratory eight-cell dataset**: not the recovered 410,953-case run, not proven
equivalent, and not a verified correction or superset. Its historical count is
not a sampling target here. This is a new prospective study informed by its
reviewed findings, with a different, explicitly controlled truth population.

## Question and frozen implementation

How does exact inference change as the number of cells and the size of globally
coupled structure increase, with occupancy, geometry, observations and material
information controlled separately where possible? The principal comparisons are
volume at fixed k=2; volume at fixed k/N; k at fixed volume and occupied positions;
and matched changes of palette, views, resolution and arrangement. Camera cases
and abstract structured cases answer different questions and will not be pooled.

The accepted A2 solver remains the implementation accepted at
`e73b710a42bca1cd0c7a69b9b9c37db2595a1cb1`. Retain search, decomposition, fixed-hit
and both, all with GAC. Freeze minimum-domain selection, greatest residual-factor
degree tie-breaking, then lowest cell ID, with air/bottom/top value order.
Fixed-hit's existing restriction to relevant shape-choice variables remains
part of that configuration. No heuristic tuning, extra solver baseline or
production instrumentation is proposed. A GAC/root-graph diagnostic is recorded
separately, not treated as a fifth solver configuration.

Inspection found that `Grid`, both A2 renderers, `A2Problem` and the solver use
variable cell counts. The old collector and validation adapter hardcode eight
cells, 32-bit packed masks and particular resolutions. A later collector must
use variable-length domain/state arrays and a new reference adapter; it must
not silently extend that packing or edit the frozen Python reference. Exact
camera bases and the solver/reference subtree identities are pinned in the JSON.
Use GCC C++20 with `-std=c++20 -O3 -DNDEBUG`, without PGO, LTO, `-march=native`
or fast-math. Pin the actual compiler version and binary, build command and host
before timing; do not change them between volumes or modes.

## Volumes and observation controls

| Role | Cells | Dimensions (x,y,z) |
| --- | --- | --- |
| Primary progression | 4, 6, 8, 10, 12 | (2,2,1), (3,2,1), (4,2,1), (5,2,1), (6,2,1) |
| Compact controls | 8, 12 | (2,2,2), (3,2,2) |
| Deferred stress point | 27 | (3,3,3); outside v1 collection |

The primary sequence preserves a two-cell vertical cross-section and increases
depth along the first, minus-x camera's viewing direction. Compact controls
separate aspect-ratio effects at equal cell count. This is not a claim to isolate
cell count from every geometric change, nor a representative sample of arbitrary
3D buildings. The 2×2×2 control uses a small new matched cohort because occupancy
and timing sessions differ from the old study; the old corpus is not recollected
wholesale and its timings are not mixed into new repetitions. Four saved physical
path/triangle palette fixtures serve as additional compatibility anchors.

Use the unchanged `phase_a_cameras(grid, R)` rig: grid-center target, distance
3 times the longest side L, focal lengths 3R/2 and principal point (R/2,R/2).
The six suites are the original nested axis_1, axis_2, axis_3, axis_6, oblique_1
and oblique_2. Axis and oblique comparisons at equal view count control pose;
the nested axis suites control added observations. Record all exact rational
cameras, raw rays and factor-deduplication mappings.

Resolution is a separate factor:

- **Primary nominal sampling-density regime:** R=8L, namely 16–48 pixels per
  side. With the existing distance rule this holds nominal pixels per cell
  approximately constant, not information or visibility constant.
- **Fixed image-budget control:** R=16 for the same truths, masks and suites.
- **Higher-resolution control:** R=16L (32–96), axis_1 and axis_6 only, truth
  draw indices 0–3. This subset is chosen before results.

When R=16 coincides with R=8L, measure that input once and record both cohort
memberships. Never count it twice within a summary. Report regimes separately:
view count and resolution are not interchangeable, and strong-view success must
not obscure weak-view failures.

## Deterministic camera population

For N cells use the distinct k values **{0,2,N/2,N}**. Intermediate k has four
mask recipes: a connected breadth-first prefix, a greedy spread-out prefix,
and two independently hashed random orders. Each recipe supplies nested masks
as k grows. Endpoints k=0,N have one recipe. Record mask connectivity, but do
not equate face adjacency with the solver's residual connectivity.

There are **16 truth draws per geometry/k/mask-recipe/occupancy-target stratum**.
The occupancy targets are 1/4, 1/2 and 3/4. Every draw has an exact occupied-cell
count q; when N times the target is fractional, the JSON's balanced integer
rounding fixes which draws use floor versus ceiling. The cross-volume headline
comparison uses half occupancy, exactly q=N/2 at every v1 volume. Low/high
profiles report their actual q values rather than concealing rounding differences.

Choose occupied positions uniformly by a deterministic pseudorandom ranking,
then choose bottom/top with equal probability on occupied incomparable cells;
other occupied cells are bottom slabs. Occupancy ranks and latent shape bits
are shared across k, mask recipes, palettes, views and resolutions. Thus changing
k does not change q or the occupied positions. Occupancy is a property of the
generating truth, **not a cardinality constraint given to inference**. Candidate
worlds and reference enumeration must still include all worlds allowed by the
input domains: air/bottom/top at selected cells and air/bottom elsewhere.

Master seed is `mcr-a2-larger-v1-20261001`. All choices use the specified SHA-256
ranking of compact ASCII JSON tuples, with cell-ID tie-breaking; no library RNG
or outcome-dependent rejection sampling is used. Draws are with replacement.
Repeated masks, truths or images retain their design memberships and timing
slots, including collisions between mask recipes. They are not silently replaced
by novel or harder cases. Pair palettes by truth ID, domains, cameras and draw,
not by the lexicographic rank of an observation family.

For draw 0 of each camera stratum, at primary resolution and axis_1/axis_6,
add two separate robustness cases: deterministic truth-preserving domain
restrictions and a contradictory duplicate of the first nonempty ray. The
duplicate changes background to oak, or any foreground to background, retaining
the original observation. Its UNSAT status is independently known. No stone
label is introduced into the oak-only palette. Neither class enters clean
palette comparisons; these contradictions are easy controls, not hard UNSAT.

## Structured coupling and ambiguity

At N=6,8,10,12 construct nine abstract ray-factor families: a path; an even
cycle; an odd (N−1)-cycle with a leaf; repeated disconnected two-vertex paths;
two paths of N/2 vertices; a path plus a disjoint triangle; a fixed-hit region;
a shape-coupled path plus a disjoint fixed-hit region; and a path with one
orientation fixed by a unary observation. Each has four predeclared vertex
permutations and two factor orders, paired across palettes. The JSON defines
the edges, domains, observations and ordering completely.

An edge uses one bottom-only ray and one top-only ray through its endpoints,
requiring opposite slab choices. Bipartiteness and component orientations give
an independent exact reference; an odd cycle is UNSAT in either palette.
Fixed-hit regions use air/bottom domains and one oak first-hit factor. The unary
path control changes observation strength while keeping initial k, occupancy
and graph construction fixed; it deliberately changes ambiguity and the graph
remaining after propagation. Record those changes rather than claiming an
isolated causal effect of ambiguity. These graph cases are abstract factor
instances, not asserted to arise from one physical camera rig. The four legacy
physical anchors retain that distinction. None proves exponential complexity.

Camera residual components and ambiguity are measured outcomes, never admission
criteria. Record their distributions and post-stratify descriptively into
singleton/ambiguous/unknown and connected/disconnected/empty residual graphs.
Do not select or discard camera cases to fill bins after observing GAC or search.
Structured known ambiguity provides controls without filtering random cases.

## Measurements and independent repetitions

Use **five separate timing epochs**, with fresh worker processes and independently
shuffled order, on one recorded host with one pinned CPU and no concurrent solver
jobs. Repeat the whole fixed workload, not just fast or difficult cases. Pair
palette and mode order as specified in the JSON; record all trials without
outlier deletion. Process setup, input construction and loading are outside the
API timer. No unrecorded solver warmups are permitted. These are independent
process sessions, not independent hosts; retain session effects and uncertainty.

Each slot receives feasibility and full projection in all four modes per epoch.
Sample direct support queries on draw 0, primary resolution, axis_1/axis_6,
all robustness controls and all structured/anchor slots: all three states of
two hash-selected cells, fixed across palettes/configurations. Include excluded
states and report this deterministic query sample separately from population
estimates. Query UNSAT denotes an unsupported literal, distinct from scene UNSAT.

Record API time, nodes, branches, GAC/factor work, literal queries, returned
witness counts, component solves/decompositions, fixed-hit guard checks,
rejections and successful uses. Record root component-size vectors, largest
component, shape variables, exact supported-literal counts and family size where
independently available. Full projection is complete-support certification;
standalone feasibility measures work to one witness or an UNSAT proof. The
accepted API exposes no exact internal first-witness timing boundary: do not
subtract separate call times and label the difference a certification phase.
Root diagnostics are not a descendant trace.

Oracle, rendering, audits, root diagnostics and I/O have separate cost records.
Inference workers contain no oracle corpus. Record worker peak RSS, input sizes
and the responsible job, **not solver-only memory**; subtracting baseline RSS
would not cleanly isolate allocations. No solver-only memory claim is planned.

## Reference coverage

| Scope | Mandatory independent evidence |
| --- | --- |
| Every sampled camera/control input with N≤8 | Exhaust all legal domain worlds; compare feasibility and complete supports. |
| N=10/12 camera inputs | Twelve preselected palette pairs per geometry, covering every k and density target, plus one restricted pair; selection fixed in JSON. Other cases have partial external coverage. |
| Every structured case | Independent parity/component and fixed-hit reference; exhaustive cross-checks at N≤8 and canonical instances at N=10/12. |
| Every returned witness at every size | Original domains and original observations, checked independently of solver propagation/search. Projection witnesses must cover exactly its returned supports. |

The exhaustive bounds at full k are 81, 729, 6,561, 59,049 and 531,441 worlds
for 4,6,8,10,12 cells. Enumeration is small enough to require full sampled-case
coverage through eight cells; 10/12 use the declared audits rather than promise
all-case exhaustive validation. No 3^27 enumeration is proposed.

Camera references must construct intersections from raw rational rays and
primitive AABBs independently of the solver traversal. A later adapter can use
the frozen generic Python `reference_ray` without editing it, with explicit
material-code translation. Stream/cache enumeration outside timing; do not feed
truths, reference supports or oracle pruning into the solver. Check every source
truth with both C++ rendering paths. For structured inputs use a separate scalar
first-hit evaluator and the graph reference, not the solver automaton.

Cross-configuration agreement is required but **is not proof of correctness**.
Outside the exact audits, witnesses validate positive claims; unsupported literals
and UNSAT answers lack an independent exhaustive certificate unless covered by
the structured/contradiction proof. Label that coverage per case. Mandatory
reference checks cannot be dropped because they become expensive: pause instead.

## Budgets, censoring and decision rules

Keep **1,000,000 search-node entries and 60 seconds per API call**, including
the entire projection, not a fresh budget per internal query. The eight-cell
study did not approach these limits, so increasing them now would weaken the
comparison without evidence. Preserve observer checks and the post-return
deadline check; an attempted aborting entry can make the saved counter 1,000,001.
A 65-second external watchdog and a 4 GiB inference-worker ceiling guard against
missed callbacks or resource exhaustion. Budget/watchdog/memory exhaustion is
`UNRESOLVED`, never UNSAT. Missing final counters stay missing.

Unattempted, invalid and interrupted records have separate coverage flags; they
are not completed calls. A projection stopped early supplies no certified exact
support mask. Report unresolved rates beside resolved-only latency/work
distributions and censored observations. Do not replace unresolved values by
zero, discard them, or call the 60-second bound a completed runtime.

The following are **engineering policy thresholds**, not complexity tests or
universal usability claims. The operating cohort is the primary progression,
half occupancy, k=N/2 and N, axis_2/axis_3, R=8L, each palette separately, with
both optimizations. A rate denominator is design case slots, not timing calls;
at final review a slot fails if any epoch is unresolved. Evaluate feasibility
and projection APIs and
geometry/k/view/palette strata separately; never dilute failures with six-view
successes. Use nearest-rank percentiles of per-slot medians across five completed
epochs, equal recipe/draw weights; no readiness pass on censored/incomplete data.
With only 16 truth draws per recipe, these are finite-design screening criteria,
not precise population failure-rate estimates. Zero observed failures would not
exclude rare difficult worlds.

- **Integrity stop:** any invalid witness, exact-reference disagreement,
  contradictory resolved configuration answers or completed-repeat work mismatch
  stops collection immediately. Do not encode implementation errors as UNSAT or
  ordinary budget failures. Solver changes require a new experiment version.
- **Architecture review:** pause expansion if an operating stratum has at least
  5% unresolved slots, p95 projection time above 5 s or feasibility above 1 s,
  or p95 nodes at least 100,000. Also flag matched N=12 versus primary N=8
  growth above 8× when p95 nodes reach 10,000, or when p95 time reaches 1 s
  for projection / 0.1 s for feasibility. The node-ratio denominator is at
  least one; do not infer an exponential law from a ratio.
- **Memory review:** pause on a worker peak above 2 GiB, or matched 12/8
  growth above 4× with the larger worker above 512 MiB. This diagnoses the
  practical inference process, not solver-only allocation growth.
- **Possible next research direction:** only complete 10/12-cell operating
  cohorts with every declared external audit complete, zero unresolved slots
  (including their sampled support queries), p95 feasibility
  ≤0.1 s, p95 projection ≤1 s, maximum nodes <100,000, worker peaks ≤512 MiB,
  no growth flag, and all structured cases resolved with both options justify
  considering a bounded camera/lattice-recovery prototype without first changing
  the solver. This is not authorization to start it or evidence for larger scenes.
  An intermediate result means review before expansion, not an automatic pass.

Process volumes in ascending N. After a complete first epoch at a volume,
apply the unresolved/absolute-node/absolute-time/absolute-memory thresholds; if
triggered, pause the whole remaining collection, not only difficult cases or
modes. All growth ratios and readiness use all five epochs. An administrative
cap of 48 timed solver-hours and 12 preparation/reference/audit wall-hours also
pauses collection
with the entire manifest and missing coverage preserved. Reference timeouts
(300 s per required exhaustive audit) are unresolved validation tasks, not
evidence that those cases are correct. No resampling or automatic budget increase.

The 27-cell point is deferred to a separately published prospective extension.
Retain the same per-call limits as its starting proposal; at least 10% unresolved
projection cases in its eventual declared moderate-visibility operating cohort
would count as substantial failure and require an architecture review. Its
population and cost must be declared before any 27-cell execution.

## Weighting, size and publication

Primary results give equal weight to the 16 truth draws within each fixed
design stratum, then equal mask-recipe weight at fixed N/k/occupancy/view/
resolution/palette. This is the defined **occupancy-first truth distribution**,
not uniform sampling over all legal worlds and not the old truth weighting.
Report each factor level separately; the volume headline uses exact half
occupancy and the nominal sampling-density regime. Draw pairing is preserved
in differences. No grand mean mixes resolutions, palettes, controls or volumes.

An auxiliary **observed-family-weighted** summary first groups equal original
domains and full image signatures within the same mask/stratum, averages their
draw measurements, then gives observed families equal weight. It does not
estimate the complete family population from unobserved images. Palettes can
collapse different numbers of families. Report unique input counts and retained
draw multiplicity; neither timing repeats nor duplicate resolution memberships
become new independent experimental cases.

| Planned workload | Design slots |
| --- | ---: |
| Camera cases, primary and fixed-image regimes | 66,816 |
| Higher-resolution controls | 3,168 |
| Restricted/contradictory controls | 1,584 |
| Abstract structured cases | 576 |
| Legacy physical anchors | 4 |
| **Total before any stopping** | **72,148** |

This is **3,240,640 planned timed API calls** over five epochs, including
354,720 sampled direct-query calls, plus up to one untimed root-GAC diagnostic
per slot and separate references/audits. Counts retain repeated draw/recipe
memberships; they do not assert that every input is distinct. As workload
arithmetic, an average timed call of 1/10/100 ms would cost approximately
0.9/9/90 solver-hours before preparation and process overhead. These are scenarios,
not measured forecasts. All calls consuming 60 s would exceed 54,000 hours;
the administrative cap prevents that plan from silently expanding. Reserve
20 GiB storage; a storage shortfall pauses rather than truncates evidence.

Before later execution, implement and inspect the new collector/reference adapter,
record compiler/flags/host/CPU and enforcement support, freeze executable/source
hashes, and commit the complete input manifest before any solver or root-GAC
evaluation. Archive exact inputs, protocol membership, every attempted call,
validation coverage, logs and closed-file hashes with atomic completion markers.
Changes after seeing results require a new version stating what was already
observed; never overwrite v1 evidence or tune cases/heuristics in place.

The v1 population and decision rules are fixed for the collector. Remaining
preflight questions are implementation capacity
and the execution host's enforcement/timing environment, not permission to alter
the design after results. The 27-cell extension remains an explicit later design
question. No solver, benchmark, input generation, correctness gate, sanitizer
or camera recovery ran for this protocol checkpoint.
