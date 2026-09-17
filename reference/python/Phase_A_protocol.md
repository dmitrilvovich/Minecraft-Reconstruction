# Phase A: an exact known-camera reconstruction oracle

This protocol tests the discrete reconstruction mathematics. Cameras, the grid,
the bounded volume, geometry, and image formation are exact. It contains no
camera recovery, image recognition, learned proposals, structural priors,
Minecraft exporter, or application architecture.

`phase_a.py` is a runnable implementation of the tiny correctness experiment.
The final section below is filled from its measured results. The larger scaling
gates described here are **proposed next experiments**, not completed benchmarks.

## 1. Frozen worlds and vocabulary

Use

\[
V=\{0,1\}^3,\qquad |V|=8.
\]

Cell indices are lexicographic `(x,y,z)`, with `z` changing fastest. Coordinates
refer to unit cells `[x,x+1) × [y,y+1) × [z,z+1)`; `y` is vertical.

The first vocabulary is

\[
D_0=\{0=\text{air},1=\text{stone cube},2=\text{oak cube}\}.
\]

There are exactly **3^8 = 6,561** worlds. Every assignment is legal in this
synthetic model, including floating cubes. There are no gravity, adjacency,
symmetry, compactness, or material-smoothness constraints.

Do not start full enumeration at 3×3×3: **3^27 = 7,625,597,484,987** worlds.

In this experiment, stone and oak are two constant, distinct optical labels.
They are not vanilla texture assets. This deliberate simplification tests
visibility and discrete inference without photometric modeling.

## 2. Cameras, pixels, and exact boundary rules

The image is 8×8 samples. Intrinsics are `fx=fy=12`, principal point `(4,4)`,
zero skew, no distortion. A pixel center is `(a+1/2,b+1/2)`, with integers
`a,b` in `0,...,7`.

Let `f,r,u` be the forward, horizontal, and vertical camera basis, respectively.
The world ray is

\[
c+t\left(f+\frac{2a+1-8}{24}r+\frac{2b+1-8}{24}u\right),\quad t\ge0.
\]

The vertical pixel-coordinate convention is the direction of `u`; no display
flip is implicit. Each basis is orthonormal and satisfies `r × u = f`.

The primary camera sequence is:

| Name | Center `c` | Forward `f` | Horizontal `r` | Vertical `u` |
|---|---|---|---|---|
| minus_x | (-5,1,1) | (1,0,0) | (0,0,-1) | (0,1,0) |
| minus_y | (1,-5,1) | (0,1,0) | (1,0,0) | (0,0,-1) |
| plus_z | (1,1,7) | (0,0,-1) | (-1,0,0) | (0,1,0) |
| plus_x | (7,1,1) | (-1,0,0) | (0,0,1) | (0,1,0) |
| plus_y | (1,7,1) | (0,-1,0) | (1,0,0) | (0,0,1) |
| minus_z | (1,1,-5) | (0,0,1) | (1,0,0) | (0,1,0) |

Test every prefix of this sequence: 1, 2, 3, 4, 5, and 6 views. These are nested
observation sets, so exact supported-state sets can only shrink as views are
added.

Also test one oblique camera and its opposite pair:

* Oblique: `c=(-3,5,3)`, `f=(2,-2,-1)/3`, `r=(1,2,-2)/3`, `u=(2,1,2)/3`.
* Opposite: `c=(5,-3,-1)`, with `f` and `r` negated and `u` unchanged.

All calculations use rational coordinates. The oracle intersects the ray with
axis-aligned boxes using exact rational interval arithmetic. A hit requires a
positive-length intersection. A zero-length edge/vertex tangency is a miss.
For a ray parallel to a box face, membership uses the half-open interval
`lo <= coordinate < hi`. The boundary convention is part of the renderer, not
an epsilon chosen by the solver.

The finite sampling resolution is intentional. It can cause genuine
observational aliasing. A 16×16 or 32×32 rerun is a separate resolution control;
it must not be described as fixing inference errors in the 8×8 model.

## 3. Exact observation model

Each pixel contains exactly one of `{0=background,1=stone,2=oak}`. It records
the material of the nearest positive-length opaque geometry intersection.
No intersection means background. Background is a known constant distinct
from both materials.

There is no shading, antialiasing, texture filtering, noise, transparency,
lighting dependence, or tolerance threshold. Consistency means equality of
every supplied pixel. Depth, cell IDs, surface normals, and visibility masks
are not observations. In particular, the oracle must not reveal which cell
generated a pixel to the inference procedure.

The oracle and inference geometry computations are independent:

1. **Oracle:** ray/AABB interval intersection for each candidate primitive;
   select the nearest active primitive by exact intersection-depth order.
2. **Inference:** intersect the ray with all volume grid planes, order the
   rational events, and use interval midpoints to identify traversed cells.
   For slabs, split the interval at the half-height plane and test the resulting
   subintervals. Construct a ray automaton from these traversals.

For every enumerated world and all 512 raw camera pixels, assert that these
two forward computations agree. This checks the entire feasible-world relation,
not just a few recovered scenes or marginal domains.

Identical ray functions may be deduplicated, but only when the ordered cells
and their full state-to-emission tables are identical. This is lossless in this
model. Duplicate observations with conflicting colors are infeasible and must
not be silently merged.

## 4. Generating structures and observations

For the tiny volume, enumerate **every world as a possible truth**. For each
view set, partition worlds by their exact observation signature. A signature's
equivalence class is its complete feasible-world set. Solve each tested class
once; solving it once per generating truth would duplicate work.

The implementation tests all observation classes for the 1-, 3-, and 6-axis-view
suites when the corresponding `--all-a0` / `--all-a1` flag is enabled. For each
remaining suite, it tests 128 seeded, uniformly selected observation classes
plus a deterministic adversarial catalog. The seed is `20260917`.

The catalog includes empty, uniform stone, uniform oak, single occupied cells,
front and back sheets, a front stone sheet with oak behind it, alternating
materials, and an open ray corridor. The geometry extension adds slab sheets,
full/slab alternation, and slabs in front of differently colored cubes.

Enumeration also covers configurations not named in the catalog. Test selection
is not a prior used by inference. Report separately:

* metrics averaged uniformly over all possible truth worlds;
* metrics on the selected observation classes;
* deliberate adversarial cases.

In A1, also stratify oracle metrics by the number of slabs in the true world.
The zero-slab stratum uses exactly the same true cube worlds as A0, while the
inference vocabulary is larger. This separates hypothesis-space expansion
from changing the distribution of true scenes.

## 5. What the exhaustive oracle computes

For observations `I`, compute

\[
F(I)=\{X\in D^V:\mathcal R(X)=I\}.
\]

For every cell/state, store its exact occurrence count

\[
n_{v,s}=|\{X\in F(I):X_v=s\}|.
\]

If `F` is nonempty:

* `n=|F|`: the state is forced;
* `0<n<|F|`: the state is possible but not forced;
* `n=0`: the state is impossible.

The exact supported-state domain is `S_v={s:n_(v,s)>0}`. A cell is identifiable
iff `|S_v|=1`. The whole world is identifiable iff `|F|=1`.

If `F` is empty, report **INFEASIBLE**. Do not label every state forced using
vacuous universal quantification.

### Completely unconstrained has a stronger definition

A full marginal domain `S_v=D` does not establish that the cell is independent
of the observations. Define a freely variable cell by

\[
F(I)=D_v\times\pi_{-v}F(I).
\]

Equivalently, for every feasible assignment of all other cells, replacing this
cell by **any** state in its original allowed domain leaves the observations
unchanged. Check this by grouping feasible worlds by all coordinates except
`v`: each group must contain every state of `v`.

Example: one ray traverses `A` then `B` and observes stone. The feasible pairs
are `(stone,air)`, `(stone,stone)`, `(stone,oak)`, `(air,stone)`. Cell `B` has
all three marginal states, but is not freely variable: when `A=air`, `B` must
be stone. Once `A=stone` is fixed, `B` becomes freely variable.

Being hidden in the chosen generating world is another, different property.
It does not establish observational freedom over every feasible explanation.

## 6. A0: the fixed-geometry elimination algorithm

Maintain domain bitsets, non-air domains `M_v`, and an active candidate set
`U={v:M_v is nonempty}`. Some cells may be forced absent or forced present by
an explicit feasibility query. Initially, every state is allowed.

For each ray, maintain its frontmost active cell whose fixed geometry is hit.

```
initialize a queue with every ray
while the queue is nonempty:
    take a ray r
    advance its front pointer past inactive/missed candidates
    if no candidate remains:
        fail if r does not observe background
        otherwise continue
    v = the first remaining hit cell
    remove from M[v] every material whose emission differs from observation[r]
    if M[v] becomes empty:
        fail if v was explicitly required to be present
        otherwise remove v from U and revisit affected rays
```

Background mismatches eliminate the current foreground candidate and expose
the next one. They do not cause one unchecked vote for air at every depth.

At termination, choosing all active cells and any retained material at each
cell gives a feasible witness. Remaining non-air states are exactly supported
under the fixed-geometry assumptions. Air support requires an explicit test:
force `X_v=air` and rerun the complete feasibility procedure.

The implementation also runs exact per-ray generalized arc consistency (below)
to remove locally impossible air alternatives before these probes. The probes
complete the state-support calculation; no combinatorial branching is required
for A0.

### Invariants and proof obligations

1. Every feasible world's occupied cells lie inside the current `U`.
2. Every feasible non-air label remains in its cell's material domain.
3. If `v` is the first hit in the maximal candidate scene `U`, any feasible
   subworld containing `v` also exposes `v` on that ray. Hence intersecting its
   material domain with the observed label is sound.
4. Removing a cell can only expose later candidates. Non-air label changes do
   not change hit geometry in this restricted problem.
5. At a fixed point, the maximal candidate witness satisfies every ray because
   material choices are independent once hit geometry is fixed.

The maximal witness is not an assertion that every retained cell truly exists.
The domain projections and the ray constraints must both be retained.

## 7. Compact first-hit constraints and exact local propagation

For ray `r` and traversed cell `v_j`, define `e_(r,j)(s)` to be zero for a miss,
or the material label for a hit. Cell order is geometry-independent because
every permitted shape stays inside its owning unit cell.

For target foreground label `b`, use a two-state automaton:

* **alive:** no surface has yet terminated the ray;
* **done:** an acceptable first hit has occurred.

From alive, a miss stays alive, a hit with color `b` goes to done, and a hit
with another color rejects. From done, every state is allowed and stays done.
At the end, accept done for foreground observations and alive for background.

Use forward reachable states and backward accepting states to determine whether
each cell value lies on an accepting path. Delete a value only if it has no
such path through that factor under the current domains. Revisit affected ray
factors until no domain changes. This is generalized arc consistency (GAC).

Each ray visits a cell at most once, so the automaton's local support calculation
does not accidentally treat repeated occurrences of one variable as independent.

A GAC fixed point is precisely defined. It is **not** the result of every
conceivable sound propagation rule, and it need not equal the global supported
domains. That gap is one of the measured quantities.

## 8. Audit every pruning operation

For each tested observation class and every query/search branch, condition the
oracle family on the branch's current domain restrictions:

\[
F_{\rm branch}=\{X\in F(I):X_v\in D_v\text{ for every }v\}.
\]

Before a deletion `s from D_v`, assert

\[
\nexists X\in F_{\rm branch}:X_v=s.
\]

Before reporting a contradiction, assert `F_branch` is empty. A branch choice
is recorded as a conditional assumption, not a global exclusion of its siblings.

The audit is read-only and is excluded from measured solver timings. It never
supplies domains, branch choices, or supports to the solver. A second, unaudited
run measures solver work and must reproduce the audited result.

The regression suite deliberately injects an invalid deletion of an occluded
oak state and asserts that the audit rejects it. It also tests contradictory
duplicate-ray observations and exact boundary conventions.

Validate returned witnesses by membership in the oracle class. Compare every
final supported-state domain with the exhaustive domain. Combined with the
all-world renderer/constraint equivalence check, this establishes much more
than reconstruction accuracy on one chosen truth.

## 9. Introducing exactly one geometry-changing state

Use

\[
D_1=D_0\cup\{3=\text{oak bottom slab}\}.
\]

The slab at `(x,y,z)` occupies

\[
[x,x+1)\times[y,y+1/2)\times[z,z+1).
\]

It has the **same observed color as the oak cube**. No new material label
reveals the geometry. Only one state is added, keeping every A0 scene available.
There are exactly **4^8 = 65,536** worlds.

Do not run the original fixed-geometry procedure on a cube/slab mixture as if
both shapes had the same hit pattern. However, **A1 is still polynomial under
this exact observation model, even with different materials**. The stronger
reason is nested geometry:

\[
G_{\rm air}\subset G_{\rm slab}\subset G_{\rm full}.
\]

### A stronger complete procedure: nested-geometry envelopes

At every cell, form the union of all currently allowed geometries. Because
the geometries are nested, this envelope is the geometry of an actual remaining
state. Full stone and full oak tie in geometric size.

For each ray, find its first hit cell in the envelope scene. At that cell,
delete every state that **hits this ray and gives the wrong color**. Keep a
state that misses this ray: a later cell may explain the observation. Revisit
affected rays as domains and envelopes shrink. A domain becoming empty is a
contradiction. An escaping ray with a foreground observation is also a
contradiction.

This pruning is sound: earlier envelope cells cannot hit the ray, so no feasible
earlier block can hide a wrong-color state that hits the current cell. Geometry
contained in each owning cell ensures the state's hit remains earlier than
hits in subsequent cells, even when its surface is deeper than the envelope's.

At a fixed point, select a state with maximal remaining geometry at every cell.
For each ray, its first selected hit is exactly its envelope hit. Every remaining
maximal-geometry state at that cell has passed the ray's color test. Therefore
the simultaneous selections form a legal feasible witness. Ties between full
cube materials do not invalidate this argument: an incompatible tied material
would already have been removed by a visible ray.

There are finitely many state deletions and every update is polynomial. Running
this complete feasibility procedure under each singleton condition `X_v=s`
computes exact globally supported domains without combinatorial branching.
The implementation uses GAC at the root and this procedure for the remaining
state-feasibility queries.

This is a real limitation of the proposed search-growth test: **adding one
contained partial shape to full cubes leaves the geometries nested**. Search
performed by a generic solver on A1 is avoidable. A1 tests the stronger theorem
and its implementation; it does not demonstrate a need for exponential shape
search. The `--a1-mode search` option retains the generic solver as a comparison.

### Residual coupled components

Create a bipartite graph of non-entailed ray factors and unresolved variables
that can affect them. Discard variables beyond a guaranteed earlier hit, and
variables whose emissions are identical across all remaining states for that
ray. Keep a conservative dependency graph; this need not be the smallest
semantic decomposition.

Record component sizes for all scene variables and separately count variables
that still admit both a full cube and a slab. Air/non-air uncertainty is a
separate count. A connected component of size eight is not, by itself, evidence
that eight exponential decisions are necessary.

### Same-material control

Also test `air`, `oak bottom slab`, `oak full cube`, all occupied surfaces with
the same constant color. Geometry is nested:

\[
\varnothing\subset G_{\rm slab}\subset G_{\rm cube}.
\]

After background exclusions, selecting the largest remaining geometry at every
cell maximizes foreground coverage without changing any hit's color. If that
maximal assignment cannot satisfy a foreground ray, no smaller assignment can.
This gives polynomial feasibility and exact supports through singleton queries.

Therefore, this three-state experiment can have many ambiguous, coupled cells
and still need **zero search branches**. A1 has the same zero-branch guarantee
under the broader envelope argument. Both controls prevent equating shape
ambiguity or a large graph component with computational hardness.

### A2: the smallest useful contrast for actual shape search

To exercise the missing assumption, use a separate three-state palette:

\[
D_2=\{\text{air},\text{oak bottom slab},\text{stone top slab}\}.
\]

The top slab occupies `[x,x+1) × [y+1/2,y+1) × [z,z+1)`. The two non-air
geometries are incomparable. Their union is a full cube, which is not an
allowed state in this palette. A domain envelope can therefore be a phantom
shape with no legal representative, so its consistency no longer proves
feasibility. There are only **3^8 = 6,561** worlds.

A2 is an explicit contrast beyond the requested one-new-state extension. It
is necessary to test actual coupled shape choices under these assumptions.
It does not expand the Minecraft vocabulary toward an application.

The declared generic solver for A2 is:

1. Propagate exact ray GAC.
2. If every remaining non-air state at each cell has the same hit/miss behavior
   on all relevant rays, use the fixed-geometry feasibility procedure.
3. Remove entailed ray factors and split independent residual components.
4. Otherwise branch on the smallest-domain active cell, then highest active
   ray incidence, then cell index. Try states in numerical order.
5. Return the first witness for a feasibility query; exhaust all branches before
   declaring the query impossible. Use witnesses to certify other cell states.

No timeout is interpreted as impossibility. The tiny correctness run has no
search cutoff.

### A geometric adversarial fixture that GAC cannot solve by itself

Use three active cells `A=(0,0,0)`, `B=(1,0,0)`, `C=(0,0,1)`. All other cells
in the 2×2×2 bound are explicitly fixed to air. Enumerate their 27 assignments.

For each pair, use one lower ray at height `h=1/4`, observing oak, and one upper
ray at `h=3/4`, observing stone. These are exact central rays of one-pixel
pinhole cameras:

| Pair | Ray origin | Direction |
|---|---|---|
| AB | (-1,h,1/2) | (1,0,0) |
| AC | (1/2,h,-1) | (0,0,1) |
| BC | (3,h,-1) | (-1,0,1) |

The BC ray traverses B then C and touches the other cells only at a zero-length
grid-corner event. Each pair of observations requires its two cells to occupy
opposite slab halves.

* **Path fixture, pairs AB and AC:** exactly two worlds are feasible. Root GAC
  nevertheless retains air at all three cells. This gives three unsupported
  surviving values and a concrete distinction between ambiguity and incomplete
  propagation. Exact support queries remove air, while preserving both slab
  choices and their correlations.
* **Triangle fixture, all three pairs:** no world is feasible, because three
  binary slab choices cannot be pairwise opposite. Root GAC still retains all
  three states at each cell. The exact solver must prove infeasibility.

The path observations come from valid worlds. The triangle deliberately combines
mutually inconsistent observations and is labeled as a negative test, not as a
rendered truth. `adversarial_fixtures.py` verifies both with an independent
geometric oracle and the pruning audit.

## 10. Metrics and their interpretation

For every tested observation case record:

| Metric | Definition |
|---|---|
| Initial candidates | Sum of original domain sizes: 24 in A0, 32 in A1 |
| Root candidates removed | Initial candidates minus candidates after the declared root propagation |
| Unsupported survivors | Root values absent from the exact global supported-state set |
| Exact candidates | Sum of globally supported domain sizes |
| Identifiable fraction | Cells with exactly one supported state, divided by eight |
| Freely variable fraction | Cells satisfying the product/freedom definition, divided by eight |
| Feasible-world count | Exact size of the reference equivalence class |
| Residual components | Number, sizes, maximum size; all variables and shape variables separately |
| Literal queries | Number of actual global state-feasibility queries |
| Query outcomes | Feasible versus infeasible; never replace timeout with infeasible |
| Search nodes | Recursive solver invocations, including query roots and component subcalls |
| Branches | Actual child state assignments attempted; this excludes independent query assumptions |
| Per-query search | Maximum nodes for a single query and for a single infeasible query |
| Propagation work | Calls, ray updates, state deletions, fixed-kernel invocations |
| Agreement | Exact world-relation, supported-state, and witness checks |
| Audit work | Every individual domain-reduction step checked against the oracle |
| Time | Oracle/render setup, audit validation, and unaudited inference separately |
| Memory | Current executable records whole-process peak RSS, including the oracle |

The prototype's peak RSS is **not** solver-only memory. Isolate reference and
solver processes before interpreting memory scaling. Its single-run timings
are useful smoke measurements, not a hardware-independent performance claim.

For scaling, add median/p95 times, peak solver RSS, time to first witness,
time to certify unsupported states, and timeouts. Report branch counts both
per case and per query. The measured nodes are those used by this particular
algorithm, not a proven lower bound on necessary search.

## 11. Distinguishing ambiguity from incomplete inference

If a value remains in the propagated domain:

* Oracle support exists: the value is genuinely possible.
* Oracle support does not exist: propagation was incomplete.
* The exact solver has a witness: possibility is certified.
* The exact solver exhausts all branches: impossibility is certified.
* A solver budget expires: the value remains computationally unresolved.

Multiple feasible worlds are not reconstruction mistakes. Selecting the exact
generating world from an indistinguishable class is not a valid success metric.
Conversely, surviving domains alone do not certify possibility until supported
by the complete fixed-geometry theorem or an actual global feasibility result.

## 12. Proposed scaling experiment and concrete stopping gates

The tiny experiment establishes correctness. It cannot establish useful
asymptotic scaling. The next experiment keeps the same renderer and vocabulary.

Retain A0 and A1 as polynomial controls at every size. For the search-growth
experiment use A2. On eight cells, vary the number of cells permitted to use
both slab orientations from 0 through 8; the other cells allow air and one
fixed slab orientation. This is an explicitly controlled domain family, not
a claim inferred from images. Its reference size is `3^k * 2^(8-k)`. Test
multiple spatial arrangements of those cells, not just one convenient order.

Then increase volumes through 2×2×1, 3×2×1, 2×2×2, 5×2×1, 3×2×2, 4×2×2,
and 3×3×3. Full A2 enumeration is practical at 4, 6, 8, 10, and 12 cells
(3^12 = 531,441). At 16 cells there are 43,046,721 assignments; use a separately
budgeted chunked oracle for selected cases if feasible. At larger sizes use
certified search and renderer witness checks, and do not claim a full exhaustive
oracle was run.

For each size and view count, include at least 100 fixed-seed cases split among
random occupancy/slab assignments, thin occluders, repeated layers, alternating
slab orientations, and differently colored surfaces visible through slab gaps.
Include satisfiable and infeasible extensions of the pair-ray fixtures; do not
assume an odd cycle alone establishes exponential complexity. Preserve 1-, 2-,
3-, and 6-view suites and oblique controls.

Keep approximate pixel density per block constant as volume size changes:
camera distance `3*max(shape)`, resolution `4*max(shape)`, focal length
`1.5*resolution`. Repeat selected cases with four times that resolution to
separate low-information sampling from computational difficulty.

Compare declared, fixed baselines: feasibility search without propagation,
ray GAC plus search, and GAC plus the fixed-geometry procedure and component
decomposition. Do not retune heuristics separately for each favorable case.
Count all lookahead work if singleton/failed-literal propagation is added.

Preregister these gates:

1. **Correctness failure:** any unsound deletion, false infeasibility, false
   witness, or supported-state mismatch fails the implementation immediately.
   Minimize and save the counterexample. This falsifies a theorem only if the
   implementation actually satisfies that theorem's stated assumptions.
2. **Promise insufficient:** passing the eight-cell cases permits only the next
   known-camera scaling experiment. It does not justify application work.
3. **Search-scaling failure:** stop treating general exact global search as the
   main route if at least 10% of the fixed 27-cell A2 tests with
   three informative views require over 10^6 search nodes or 60 seconds to
   certify state supports, and the growth trend is already visible at smaller
   sizes. Treat the constants as project engineering budgets, not theorems.
4. **Exponential trend:** a median certified-search node count rising by at least
   4× per four extra residual coupled shape variables over three successive
   size steps is a warning; combined with the budget failures above, it is a
   reason to abandon this global strategy rather than add block types.
5. **Weak-decomposition failure:** if most unresolved variables remain in one
   growing component and the certified-query costs grow exponentially despite
   GAC and the fixed-geometry procedure, the proposed decomposition has not
   delivered its intended advantage. Component size alone does not trigger this
   gate; the same-material control demonstrates why.
6. **Information limitation:** if the oracle says many cells remain ambiguous,
   report that. Test higher resolution or a discriminating view. Do not call
   missing information a solver failure or introduce a completion prior.

If the exact global strategy fails the scaling gate, keep the validated renderer,
constraints, and oracle. Change the inference strategy to bounded components,
surface proposals, or explicit unresolved outputs. Do not expand the architecture
to conceal the failure.

## Running and reproducing the implemented experiment

Requires Python 3 and NumPy. From the extracted folder:

```bash
python phase_a.py --output results --all-a0 --all-a1 --all-a2 --samples 128
python adversarial_fixtures.py
```

Use `--samples 4` without the exhaustive-class flags for a faster smoke run.
Enumeration of worlds and all-world renderer-equivalence checks remain complete;
only the number of observation classes passed through the inference solver changes.

`results.json` records the exact arguments, Python/NumPy versions, environment,
whole-process memory, and summary metrics. `cases.csv` records the tested
observation classes and detailed inference measurements. Random seeds affect
case selection only, not renderer or solver semantics.

## Recorded results

Final 8×8 run: **PASS**. This is a correctness result for the declared tiny model, not a scaling result.

* 93,439 observation cases passed exact supported-state comparison.
* 43,632,128 raw ray/world outputs agreed between the independent renderer and the ray-constraint construction.
* 1,482,343 domain-reduction steps were checked against branch-conditioned exhaustive families.
* All observation classes were tested for the 1-, 3-, and 6-axis-view suites in A0, A1, and A2. Other suites use the documented class samples and catalog.
* The contradictory-observation, correlation/freedom, boundary, and deliberately-unsound-deletion regressions passed.

| Model | Enumerated worlds | Tested observation cases | Maximum branches per case | Maximum nodes per individual query | Maximum unsupported values after root GAC/propagation |
|---|---:|---:|---:|---:|---:|
| A0 | 6,561 | 9,525 | 0 | 0 | 0 |
| A1 | 65,536 | 71,410 | 0 | 0 | 0 |
| Amono | 6,561 | 1,127 | 0 | 0 | 0 |
| A2 | 6,561 | 11,377 | 30 | 10 | 8 |

The generic ray-search baseline initially used up to 51 branches per A1 observation case. The stronger nested-geometry procedure returned the same exact supports with zero branches. Those branches were an algorithmic choice, not inherent shape-search complexity.

Identifiability below is the average fraction of identifiable cells, uniformly over the same 6,561 cube-only true worlds. A1 nevertheless allows slabs as alternative explanations:

| Axis views | A0 allowed palette | A1 allowed palette, cube-only truths |
|---:|---:|---:|
| 1 | 11.11% | 11.11% |
| 2 | 46.30% | 41.98% |
| 3 | 73.86% | 69.99% |
| 4 | 84.75% | 81.54% |
| 5 | 90.26% | 86.15% |
| 6 | 93.87% | 91.04% |

The 16×16 resolution control enumerated all worlds for A0, A1, and Amono and tested a smaller inference-case sample. With six axis views, **100% of cells were identifiable for every world in each of those three palettes**. This demonstrates that the remaining six-view ambiguity at 8×8 was sampling ambiguity in this particular tiny setup. It does not establish identifiability for larger volumes. The resolution control used the generic A1 search baseline; the image/oracle result does not depend on which inference algorithm is used.

For the three-cell fixtures, the path has two feasible worlds but GAC retains three globally impossible air values. The triangle has zero feasible worlds although GAC retains every state. Exact feasibility took 3 nodes / 2 branches for the path and 4 nodes / 3 branches for the triangle; support queries and every pruning step matched enumeration.

The main A2 suite also contains genuine local/global gaps: up to eight root-domain values were unsupported globally; 628 infeasible state-support queries were certified. This goes beyond checking recovery of a generating world.

Runtime reporting:

| Model | Complete phase time, including oracle and audit | Median unaudited inference per tested case | Maximum unaudited inference per tested case |
|---|---:|---:|---:|
| A0 | 6.85 s | 0.212 ms | 1.446 ms |
| A1 | 62.71 s | 0.302 ms | 7.348 ms |
| Amono | 1.65 s | 0.264 ms | 1.841 ms |
| A2 | 11.91 s | 0.379 ms | 7.758 ms |

Whole-process peak RSS was 158.88 MiB. This includes the exhaustive oracle and all retained benchmark records; it is not a solver-only memory measurement.

**Decision:** retain the known-camera core and proceed to the declared A2 scaling study. Do not infer scalable performance from at most eight active cells. The requested one-slab extension is polynomial in this model and cannot by itself test whether general shape search explodes.
