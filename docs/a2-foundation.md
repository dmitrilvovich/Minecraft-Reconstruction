# A2 incomparable geometry: development foundation

This A2 development line establishes geometry, local propagation, complete finite
search, and checked reductions to independent or fixed-hit regions. It is not
full A2 certification or a scaling experiment. Accepted A0/A1 production code
and frozen Python are unchanged.

## Palettes and the control

`A2State` identifies air, bottom slab or top slab. `A2Palette` specifies the
materials; a world must be interpreted with its palette. A problem rejects
constraints constructed with a different palette.

| Palette | Air | Bottom half | Top half |
| --- | --- | --- | --- |
| `split_material` | background | oak | stone |
| `same_material` | background | oak | oak |

Both palettes have exactly three legal states and 3^8 = 6,561 worlds on the
declared 2 x 2 x 2 grid. The control changes only the top material, preserving
geometry, cameras and world populations. For corresponding truth worlds its
images are the split-material images with stone relabeled oak. It removes color
as a cue; it does not remove the mutually exclusive shape choice. For example,
a vertical oak observation distinguishes bottom from top in the split palette,
but permits either half in the control.

The frozen experiment remains the historical split-material reference. Nothing
under `reference/` was edited. The same-material extension is intentionally
separate from the earlier *nested* slab/full-cube monochrome control.

## Why the A1 proof stops

Bottom occupies `[y,y+1/2)` and top occupies `[y+1/2,y+1)`. Neither contains the
other. Their union is a full cube, which neither A2 palette admits. Thus a
domain's geometric envelope need not have a legal representative. Choosing
one equal-volume shape cannot realize that envelope simultaneously on all rays.
The A1 final-witness argument cannot be applied to these domains.

The physical fixtures use the frozen AB/AC/BC rays in
`reference/python/adversarial_fixtures.py`: three active cells, all other cells
fixed to air. Each pair has a lower and an upper ray, requiring opposite halves.
This relation holds in **both** palettes, even with identical oak labels.

| Fixture, either palette | Root GAC | Feasible worlds | Exact active domains | Feasibility nodes / branches |
| --- | --- | ---: | --- | ---: |
| Path AB, AC | air/bottom/top at all three cells | 2 | bottom/top at all three cells | 3 / 2 |
| Triangle AB, AC, BC | air/bottom/top at all three cells | 0 | empty | 4 / 3 |

On the path, the three surviving air values are globally impossible. The two
slab choices remain correlated; arbitrary combinations of marginal states are
not necessarily feasible. The triangle is an explicitly inconsistent observation
set, not a rendered truth. Its phantom envelope covers all requested rays even
though no legal assignment does. The split-material fixture counts match the
saved frozen `adversarial_results.json`; that Python suite was not rerun here.

These examples refute completeness of GAC and the A1 envelope argument for A2.
They do **not** prove that exponential search is unavoidable: the particular
opposite-pair graph can be decided by bipartiteness. Recorded branches describe
this baseline's policy, not an inherent lower bound.

## Implementation and exactness

Separate A2 types reuse `StateDomain`, the exact first-hit automaton and the
existing propagation queue. Independent render paths use per-state AABB
intersection versus grid traversal split at half-height events. Positive-length
intersections and half-open boundaries retain the established model semantics.

`a2_solver.hpp` exposes `gac`, `exact_feasible`, `query_support` and
`exact_supports`. GAC's flag is only local consistency. Exact feasibility runs
GAC at every search node, checks a concrete candidate world, and otherwise
uses the checked fixed-hit shortcut, splits independent residual components, or
conditions one unresolved variable. Residual scopes ignore entailed factors,
invariant emissions and cells behind a guaranteed earlier hit. With the shortcut
enabled, branching is restricted to cells with differing relevant occupied-state
hit patterns. Selection is minimum domain, then greatest active-factor degree,
then cell ID; values are tried in air/bottom/top order.

Every branch fixes an unresolved cell to one of its remaining values. These
disjoint branches cover all possible worlds at that node, and propagation only
removes locally impossible values. Depth is finite; exhaustion proves
infeasibility. A success returns a simultaneous witness. There is no timeout or
node cutoff that could be confused with infeasibility, and no polynomial runtime
claim. Optional `A2SearchOptions` can independently disable decomposition and
the fixed-hit optimization; disabling both retains the initial branching policy.
`A2SearchDiagnostics` counts splits, component solves and applicability checks.
The existing work counters count successful shortcut uses as `fixed_calls`.

### Sound residual decomposition

All analysis is recomputed from the current domains after GAC, including inside
branches and singleton support queries. For each ray, compute all possible
first-hit labels over the Cartesian domains. If the only possible label is its
target, the ray is entailed and can be omitted. Otherwise keep the prefix up to
and including the first cell whose every allowed state hits. Its tail stays
hidden under every subsequent domain restriction. Within that prefix, a cell
belongs to the scope only if its allowed states have different emissions.

Connect all variables in each retained scope. Every residual factor then
depends only on the variables of one connected component and constant context.
Scopes are conservative, not asserted to be the smallest semantic decomposition.
In particular, an optional occluder does not justify dropping a tail; and a ray
that still couples two regions keeps them in one component. An invariant shared
cell or an entailed bridge does not couple them.

Each component solve retains original cell IDs, original domains, and its own
ray factors. Out-of-scope cells remain in those factors as context. Only the
component's own variable values are copied into the assembled witness, so local
arbitrary assignments cannot overwrite sibling solutions. Ignored emissions
were invariant, hidden, or entailed over the parent domains, and stay so under
restrictions. Therefore the component witnesses compose. The implementation
also checks the assembled assignment against all original domains and rays.
One infeasible component rejects the entire query, including support requests
about a different, satisfiable component.

### Checked fixed-hit shortcut

For every cell in every non-entailed, potentially visible ray prefix, inspect
the **actual emission** of each allowed non-air state. All those states must
agree on hit versus miss for that ray. The bit may differ between rays; the
requirement is agreement among states on each relevant ray. This is not a check
of state names, geometric volume or material equality. Entailed rays and tails
behind guaranteed hits need no shape decision.

After this guard passes, the completed GAC fixed point already supplies the
necessary pruning. On any foreground ray, consider its first cell with a
possible hit. Earlier cells always miss. All allowed occupied states at that
cell hit, so a wrong-material one would lack local support and would have been
removed by GAC. Selecting any retained occupied state in every cell that permits
one therefore produces the correct first hit on every foreground ray. Background
rays can have no permitted hit after GAC. Entailed rays accept every assignment
in the current domains. This proves the construction is a simultaneous witness.

The shortcut constructs that witness and checks it against the complete current
problem. It does not introduce another pruning engine or convert equal-color
shapes into a shared geometry without checking their hit patterns. The path and
triangle fail the guard in both palettes and retain their original search
counts. Where a component still has differing hit patterns, only such cells need
branching; after those choices are resolved, the remaining region is fixed-hit.

Support queries intersect the original domain with the requested singleton.
Projection first establishes feasibility, then queries uncertified states and
reuses witnesses to cover further literals. Its masks are exact marginals.
Infeasible inputs return empty masks and no witnesses. Zero-cell and empty-domain
inputs have explicit coverage. Each GAC call resets the observer's assumptions
to that search node; final projection exclusions restore the root context.
Component event indices are translated back to the caller's original factors,
including through nested decomposition. The observer audits these reductions,
not a separately serialized search proof. Projection still uses complete
singleton queries and witness reuse; decomposition does not replace correlated
solutions by the Cartesian product of marginal support masks.

## Focused development checks

The focused runs used GCC 13.3.0, C++20, `-O3 -DNDEBUG`, compiling directly because
CMake was unavailable in the executor. All eight selected executables passed:
`a2_geometry_tests`, `a2_inference_tests`, `geometry_tests`, `a1_geometry_tests`,
`constraint_tests`, `a1_constraint_tests`, `inference_tests`, `adversarial_tests`.

The A2 checks include 870 named geometry samples, 32,768 sampled camera ray/world
comparisons, and A1 bottom-slab rendering compatibility. Inference checks include
all 512 restricted-domain triples for each physical fixture in each palette,
reversed/duplicated factors, disconnected paths with nested search, 320 seeded
four-cell factor cases, malformed inputs and independent audit fault injection.
Twenty-two additional named cases compare all four optimization configurations
against direct enumeration, including every support query and projected mask.
They cover independent feasible/infeasible components, shared invariant context,
entailed bridges, hidden tails, optional occluders, genuine cross-component
coupling, mixed fixed-hit/search regions, and graph changes under conditioning.
Observer checks also verify original factor IDs through nested component remapping.
Together: **2,470 feasibility/projection cases**, **54,720 direct support queries**,
**4,854 verified witness returns**, **25,192 audited reductions** and **57,908
checked propagation contradictions**. Every projected state is covered by a witness.
Physical witnesses belong to independently AABB-rendered feasible families;
abstract cases use direct first-hit enumeration. A small A1 end-to-end regression
also checks that its existing feasibility/projection/query APIs still use zero
search. These counts are focused development checks, not a full camera-family
acceptance claim.

Direct feasibility branch counts in the targeted comparisons:

| Case | Baseline | Decomposition only | Fixed-hit only | Both |
| --- | ---: | ---: | ---: | ---: |
| Fixed-hit two-cell ray, both palettes | 1 | 1 | 0 | 0 |
| Two satisfiable paths, both palettes | 4 | 4 | 4 | 4 |
| Path followed by independent triangle, both palettes | 9 | 5 | 9 | 5 |
| Fixed-hit region plus path, both palettes | 3 | 3 | 2 | 2 |
| Guaranteed opaque cell hides a bridge, split palette | 3 | 2 | 2 | 2 |
| Optional version preserves that bridge, split palette | 3 | 3 | 2 | 2 |

The path-plus-triangle case avoids repeating an impossible independent search
under alternative path solutions (four fewer branches). The two satisfiable
paths save no branches and use seven call nodes instead of five because component
entries count as nodes. These are deterministic focused work counts, not timing
benchmarks, scaling results, or a claim that every instance becomes faster.

Reproduce the focused checks with:

```sh
cmake -S . -B build/a2-development -G Ninja -DCMAKE_BUILD_TYPE=Release -DMCR_ENABLE_PYTHON_ORACLE=OFF
cmake --build build/a2-development --parallel 2 --target a2_geometry_tests a2_inference_tests geometry_tests a1_geometry_tests constraint_tests a1_constraint_tests inference_tests adversarial_tests
ctest --test-dir build/a2-development --output-on-failure -R '^(a2_geometry_tests|a2_inference_tests|geometry_tests|a1_geometry_tests|constraint_tests|a1_constraint_tests|inference_tests|adversarial_tests)$'
```

Push/PR CI now runs this focused set. The existing full Release/Python/sanitizer
gate remains available via manual workflow dispatch for milestone boundaries.
No new benchmarks, acceptance manifests, tags or sanitizer run were made here.

## Next checkpoint: full A2 correctness gate, before scaling

1. Enumerate all 6,561 worlds in each palette. Check all 512 established camera
   rays with independent AABB/traversal rendering and frozen Python functions
   called through an external adapter (6,718,464 ray/world/palette comparisons).
   Keep the frozen reference unchanged when supplying the same-material palette.
2. Partition the worlds by observations for all eight 8 x 8 view suites. For
   every family, compare complete feasibility, exact masks, every cell/state
   query, and independently rendered witnesses. Compare all four optimization
   configurations and the Python oracle; witness choices and work counts need
   not be identical when the feasible family is the same.
3. Include restricted and empty domains, unattainable/contradictory observations,
   slab boundaries, ray order/duplicates, both physical adversaries, and all
   decomposition/guard counterexamples above. Audit pruning under branch/query
   assumptions, component event mapping, and every composed witness.
4. Run the complete Release and ASan/UBSan regression gate, including A0/A1,
   then save one reproducible milestone-boundary correctness record. Do not
   conflate it with performance or larger-volume experiments.

That gate has not run in this development chunk. No scaling study has started.
Known cameras, ideal opaque labels and exact arithmetic remain assumptions;
larger-volume performance, camera recovery and broader block states are untested.
