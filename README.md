# mcr — exact reconstruction core

C++20 implementation of the known-camera reconstruction mathematics.
Current scope: **Milestone 1 / A0**, eight lattice cells and three states:
air, stone full cube, oak full cube.

See [the C++ design](docs/design.md) and the
[frozen research reference](reference/README.md). Python remains an independent
oracle; new solver work belongs in C++.

Milestone 1 reproduces all 6,561 worlds and exact state supports. The C++ core
contains no A1/A2 shape solver yet. See [the acceptance record](docs/milestone-1.md).

## Build and test

Requirements: C++20 compiler, CMake 3.20+, Ninja. The core has no third-party
C++ dependency. Cross-language acceptance additionally needs Python 3.10+ and
NumPy (the recorded run uses Python 3.12.14 and NumPy 2.3.5).

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release --parallel 2
```

The full test run regenerates oracle fixtures from the frozen Python files,
checks their hashes, compares both C++ renderers with Python on all worlds,
and audits complete feasible families and every pruning operation. Acceptance
requires all tests; `MCR_ENABLE_PYTHON_ORACLE=OFF` is only a local development
option and does not constitute full Milestone 1 validation.

For address/undefined-behavior checks with GCC or Clang:

```sh
cmake --preset sanitize
cmake --build --preset sanitize --parallel 2
ctest --preset sanitize --parallel 2
```

In this managed workspace, LeakSanitizer cannot run under process tracing. The
recorded sanitizer command therefore prefixes CTest with
`ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior instrumentation
remain enabled. Leak detection is not claimed for that run. The default preset
and CI do not disable it.

Other generators work with ordinary CMake commands, for example:

```sh
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Release
cmake --build build/local --config Release
ctest --test-dir build/local -C Release --output-on-failure
```

Only GCC/Linux is validated in the recorded run. The CI workflow is prepared
for release and sanitizer checks on pushes and pull requests; it has not yet
run on a GitHub remote.

## C++ benchmark

```sh
./build/release/mcr_a0_benchmark --output out/a0 --repetitions 5
```

This runs only the fixed 2 x 2 x 2 A0 experiment. It writes a per-class CSV and
JSON summary, including feasible-world counts, root pruning, exact candidates,
identifiability, true freedom, support queries, propagation work, and timings.
The measured calls exclude oracle lookup, audit callbacks, and problem setup.
Memory is whole-process peak RSS, including the exhaustive C++ corpus.
There is no scaling study or cross-language speedup claim in this milestone.

## Layout and API

| Path | Purpose |
|---|---|
| `include/mcr/`, `src/` | Geometry, camera, grid, states, renderers and inference |
| `src/experiments/`, `experiments/` | Separate C++ experiment support and benchmark executable |
| `tests/fixtures/` | Named adversarial A0 scenes |
| `tests/oracle/` | Python export adapter and read-only exhaustive pruning audit |
| `reference/python/` | Original Python source, protocol and results, unchanged |
| `results/milestone-1/` | Recorded acceptance and benchmark evidence |
| `docs/` | Design, proof assumptions, and acceptance scope |

`mcr` is the solver/geometry CMake target. `mcr_experiments` depends on it and
contains exhaustive experiment utilities; the core never depends on the oracle.

The public inference entry points are `fixed_geometry(problem, domains, stats)`
and `exact_supports(problem, domains, stats)`. Optional audit observers return
no inference decisions. Domain bit `1 << state` indicates membership; A0 state
codes are air=0, stone=1, oak=2. Feasible results include an actual witness;
infeasible support results have no witness and no supported-domain array.
Input errors and arithmetic overflow throw exceptions, never `infeasible`.

Rays have exact rational coordinates, unnormalized directions and positive-length
half-open-cell intersections. The numeric backend deliberately fails on checked
integer overflow; it is not an arbitrary-precision type. All declared cameras
and boundary tests fit its range and agree exactly with Python fractions.

Future milestones are A1 nested-geometry envelopes, then A2 propagation and exact
residual search, then C++ scaling measurements. Each must retain the frozen
reference and pass its own correctness gate before the next stage.
