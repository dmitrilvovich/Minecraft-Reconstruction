# A2 incomparable geometry: development foundation

This is the first A2 development chunk after Milestone 2. It establishes geometry,
local propagation and a complete finite search baseline. It is not full A2
certification or a scaling experiment. Accepted A0/A1 production code and frozen
Python are unchanged.

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
conditions one unresolved variable. It ignores entailed factors, invariant
emissions and cells behind a guaranteed earlier hit when choosing that variable.
Selection is minimum domain, then greatest active-factor degree, then cell ID;
values are tried in air/bottom/top order.

Every branch fixes an unresolved cell to one of its remaining values. These
disjoint branches cover all possible worlds at that node, and propagation only
removes locally impossible values. Depth is finite; exhaustion proves
infeasibility. A success returns a simultaneous witness. There is no timeout or
node cutoff that could be confused with infeasibility, and no polynomial runtime
claim. This first baseline does not yet split components or use a fixed-hit
geometry shortcut.

Support queries intersect the original domain with the requested singleton.
Projection first establishes feasibility, then queries uncertified states and
reuses witnesses to cover further literals. Its masks are exact marginals.
Infeasible inputs return empty masks and no witnesses. Zero-cell and empty-domain
inputs have explicit coverage. Each GAC call resets the observer's assumptions
to that search node; final projection exclusions restore the root context.
The observer audits these reductions, not a separately serialized search proof.

## Focused development checks

The first run used GCC 13.3.0, C++20, `-O3 -DNDEBUG`, compiling directly because
CMake was unavailable in the executor. All eight selected executables passed:
`a2_geometry_tests`, `a2_inference_tests`, `geometry_tests`, `a1_geometry_tests`,
`constraint_tests`, `a1_constraint_tests`, `inference_tests`, `adversarial_tests`.

The A2 checks include 870 named geometry samples, 32,768 sampled camera ray/world
comparisons, and A1 bottom-slab rendering compatibility. Inference checks include
all 512 restricted-domain triples for each physical fixture in each palette,
reversed/duplicated factors, disconnected paths with nested search, 320 seeded
four-cell factor cases, malformed inputs and independent audit fault injection.
Together: **2,382 feasibility/projection cases**, **53,136 direct support queries**,
**3,520 verified witnesses**, **16,921 audited reductions** and **54,868 checked
propagation contradictions**. Every projected state is covered by a witness.
Physical witnesses belong to independently AABB-rendered feasible families;
abstract cases use direct first-hit enumeration. A small A1 end-to-end regression
also checks that its existing feasibility/projection/query APIs still use zero
search. These counts are focused development checks, not a full camera-family
acceptance claim.

Reproduce the focused checks with:

```sh
cmake -S . -B build/a2-development -G Ninja -DCMAKE_BUILD_TYPE=Release -DMCR_ENABLE_PYTHON_ORACLE=OFF
cmake --build build/a2-development --parallel 2 --target a2_geometry_tests a2_inference_tests geometry_tests a1_geometry_tests constraint_tests a1_constraint_tests inference_tests adversarial_tests
ctest --test-dir build/a2-development --output-on-failure -R '^(a2_geometry_tests|a2_inference_tests|geometry_tests|a1_geometry_tests|constraint_tests|a1_constraint_tests|inference_tests|adversarial_tests)$'
```

Push/PR CI now runs this focused set. The existing full Release/Python/sanitizer
gate remains available via manual workflow dispatch for milestone boundaries.
No new benchmarks, acceptance manifests, tags or sanitizer run were made here.

Before scaling: add conservative residual-component decomposition and a checked
fixed-hit shortcut, integrate both palettes into the tiny camera-family corpus,
then run the complete A2 reference/sanitizer gate at that meaningful boundary.
Known cameras, ideal opaque labels and exact arithmetic remain assumptions;
larger-volume performance, camera recovery and broader block states are untested.
