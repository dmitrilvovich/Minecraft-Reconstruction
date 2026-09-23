# Milestone 2A: A1 geometry foundation

Scope: the geometry, traversal and rendering needed for air, stone full cube,
oak full cube and oak bottom slab on the existing 2 x 2 x 2 lattice. A1
inference and the full Milestone 2 acceptance are separate checkpoints.

## Canonical source and recovered work

The GitHub repository is `dmitrilvovich/Minecraft-Reconstruction`. Inspection
on 2026-09-18 found only the remote `main` branch, at
`93fbbcd9789b7ebc41eb5187dd65edf696de6fe3` (`Update README.md`). Its preceding
commit is `7198ef3edc8cec9387fc28e904d8ec6a5815c638` (`docs: rewrite README in
project voice`). Both are retained. The `cpp-milestone-1` tag remains at
`235971a41287e3e97ea058c5211912953886c67e`. No remote Milestone 2 branch or pull
request was present.

The interrupted local run retained two commits:

* `37bd50ab56e48912b863491637d555fff2dc5a80`: initial Milestone 2 design notes.
* `8373a776fc4ceb0523a0ecb013a85d07e2c9464c`: A1 states, exact slab rendering,
  traversal and exhaustive rendering checks.

The latter was not present on GitHub. Both are recovered, with their original
IDs, through a merge into `milestone-2a`, based on the freshly fetched GitHub
`main`. No repository was initialized and no existing commit was rewritten.
Later uncommitted inference/benchmark work remains in the previous working
directory and is not included in this checkpoint. The broader design note is
a plan from that earlier run, not a claim that A1 inference is complete here.

The connected GitHub profile and existing repository configuration both identify
`Dmitri Lvovich <dmitri.lvovich@yahoo.ca>` (`dmitrilvovich`). New local commits use
that identity for both author and committer, with no AI coauthor trailers. No
connector-created commit is needed. Nothing is pushed by this checkpoint.

## Implementation

The recovered production geometry implementation is reused without changes.

| Module | Responsibility |
|---|---|
| `model/a1` | Four legal states, material labels, geometry ranks, worlds and domains |
| `model/domain`, `model/observation` | Shared domain bits and pixel labels; A0 remains three-state |
| `render/a1`: AABB path | Independently intersect each candidate solid and select the earliest active state |
| `render/a1`: traversal path | Traverse cells, split intervals at half height, and record slab-hit flags |
| `tests/a1_geometry_tests.cpp` | Explicit expected intersections, boundaries, ordering and occlusion |
| `tests/a1_render_tests.cpp` | Exhaustive cross-renderer/world agreement and boundary sweep |
| `tests/oracle/export_a1.py` | Export images by calling the frozen Python renderer |

The slab occupies `[x,x+1) × [y,y+1/2) × [z,z+1)` and has the same observed oak
label as the oak cube. Intersections must have positive ray length. A ray
parallel to the slab's top surface misses; one running on the included bottom
surface hits. Crossing the top into the interior hits, while a point tangent or
an outward ray beginning on the boundary does not. Cameras and coordinates
remain exact rationals under the existing checked-integer arithmetic contract.

The new focused tests exercise six entry directions in every cell, exact
entry/exit parameters, rays starting inside solids or gaps, top and bottom
boundaries, zero-length corner contacts, half-height crossings at cell entry or
exit, front/rear cube/slab ordering, reversed rays, vertical ordering, and
changes to hidden states. All expected pixels are checked through both C++
renderers. No inference routine is involved.

## Validation

Results and reproducible source hashes are recorded in
[`results/milestone-2a/`](../results/milestone-2a/). The gate includes:

**Passed:** 12/12 Release tests (13.41 seconds) and 12/12 sanitizer tests
(150.79 seconds), with zero renderer mismatches and no sanitizer diagnostics.
These durations include oracle/test work and are not inference benchmarks.

* All 65,536 A1 worlds on all 512 original camera rays: 33,554,432 comparisons
  between Python's AABB renderer, C++ AABB rendering and C++ traversal.
* 8,918 boundary rays with all single-cell/state placements in both C++ paths.
* 153 additional focused ray/scene cases, with exact interval and cell-order checks.
* A0 rendering compatibility on all 6,561 original worlds and all 512 rays.
* Every existing A0 inference, exhaustive acceptance, adversarial and pruning
  audit test as regression coverage.
* Complete Release and AddressSanitizer/UndefinedBehaviorSanitizer test suites.

Commands, using CMake 4.4.3, Ninja 1.13.2, GCC 13.3.0, Python 3.12.14 and NumPy
2.3.5 on Linux:

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release --output-on-failure
cmake --preset sanitize
cmake --build --preset sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset sanitize --output-on-failure
```

The local run uses explicit paths to the installed CMake/Ninja binaries. Leak
detection is disabled because this managed workspace uses process tracing;
address and undefined-behavior checks remain enabled, and UB terminates the
test. Leak detection is not claimed. CI retains its default leak detection and
now collects the A1 renderer metadata as well as A0 results. Remote CI has not
been run for this unpushed checkpoint.

The Python files and `reference/SHA256.json` are unchanged from GitHub `main`.
This validates the A1 forward model only. It does not establish exact A1
supported-state inference, pruning soundness for A1, or scaling behavior.

## Next checkpoint: 2B

Review and selectively reuse the preserved A1 constraint/envelope work. Validate
compact first-hit constraints, GAC, complete nested-envelope feasibility and
exact supported-state queries against every exhaustive family, including
contradictions and restricted domains. Audit every deletion under its proper
query assumptions and require zero search branches. Only after that should A1
benchmarks and the full Milestone 2 acceptance record be completed. A2 is
outside both this checkpoint and that A1 work.
