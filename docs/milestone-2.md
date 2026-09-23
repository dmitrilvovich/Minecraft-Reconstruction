# C++ Milestone 2 — A1 acceptance record

**PASS.** The C++20 implementation provides exact feasibility, simultaneous
witnesses, requested state support and whole-scene supported domains for the
declared nested A1 model, with zero combinatorial search. Correctness and
benchmark evidence agree with exhaustive enumeration and frozen Python.

This record closes the milestone from published benchmark checkpoint
`d2eda1664ba0eeb7c7739e6efea6f0ea2d26da8b`. The full correctness gate was published
as `7b93b8e7b32ee22ef40583903da185b87dcd658b`. This closeout changes documentation
and reconciliation evidence only. Production code, tests, benchmark code, raw
measurements and the frozen reference remain unchanged. The release marker is
`cpp-milestone-2`, targeting the final documentation commit containing this record.

## Mathematical scope

The volume is exactly 2 x 2 x 2 unit lattice cells, with vocabulary

\[
D_1=\{\text{air},\text{stone full cube},\text{oak full cube},\text{oak bottom slab}\}.
\]

There are 4^8 = **65,536** unrestricted worlds. The bottom slab occupies the lower
half of its cell in the vertical y coordinate. Both oak states emit the same oak
label. Floating structures are legal; no gravity, adjacency or structural prior
is imposed. Input restrictions are arbitrary per-cell subsets of this palette.

Cameras, intrinsics, poses, lattice registration, volume and renderer are known
exactly. The accepted camera corpus uses 8 x 8 pixel centers, six successive axis
views and one/opposite-pair oblique views from the frozen protocol. Observations
are only the first opaque hit's background/stone/oak label, with no depth or
generating-cell identity. There is no noise, shading, texture, filtering or
rendering mismatch. Half-open faces and positive-length intersections determine
boundary behavior; zero-length tangencies miss. Calculations use checked exact
64-bit rational arithmetic. Numeric overflow throws rather than implying
infeasibility; arbitrary numeric range is not claimed.

For observations I and original domains D, the reference family is

\[
F(I,D)=\{X:\ X_v\in D_v\ \forall v,\ \mathcal R(X)=I\},\qquad
S_v=\{s:\exists X\in F(I,D),\ X_v=s\}.
\]

The experiment accepts equality of this feasible relation and its exact marginal
supports, not merely recovery of a generating scene.

## Implemented capabilities and completeness

| Layer | Accepted behavior |
| --- | --- |
| Geometry/rendering | Independent ray/AABB and lattice traversal paths; bottom-slab intersection, cell order, first hits, occlusion and boundaries |
| Ray constraints | Compact two-state first-hit automata; exact local state supports |
| Shared propagation | GAC over common cell domains; sound reductions and stable successful fixed points |
| Complete feasibility | `envelope_feasible`: correct feasible/infeasible decision under restrictions and one simultaneous witness when feasible |
| Requested support | `query_support`: singleton conditioning, complete feasibility, and a witness containing the requested cell/state |
| Exact projection | A1 `exact_supports`: globally supported masks plus witnesses collectively covering every reported state |
| Audit | Read-only, assumption-aware deletion/contradiction auditing; no oracle decisions supplied to inference |

The complete method relies on containment:

\[
G_{air}\subset G_{slab}\subset G_{cube}.
\]

Stone and oak cubes tie geometrically. In every nonempty domain, the union of
allowed geometries is represented by an actual maximum-geometry allowed state.
All shapes stay in their owning cells, so cell order on a ray is independent of
state choices. At the first envelope hit, no earlier allowed cell can hide a
state that hits with the wrong material. Deleting only those wrong hitting states
is sound; states that miss remain possible. Shrinking domains cannot create an
earlier hit, so cached ray fronts advance monotonically. Empty domains and
foreground rays with no remaining hit certify contradictions.

At a fixed point, choose a maximum-geometry state in every cell. These choices
simultaneously realize the envelope scene, and every first hit has the required
material. Material ties cannot invalidate the witness: an exposed wrong-material
tied state would already have been removed. This establishes complete feasibility,
rather than treating a local-consistency flag as a global existence proof.

Exact projection certifies a root witness, then uses complete singleton queries
for remaining uncertified root literals and reuses their witnesses. Every retained
literal has a feasible scene; every excluded literal has a sound root deletion
or conditioned infeasibility. Assumptions in one query never become unconditional
deletions in another. With n cells, m ray factors, L total ray steps and K retained
root literals, the fixed four-state kernel has O(n+m+L) work per feasibility call
and projection needs at most 1+K-n such calls on a feasible root. This is a
structural zero-search result under these assumptions, not just a zero counter
on easy examples. See the [feasibility](milestone-2b-feasibility.md) and
[projection](milestone-2b-projection.md) proofs and API details.

## Correctness acceptance

The complete 2C gate covers every feasible observation family of all eight
declared suites. Each of the 65,536 truth worlds participates in every partition.
The independent C++ AABB corpus and frozen Python agree on complete family
membership and state-occurrence counts, not only final masks.

| Check | Recorded evidence |
| --- | ---: |
| Renderer agreement, all 512 raw rays x all worlds | 33,554,432 ray/world tuples |
| Automaton acceptance decisions | 14,352,384 |
| Complete camera observation families | 195,620 |
| Family membership comparisons | 524,288 |
| Cell/state occurrence-count comparisons | 6,259,840 |
| Complete-freedom count comparisons | 195,620 |
| Successive-axis support-containment checks | 2,621,440 |
| Restricted/altered full-camera inputs | 512, including 421 infeasible |
| Named adversarial scene/view pairs | 312, also replayed with duplicated/reversed constraints |
| Audited full-camera pipeline calls | 196,756 |
| Audited domain reductions / state deletions | 10,909,842 / 15,197,542 |
| Audited contradiction events | 1,493,828 |
| Independently verified projection witnesses | 346,958 |
| Independently verified standalone feasibility witnesses | 196,335 |
| Independently verified direct-query witnesses | 54,697 |
| Projection-supported literal certificates covered | 1,724,491 |
| Direct-query comparisons | 212,628 |
| Unsound deletions, false contradictions, support mismatches | 0 |
| Search branches / nodes | 0 / 0 |

Component gates supplement full-camera coverage:

| Component | Native exhaustive/audited coverage | Frozen Python agreement |
| --- | --- | ---: |
| Single ray | 98,304 domain/label combinations | 98,304 |
| Shared propagation | 90,322 runs | 12,288 |
| Complete feasibility | 75,747 calls; 16,584 witnesses | 10,125 decisions/witnesses |
| Requested support | 194,134 queries; 17,482 witnesses | 64,800 decisions/witnesses |
| Whole-scene projection | 13,760 projections; 194,192 direct-query checks | 10,125 projections |

These include all 4,096 domain triples in three abstract shared-ray scenes,
empty masks, every relevant factor order, and a six-physical-ray corpus containing
all 729 label vectors under two domain variants. Unattainable observations,
excluded requested states, unused cells, zero-cell problems, correlated ambiguity,
rank ties, slab/full occlusion, shrinking envelopes, contradictory duplicate rays
and deliberate audit faults are covered. Boundary validation includes 8,918 rays
and 153 focused geometry samples. Existing A0 acceptance remains intact.

Counts have distinct scopes and must not be summed as independent worlds. The
full-camera gate's 196,756 calls comprise 195,620 families, 512 restricted cases
and 624 named-scene replays. Benchmarking omits those 624 replays, explaining its
196,132 inputs and 344,634 projection witnesses versus the gate's 346,958.
Audited contradiction/deletion totals include conditioned queries, not only root
scenes. Release and sanitizer copies are repeated validation of the same corpus.

The full **28/28 Release** and **28/28 ASan/UBSan** correctness gates passed.
The sanitizer gate initially passed 27/28; its truncated generated reference
fixture was restored and the affected comparison passed. Logs preserve that
failure and recovery. LeakSanitizer was disabled in the traced runtime; leak
freedom is not claimed. The later benchmark harness passed six focused Release
checks and a sanitizer run on 16 camera families plus all 512 restricted inputs.
Its one-row output rollover was checked separately. This is not a claim that the
post-benchmark 29-test suite was fully rerun. No expensive suite was repeated for
this documentation closeout; saved evidence and source hashes remain intact.

## Ambiguity semantics

For a nonempty family, a state occurring in every world is forced; one occurring
in some worlds is possible; one occurring in none is impossible. A cell is
identified exactly when its supported mask is a singleton. A maximal witness is
one explanation, not a truth claim. Supported masks are exact marginals; their
Cartesian product generally contains infeasible combinations. The returned
witness collection covers literals, not all feasible worlds.

A freely variable cell satisfies F = D_v x projection_-v(F): every allowed value
works in every feasible context of the other cells. A full marginal mask alone
does not imply this independence. Infeasible inputs return an explicit false
feasibility flag, empty support masks and no witnesses; they are never described
as all states being forced by vacuous quantification. Restricting a singleton
already excluded by the input is an unsupported query, not a relaxed domain.

## Reconciliation: 91.04% versus 87.06%

**Both numbers are correct, for different truth populations.** The original
[Phase A protocol](../reference/python/Phase_A_protocol.md), immediately before
its identifiability table, specifies the same 6,561 cube-only truth worlds for
the A0/A1 comparison. A1 still allows slabs as alternative explanations. Its
91.04% is the zero-true-slab stratum, not the all-A1-world aggregate.

| Property | 91.04% figure | 87.06% figure |
| --- | --- | --- |
| Truth population | 3^8 = 6,561 worlds with no true slabs | All 4^8 = 65,536 A1 worlds |
| Allowed inference palette | Full four-state A1 | Full four-state A1 |
| Observations | Same six axis cameras, 8 x 8 pixels each | Same |
| Cell identification | Exact global support has size one | Same |
| Averaging | Uniform over the specified truth worlds, then eight cells | Same rule, larger truth population |
| Identified cell/truth pairs | 47,784 of 52,488 | 456,448 of 524,288 |
| Exact fraction | 1991/2187 = 0.9103795153177869 | 1783/2048 = 0.87060546875 |

For every truth T, compute supports over the **entire four-state family**
F(R(T), D1). The zero-slab condition filters evaluation truths only; it does not
filter feasible alternatives or tell the solver there are no slabs. Therefore
91.04% is also different from the A0 six-axis result, 93.87%, which disallows slab
explanations. Neither disputed number is an unweighted average over observation
classes or a restricted-domain benchmark statistic.

In `reference/python/results/results.json`, the A1 `axis_6` entry records both:
`oracle_by_true_slab_count["0"].identifiable_fraction` is 0.9103795153177869, and
`oracle_identifiable_fraction_uniform_world` is 0.87060546875. C++ records the
same values in `by_true_slab_count[0].identifiable_cell_fraction` and
`identifiable_cell_fraction`. The aggregate `phase_a_results.json` and earlier
generic-search baseline agree as well. The independent 16 x 16 resolution
control records 100% six-axis identifiability, so resolution is not the source
of the 91.04/87.06 difference. That control is historical Python evidence, not
a new C++ resolution benchmark or a claim about larger volumes.

The population decomposition is explicit:

| True slabs k | Worlds C(8,k) 3^(8-k) | Six-axis identified cells |
| --- | ---: | ---: |
| 0 | 6,561 | 91.03795% |
| 1 | 17,496 | 90.76932% |
| 2 | 20,412 | 89.12894% |
| 3 | 13,608 | 85.07496% |
| 4 | 5,670 | 76.93122% |
| 5 | 1,512 | 63.16138% |
| 6 | 252 | 43.65079% |
| 7 | 24 | 20.83333% |
| 8 | 1 | 0% |

Weighting these strata by their world counts gives 87.060546875%. This closeout
reaggregated all 195,620 saved camera rows, recomputed singleton counts from their
support masks, and checked all eight suites and 72 strata against both saved C++
summaries and frozen Python. It ran no solver or benchmark. Exact numerators,
denominators and input hashes are in
[`results/milestone-2/reconciliation.json`](../results/milestone-2/reconciliation.json).

The frozen prototype starts support inference with GAC; the accepted C++
projection starts with complete envelope feasibility and uses witness reuse and
conditioned queries. Thus root-domain sizes, query work and runtimes need not
agree. Standalone GAC is measured separately in C++. Global supports do agree.
Zero unsupported survivors after GAC in the unrestricted camera corpus is an
empirical observation, not a substitute for the complete feasibility proof.
Historical Python sampled some inference suites; its oracle identifiability
statistics nevertheless enumerate all truths before selecting inference cases.

## Accepted benchmark measurements

GCC 13.3.0, C++20 Release `-O3 -DNDEBUG`, Linux 6.18.44, AMD EPYC 9V74. The process
is single threaded; no CPU pinning or clock-overhead subtraction was applied.
One validation/warmup call precedes five timed complete support calls per input.
Timing distributions below weight classes equally and use each class's median;
identifiability/uniqueness weights truth worlds uniformly. The measures are
deliberately different. All 980,660 A1 timing samples are retained.

| Suite | Families | Median us | p95 us | Identified cells, all A1 truths | Unique world |
| --- | ---: | ---: | ---: | ---: | ---: |
| axis_1 | 625 | 4.146 | 6.030 | 12.50% | 0.0244% |
| axis_2 | 5,625 | 2.795 | 5.748 | 36.91% | 2.82% |
| axis_3 | 24,705 | 2.754 | 5.889 | 69.25% | 21.75% |
| axis_4 | 32,738 | 3.224 | 6.380 | 78.09% | 31.32% |
| axis_5 | 38,514 | 3.435 | 6.680 | 82.46% | 41.69% |
| axis_6 | 45,270 | 3.856 | 6.961 | 87.06% | 54.32% |
| oblique_1 | 11,353 | 3.855 | 7.311 | 66.94% | 4.68% |
| oblique_2 | 36,790 | 4.317 | 7.261 | 85.85% | 31.98% |

The 512 additional inputs comprise 91 feasible and 421 infeasible instances;
their median complete projections are 3.055 and 0.210 us respectively. The
largest camera-class median is 30.396 us; the largest individual repetition is
1,748.727 us. No timing outliers were discarded. One complete projection per
input totals 1,482,211 conditioned queries and 1,678,343 envelope calls. All
recorded search counters remain zero; repeated assumption checks are not search
branches. For axis_6, mean candidate counts are 32 initially, 15.660 after root
envelope feasibility, and 8.334 after exact projection.

Timed solver work includes input copying, allocations, internal checks and
witness construction; it excludes result destruction, rendering, problem setup,
corpus generation, exhaustive oracle calculations, audit callbacks, external
validation and output. Timed projections total 4.133 s within a 9.395 s C++
benchmark process. Setup, diagnostics and validation are separately accounted.
Python analysis/compression is outside both numbers.

Whole-process peak RSS is **21,952 KiB (21.44 MiB)**, including the exhaustive
corpus, family maps and allocator; it is not solver-only memory. Tmpfs output
storage, kernel memory and offline Python memory are not included. Same-host A0
measurements cover 29,032 classes with medians 2.193–8.232 us and whole-process
RSS 11,648 KiB. Different populations and algorithms prevent interpreting their
ratio as an isolated slab cost or language speedup. Historical Python timings
have different sampling/repetition/environment scopes and are contextual only.

The successful fourth collection produced 25 A1 gzip archives and one A0 archive
with raw/compressed hashes, row counts and reference checks. Three earlier
collections with truncated output were excluded in their entirety and documented.
They were not rejected for timing values. No source bug or weakened assertion
was involved. See [2D1 methodology and evidence](milestone-2d1-benchmarks.md) for
full distributions, memory scope, recovery details and reproduction commands.

## Evidence integrity and limits

The closeout verifies all 104 source/configuration/reference hashes and 69
evidence files in the benchmark manifest, all 32 evidence hashes in the 2C
manifest, and all eleven frozen reference files. The Release and sanitizer A1
acceptance reports remain byte-identical. The benchmark archive verifier and
the integer reconciliation use existing data only. These checks justify reuse
of the accepted validation; no source or data repair was necessary here.

The evidence is exhaustive over all worlds and feasible observation families of
the eight declared suites. It does not exhaust every camera, every contradictory
full image, or every one of the 16^8 eight-cell domain-mask assignments. Smaller
abstract domains and six-ray observation spaces are exhaustive; full-camera
restrictions are seeded tests. The mathematical argument supports the declared
nested model; these measurements establish neither large-volume runtime nor
general shape-search tractability. Identifiability is specific to the stated
truth distribution, views, resolution and palette.

This milestone does not provide incomparable-geometry inference, real-image
processing, camera recovery, expanded Minecraft block states, priors, schematic
export or a final application. No C++ A2 work begins in this closeout. The first
recommended A2 checkpoint is the minimal incomparable bottom/top slab geometry
and renderer/constraint foundation, reusing the frozen A2 oracle on 3^8 worlds
and an explicit phantom-envelope counterexample, before adding exact search.

The accepted known-camera core survives this gate. Earlier checkpoint records
remain historical snapshots; their then-pending work is resolved by this final
record, without rewriting their evidence or published history.
