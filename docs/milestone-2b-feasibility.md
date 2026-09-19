# Milestone 2B, checkpoint 3: complete A1 feasibility and one witness

Base: published `main` at `15e016d7e8f82cef35c7738c179877ffc2327df8`.
This checkpoint decides feasibility for the fixed A1 vocabulary under arbitrary
cell-domain restrictions and returns one actual simultaneous scene when feasible.
It does not compute all globally supported cell/state domains or finish A1 acceptance.

## API and implementation choice

`envelope_feasible(problem, domains, stats, observer)` in `a1_feasibility` returns
`A1FeasibilityResult { feasible, domains, witness }`. Inputs are copied. The witness
is present exactly when feasible, including an engaged empty world for a zero-cell
problem. Empty input domains prove infeasibility, even on unused cells. Malformed
domain counts throw instead of being reported as infeasibility. Failed calls have
no witness and retain diagnostic partial domains.

The reduced domains preserve all solutions but are not exact supported domains.
For example, a one-cell oak observation can leave `{air, oak cube, oak slab}` after
the envelope pass, although air cannot satisfy that observation. The returned oak
cube witness is valid; the retained air state is not a support claim.

The preserved interrupted envelope routine is reused in a dedicated feasibility
module. It uses the existing A1 scene, state ranking, shared work queue and typed
observer. Each ray caches a front position that only advances as envelopes shrink.
This differs from the frozen Python routine's repeated full prefix scans but has
the same mathematical updates. New counters record envelope calls and advances.

This kernel is complete on its own. It does not call GAC or run a singleton query.
Callers may first apply the validated GAC routine; tests also check that composition.
The GAC `consistent` flag retains its existing API meaning. The feasibility API
establishes its result through the envelope proof and an actual witness.

## Why the method is sound and complete

The current shapes satisfy

\[
G_{\mathrm{air}}\subset G_{\mathrm{slab}}\subset G_{\mathrm{cube}}.
\]

Stone and oak cubes have identical geometry. For each nonempty domain, a state of
maximum containment rank is an actual allowed state whose geometry contains every
alternative. A ray hits this envelope exactly when some allowed state hits it.
All shapes stay inside their owning cells, so the order of cells along a ray does
not depend on which states are chosen.

For each ray, locate its first envelope hit. Earlier cells miss for every allowed
state. At that cell remove only states that hit with the wrong observed material;
preserve every state that misses. Such a wrong hit cannot be hidden by an earlier
allowed state, so this deletion preserves every feasible world. Notify all incident
rays when a domain changes. An empty domain or a foreground ray with no possible
hit proves infeasibility. Domain shrinkage cannot create a hit in a skipped prefix,
which justifies the cached front positions.

At convergence choose a maximum-geometry state at every cell, with the existing
smallest-state-code tie rule. These choices realize all envelopes simultaneously.
Every foreground ray therefore reaches its stabilized first envelope hit, where
all remaining hitting states have the correct material. Every background ray
escapes. Thus the choices form one world satisfying every constraint. Material
ties cannot break this argument: a wrong-color tied state at an exposed first hit
would have been deleted. Sound failures plus a witness on every successful
termination establish complete feasibility.

The production kernel checks the witness against its final domains and all ray
constraints, throwing a logic error if that invariant fails. Tests independently
check membership in the original conditioned exhaustive family and, for physical
scenes, render the witness through independent ray/AABB intersections.

This proof uses opaque first-hit material observations, nested A1 shapes, fixed
cell order and per-cell input domains. It makes no claim for incomparable shapes,
extra relations between cells, or a renderer with different observation semantics.

## No combinatorial search

There is no branching, backtracking or assignment enumeration in production. One
simultaneous assignment is constructed after propagation. Enumeration is test-only.
If `m` is the ray count, `L` the total ray-step count, `deg(v)` the number of rays
touching cell `v`, and `delta(v)` its number of removed states, then each call obeys:

* front advances at most `L`;
* factor updates at most `m + sum_v deg(v) * delta(v)`;
* `delta(v) <= 4`.

Tests assert these bounds and zero branches, search nodes and support queries on
every audited case. Zero counters alone are not the completeness or complexity
argument. With four fixed states this implementation takes `O(n + m + L)` work
and `O(n + m)` additional space, excluding the stored problem and optional audit.
These are structural bounds; this checkpoint contains no scaling benchmark.

## Independent validation scope

* Existing three shared-ray scenes, all `16^3` domain triples (including empty
  masks), under all six factor orders. Each decision is compared with the entire
  feasible family obtained by independent scalar first-hit enumeration. Every
  returned witness must belong to that family under the original restrictions.
* Frozen Python `envelope_feasible` on all 10,125 nonempty-domain cases, checking
  both decisions and deterministic witnesses. Python expects nonempty masks;
  the C++ exhaustive checks separately cover empty inputs without modifying it.
* All 65,536 worlds on a `2 x 2 x 2` grid partitioned by six physical rays that
  cover all eight cells, with lower/upper, opposite and crossing directions.
  All 729 possible observation vectors are tested, including unrealizable ones,
  with full domains and deterministic restricted domains. AABB-rendered families
  and independent witness rendering provide the reference.
* 256 seeded four-cell cases, forward/reverse ray order, and selected GAC-prefilter
  composition checks. Named cases cover empty scenes/rays/domains, contradictions,
  same-material ambiguity, full-cube material ties, shrinking a cube to a missed
  slab, occlusion, incompatible duplicate observations, and correlated choices.
* The observer audits every reduction against the original conditioned family
  and independently computes its first-possible-hit justification, even when the
  global family is empty. Every contradiction is checked. Complete traces and
  statistics match unobserved runs. Fault injection tests bad deletions and false
  contradictions, including after resetting assumptions.
* Complete Release and ASan/UBSan regression suites retain A0 and all earlier A1
  gates. Shared test-corpus loaders are extracted without changing existing cases.

Both complete gates passed: **20/20 Release tests** (15.37 seconds) and
**20/20 ASan/UBSan tests** (181.77 seconds), with leak detection disabled in the
traced environment. There were 75,747 audited feasibility calls: 16,584 feasible
with independently verified witnesses and 59,163 infeasible. All 120,459 reductions
(145,441 removed states) passed their audits. Branches, search nodes and support
queries were zero in every call. The physical corpus had 450 realizable and 279
unrealizable full-domain observation vectors; restricted variants were also checked.

Recorded outcomes, commands and tested source hashes are in
[`results/milestone-2b-feasibility/`](../results/milestone-2b-feasibility/).
The frozen Python files and manifest remain unchanged. Test durations include
oracle generation and audits and are not inference benchmark measurements.

## Next smallest checkpoint

Add an exact query for one requested `(cell, state)` by conditioning its domain
and calling this complete feasibility kernel with a fresh audit context. Return
a supporting witness or prove that literal unsupported. Full supported-domain
projection, final A1 acceptance/benchmarks and milestone tagging remain later work.
No A2 code is included.
