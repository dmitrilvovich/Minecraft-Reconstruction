# A2 correctness gate

The gate validates the solver at `371da8938be8edf4d0664f2a7d38ca34271431de`
without changing production code, accepted A0/A1 behavior, or frozen Python.
This is the tiny known-camera correctness checkpoint. No scaling experiment is
included. The development architecture and soundness arguments remain in
[a2-foundation.md](a2-foundation.md).

## Coverage and independent checks

Both palettes have 6,561 legal worlds on the 2 x 2 x 2 grid:
air/oak-bottom/stone-top and air/oak-bottom/oak-top. The same-material palette
is a first-class control with the same geometry, cameras, truth populations,
domain restrictions, and optimization matrix.

| View suite | Mixed-material families | Same-material families |
| --- | ---: | ---: |
| Axis 1 | 256 | 256 |
| Axis 2 | 2,025 | 1,024 |
| Axis 3 | 4,913 | 4,400 |
| Axis 4 | 4,913 | 4,400 |
| Axis 5 | 5,453 | 4,400 |
| Axis 6 | 5,453 | 4,400 |
| Oblique 1 | 3,600 | 2,154 |
| Oblique 2 | 6,483 | 4,420 |
| **All eight suites** | **33,096** | **25,454** |

Every world belongs to exactly one tested family per suite: 104,976
world/suite/palette memberships. All 512 raw rays are checked, including
identical rays later deduplicated into 83 functions per palette. The
**6,718,464 ray/world/palette comparisons** agree among frozen Python AABB
rendering, C++ AABB rendering, and C++ traversal. Python separately checks its
own traversal encoding. Families are rebuilt from C++ AABB observations;
no solver receives reference families or truth-world IDs.

Additional coverage:

- **27,300** one-cell boundary ray/world/palette comparisons: 4,550 rays per
  palette, all three states, all 26 nonzero directions with coordinates in
  {-1,0,1}, origins on/outside/inside faces and half-height planes, and rational
  offsets immediately above/below half height. Existing named multi-cell
  boundary tests remain in the regression suite.
- **196,608** single-ray factor cases: both palettes, all 64 hit-pattern triples,
  all 512 domain triples including empty masks, and all three target labels.
  C++ automaton masks agree with independent scalar enumeration and frozen Python.
- **2,048** seeded camera cases, 1,024 per palette, combine restrictions,
  altered labels, contradictory duplicate observations and empty domains.
  Exactly 844 mixed and 835 same-material cases are infeasible.
- **2,048** physical path/triangle domain cases: all 512 restrictions on the
  three active cells for each fixture and palette, with other cells fixed air.
  Exactly 904 cases per palette are infeasible.

For every camera family and every additional case, run these existing options:

| Configuration | GAC at search nodes | Decomposition | Checked fixed-hit |
| --- | --- | --- | --- |
| `search` | yes | no | no |
| `decomposition` | yes | yes | no |
| `fixed_hit` | yes | no | yes |
| `both` | yes | yes | yes |

There is no production search mode without GAC; adding one would change the
solver during a correctness checkpoint. The independent all-world enumeration
supplies the non-propagating reference.

Each configuration must match exact feasibility and all eight supported masks,
and answer **every one of the 24 cell/state queries**, including input-excluded
states. Every returned witness must belong to the independently rendered,
domain-conditioned family. The union of projection witnesses must cover every
supported literal exactly. Different configurations may select different valid
witnesses; a canonical identical witness is not part of the API contract.
Nested axis suites must only shrink supports. Relabeling stone to oak must
reproduce every same-material image, and its supported masks must contain the
mixed-material masks for corresponding truth worlds.

The main gate totals **250,584 case/configuration checks**, **6,014,016 direct
support queries**, **2,571,804 verified witness returns**, **55,507,506 audited
reductions**, **62,214,610 audited literal deletions**, **4,119,832 checked
contradictions**, and **59,380,962 factor-ID checks**, per build configuration.
These counts include repeated API calls and the four modes, not distinct worlds.

The observer conditions the independent family at every search/query context,
checks assumptions stay within the requested restrictions, and checks every
reduction preserves all actual supports. For factor events it verifies the
original factor ID, affected cell, and the claimed implication using a separate
scalar enumeration of that factor. Thus an in-range but incorrectly remapped
factor ID cannot pass merely because it mentions the same cell. Deliberately
bad deletions, contradictions, IDs and relaxed assumptions must be rejected by
the checker. Composed component witnesses receive the same whole-scene checks
as all other witnesses.

The expanded focused suite additionally runs all four configurations on the
existing restricted fixtures, seeded abstract scenes, API edges and 22 named
optimization cases: **9,620** feasibility/projection cases, **214,152** direct
queries, **15,416** witness returns, **75,949** reductions and **222,544**
contradiction checks. These overlap the physical cases above and are reported
separately. The named cases include shared invariant context, entailed bridges,
genuinely coupled bridges, optional versus guaranteed occlusion, mixed fixed-hit
and shape regions, conditional graph changes and nested factor-ID remapping.

## Findings

All four configurations agree with exhaustive families. Python inference agrees
on its valid nonempty-domain inputs; the explicit empty-domain exception below
is checked directly against exhaustive enumeration. No C++ solver bug was found. The complete Release coverage remains **33/33**.
The resumed `-O1 -g` ASan/UBSan run passed **25/25 native tests** in 551.45 seconds,
including the full A2 test in 521.92 seconds; its entire A2 report equals the
preserved Release report, including every work counter and histogram. Together
with the eight verified, reused Python setup results, this completes **33/33
suite-entry coverage**. No sanitizer diagnostic occurred.

The path still has two worlds and three globally unsupported air values after
GAC; the triangle has zero worlds despite unchanged nonempty GAC domains. In
both palettes and all modes, their unrestricted feasibility work remains
3 nodes / 2 branches and 4 nodes / 3 branches respectively. Neither fixture
passes the fixed-hit guard. Equal oak labels do not remove the geometric coupling.

Across the complete camera corpus, the largest single feasibility call uses
9 nodes / 8 branches without decomposition, or 13 / 8 with it. The largest
individual support query uses 7 / 6 or 10 / 6 respectively. A whole supported-mask
projection reaches 45 nodes / 32 branches without decomposition, or 61 / 32
with it. Component entry calls count as nodes. Median feasibility work is one
node in every configuration and palette; with both options enabled the p95 is
one node for mixed materials and two for the control. Full distributions and
separate restricted/physical counts are saved in the JSON evidence.

| Camera corpus: decisions + projections + all direct queries | Search branches | Decomposition only | Fixed-hit only | Both |
| --- | ---: | ---: | ---: | ---: |
| Mixed material | 33,082 | 33,082 | 15,758 | 15,790 |
| Same material | 45,504 | 45,504 | 25,739 | 25,739 |
| **Total** | **78,586** | **78,586** | **41,497** | **41,529** |

The checked fixed-hit optimization removes about 47% of these branches. Its
benefit includes restricting branching to relevant shape choices. With both
options enabled there are 38,422 shortcut uses, 7,938 splits and 19,588 component
solves in the camera corpus. Decomposition alone saves no branches there and
adds 27,148 call nodes; whole-scene node counts and component entry counts must
not be confused with branch savings. Witness selection/reuse can change
projection work, explaining the 32-branch difference between fixed-hit-only and
both in the mixed palette. This is correctness work accounting, not a timing
benchmark or a scaling claim.

The independent path followed by a triangle still drops from nine to five
branches with decomposition in both palettes. Two satisfiable paths retain four
branches and add component entries. Optional occluders and non-entailed bridges
retain genuine dependencies. These targeted results demonstrate why the camera
corpus alone is insufficient to validate either optimization.

### Frozen Python limitation

The new adapter exposes a pre-existing edge case outside the historical Python
caller's nonempty-domain usage. With domains `[2,0]`, one ray on cell 0 with
emissions `(0,2,1)` and target oak, frozen `solve` returns `(1,0)` despite cell 1
having no legal state. Its GAC does not inspect an isolated empty domain, and its
fixed-geometry witness constructor chooses air there. Exhaustive feasibility is
false. C++ already rejects the case in all four configurations.

The frozen file is unchanged. The adapter records the raw reproducer and rejects
empty input domains before calling frozen inference; enumeration independently
confirms these cases. Per palette this handles 366 top-level cases and 27,702
direct query inputs. It does not replace any nontrivial Python search decision.
This limitation is not hidden inside a blanket claim that raw Python accepted
every C++ input.

## Reproduction and evidence

Build with the existing presets and run all 33 CTest entries:

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release --parallel 2
cmake --preset a2-sanitize
cmake --build --preset a2-sanitize --parallel 2
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset a2-sanitize --parallel 2
```

The new adapter is `tests/oracle/export_a2_acceptance.py`; the main independent
checker is `tests/a2_acceptance.cpp`. `MCR_A2_ORACLE_DIR` optionally points both
builds to one immutable, hash-verified generated corpus. The adapter checks
`reference/SHA256.json`, hashes its inputs and all six output fixtures, and
only reuses a complete matching cache. Large regenerable binary fixtures are
not committed. Manual full CI now includes A2 and uploads its reports; ordinary
development pushes retain focused checks.

[Machine-readable evidence](../results/a2-correctness/) includes reports,
fixture hashes, exact work distributions, regression logs and recovery records.
The local tools are GCC 13.3.0, CMake 4.4.3, Ninja 1.13.2, Python 3.12.14 and
NumPy 2.3.5. The `a2-sanitize` preset uses Debug with `-O1 -g`,
`-fsanitize=address,undefined` and `-fno-omit-frame-pointer`; linking also uses
`-fsanitize=address,undefined`. All 45 translation-unit commands were checked
for these flags and the absence of `NDEBUG`. The tests use unconditional `CHECK`
assertions, and ordinary C++ assertions remain enabled. Runtime options were
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Leak detection was disabled
for this traced workspace; address and undefined-behavior checks remained enabled. No leak-check result is claimed.

The interrupted, unoptimized Debug A2 sweep is **superseded**, not counted as
a complete gate. Its saved progress reaches mixed-material `axis_5`. The
continuation reused the completed Release results and existing corpora after
hash verification. To avoid regenerating them, the eight previously successful
Python setup tests were carried forward, and all 25 native tests were selected
under the new sanitizer build with:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset a2-sanitize --parallel 2 -E '^python.*export$' -FA '.*' \
  --timeout 600 --output-junit a2-sanitize-junit.xml
```

The persisted configuration points `MCR_A1_ORACLE_DIR` to the verified existing
`build/sanitize/oracle_a1` and `MCR_A2_ORACLE_DIR` to `build/oracle_a2`; A0 fixtures
were copied into `build/a2-sanitize/oracle` with identical bytes. A fresh checkout
uses the full commands above to generate its own fixtures. This is 33-entry
suite coverage with eight reused setup results, not 33 freshly executed tests.

The workspace produced truncated generated fixtures after their exporters had
recorded complete-file hashes. They were verified as exact prefixes and restored
from independently generated, hash-matching copies. Two persisted sanitizer
executables also lacked execute permissions and were relinked. On continuation,
one generated `render_tests.cpp.o` was empty and failed to link (`main` missing);
only that object was rebuilt from unchanged source. Failed attempts
and affected reruns are preserved; these are artifact recoveries, not solver
fixes. No production or frozen-reference source was changed to make a test pass.

## Limits and next experiment

This establishes exactness for the declared tiny corpus and targeted edge cases,
not a proof of useful general performance. Exhaustive camera truths cover all
worlds, but the space of arbitrary contradictory camera images and all possible
eight-cell domain restrictions is not exhaustively tested. Arithmetic remains
bounded checked rational arithmetic, cameras are known, labels are opaque and
ideal, and only the two slab geometries are legal. Marginal supports are not an
independent Cartesian scene description. Path/odd-cycle examples show failure
of the A1 envelope proof and GAC completeness, not an exponential lower bound.

The next chunk should implement a reproducible scaling harness, keeping both
palettes paired by truth IDs, domain masks, cameras and seeds. Start with the
frozen protocol's eight-cell control: vary the number `k` of cells permitting
both orientations from 0 through 8, with multiple spatial arrangements and
reference size `3^k * 2^(8-k)`. Then use 4, 6, 8, 10 and 12 cells with exhaustive
references where budget permits; move to larger volumes only after documenting
reference coverage and budgets. Keep A0/A1 as separate polynomial controls.

Use fixed 1/2/3/6 axis views and oblique controls, pixel density proportional to
volume width, and a higher-resolution control. Pair random scenes with layers,
slab-gap occluders and structured SAT/UNSAT coupled fixtures. Retain all four
current optimization configurations and freeze branching policies. Measure
unaudited decision/projection/query time separately from oracle/render/audit
cost, search nodes and branches, residual component sizes, fixed-hit guard
acceptance, time to first witness versus unsupported-state certification,
ambiguity and solver-only memory. Report truth-weighted and family-weighted
results separately; the palettes have different numbers of observation families. Explicit time/node budgets must yield
"unresolved," never false infeasibility. Preregister the protocol's 27-cell
60-second / one-million-node failure thresholds and report budget failures,
not just completed cases. No scaling run has started at this checkpoint.
