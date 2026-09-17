# C++ Milestone 1 — A0 acceptance record

**PASS.** The primary implementation is now C++20. It reproduces the frozen A0
experiment on all 6,561 worlds. Python remains unchanged as the independent
reference. This milestone contains no C++ A1/A2 solver and no scaling experiment.

Tested implementation commit: `adca57134e689b910ce603db2f0d89b3e3f2104a`.
The later evidence commit changes documentation and recorded results only.

## Acceptance criteria

| Requirement | Evidence | Result |
|---|---|---|
| Enumerate all 3^8 worlds | Every world ID 0..6560, with round-trip base-three encoding | Pass |
| Render exactly like Python | 512 raw rays x 6,561 worlds = 3,359,232 label comparisons; rational camera rays also compared | Pass |
| Independent C++ rendering paths | AABB depth ordering agrees with grid-plane traversal; first-hit automata accept exactly the rendered labels | Pass |
| Exact feasible/infeasible decisions | All 29,032 rendered observation families plus 512 conditioned/altered-observation queries, 409 infeasible | Pass |
| Exact supported-state domains | C++ equals both Python inference and exhaustive supports; complete family membership also compared | Pass |
| Adversarial and boundary tests | 24 named scenes, empty/malformed cases, contradictory duplicates, restricted domains, simultaneous crossings and half-open faces | Pass |
| No unsound pruning | 694,321 domain-reduction events and 769,711 removed literals checked against conditioned exhaustive families | Pass |

The all-world comparison uses only background/stone/oak labels. Neither inference
path receives generating-cell identity, depth, normals, or a visibility mask.
The maximal occupied witness is one explanation, not a claim about the truth.

## Coverage and independent checks

| View suite | Complete observation classes |
|---|---:|
| 1 axis view | 81 |
| 2 axis views | 1,225 |
| 3 axis views | 3,239 |
| 4 axis views | 4,157 |
| 5 axis views | 4,929 |
| 6 axis views | 5,453 |
| 1 oblique view | 3,601 |
| Opposite oblique pair | 6,347 |
| **Total** | **29,032** |

Every world participates in every suite's partition. Every class is solved,
including classes the original Python research run sampled. For each class,
the tests compare all member IDs, all 24 state-occurrence counts, marginal
domains, freely-variable-cell count, and feasible witnesses. Increasing axis
view prefixes must only shrink supported domains.

The 512 additional queries use seed `20260917`, explicit state restrictions, and
selected altered observations. Both Python's existing complete feasibility
routine and singleton queries agree with exhaustive enumeration. C++ repeats
the same queries. Empty-domain and contradictory duplicate-ray cases are also
explicit standalone regressions.

Other coverage:

* 8,918 boundary rays plus 512 camera rays compare exact ordered cell intervals
  between AABB intersection and grid traversal. No cell repeats, and the lattice
  distance rank from the camera's floor cell strictly increases.
* 7,203 four-cell local-domain/label combinations compare automaton GAC supports
  with direct scalar enumeration.
* 1,500 seeded abstract constraint systems and their reversed processing orders
  check complete feasibility, exact supports and every singleton state query.
* Named scenes include empty and uniform volumes, front/back sheets, stone
  occluding oak, a checkerboard, an open corridor and each isolated material/cell.
* The two-cell first-hit fixture distinguishes a full but correlated marginal
  from genuine freedom. Once its front stone is fixed, the back cell is free.
* Deliberately deleting a supported hidden-oak state and deliberately declaring
  a feasible family inconsistent are both rejected by the audit.

The audit has no role in solver decisions. It recomputes a conditioned reference
family at each query start. The same results must be reproduced with auditing
disabled. All 29,135 feasible tested camera cases/queries have validated witnesses;
the maximal-witness material substitutions are checked for supported non-air
states as well. The two audited decision paths account for 818 checked
contradiction reports on the 409 infeasible conditioned queries.

## Mathematical implementation

`mcr` implements exact rational geometry, cameras, independent rendering, the
three-state model, first-hit automata, GAC, and complete fixed-geometry inference.
`mcr_experiments` contains the view rig and exhaustive corpus and depends on
`mcr`; there is no dependency in the reverse direction.

The complete A0 kernel prunes only the first active cell in the maximal candidate
scene. If that cell is present in a feasible subworld, it cannot be hidden by an
earlier allowed cube. Wrong materials can therefore be removed soundly. Removing
an optional cell exposes later cells; losing a required cell is a contradiction.
At a fixed point, the retained maximal occupancy has a legal simultaneous witness.
Surviving non-air states are supported; explicit force-air feasibility queries
complete the projections. These queries are assumptions, not search branches.

All 29,032 rendered classes had zero unsupported survivors after the declared
root procedure (fixed-geometry elimination followed by GAC). The implementation
still performs the complete air queries; it does not assume this observed
root completeness for new inputs. It uses zero search branches and zero search
nodes. Ambiguous feasible families remain ambiguous.

The rational backend uses normalized checked 64-bit integers, not floating point.
Comparisons avoid overflowing cross-products. An out-of-range arithmetic operation
throws; it is never interpreted as infeasibility. The entire declared camera
model agrees with the arbitrary-precision Python fraction reference. No broader
numeric range is claimed.

## Recorded C++ measurements

Release build, GCC 13.3.0, CMake 4.4.3, Linux. Each observation class receives one
untimed validation/warmup call and five timed complete support calls. The table
reports the median across per-class median times. These are small-case smoke
measurements, not a scaling result or a controlled Python/C++ speed comparison.

| Axis views | Identifiable cells, uniformly over worlds | Median complete support query |
|---|---:|---:|
| 1 | 11.11% | 1.853 microseconds |
| 2 | 46.30% | 2.213 microseconds |
| 3 | 73.86% | 2.764 microseconds |
| 4 | 84.75% | 3.005 microseconds |
| 5 | 90.26% | 3.415 microseconds |
| 6 | 93.87% | 3.836 microseconds |

The one-oblique and opposite-pair medians were 3.835 and 6.470 microseconds.
Timing excludes oracle work, audit callbacks, corpus construction and problem
compilation. Per-class CSV records initial/root/exact candidate counts, unsupported
survivors, identifiability, true freedom, query counts, propagation work and times.
JSON includes median/p95/maximum per-class medians. Quantiles select index
`floor(p*(n-1))` in sorted samples.

Whole-process peak RSS for the benchmark was 5,088 KiB, including the C++ exhaustive
corpus and results bookkeeping. This is **not solver-only memory**. Raw timing and
memory values are environment-specific and cannot establish computational promise
beyond this tiny fixed-geometry model.

Release: **8/8 tests passed**. AddressSanitizer/UndefinedBehaviorSanitizer:
**8/8 tests passed**. LeakSanitizer cannot run under this environment's process
tracing, so the latter run used `ASAN_OPTIONS=detect_leaks=0`. Leak detection is
not validated here. Default presets and the prepared GitHub CI leave it enabled.
The GitHub workflow has not been executed remotely.

## Evidence and project history

`results/milestone-1/` contains both acceptance reports, CTest logs, Python fixture
hashes and versions, the C++ benchmark summary and all 29,032 CSV rows, plus source
hashes and environment provenance. All 11 imported files under `reference/python/`
remain byte-for-byte identical to `reference/SHA256.json`.

The Git history separates reference/design, exact geometry and traversal, A0
rendering, first-hit automata/GAC, complete inference with exhaustive audits,
benchmark/CI integration, and this acceptance record. The repository has no
configured remote, so these commits have not been pushed to GitHub.

**Decision:** C++ Milestone 1 passes. A1 may be the next implementation milestone;
A2 and scaling remain later correctness gates. This result validates the A0 port
and its finite-model mathematics, not scalable general reconstruction.
