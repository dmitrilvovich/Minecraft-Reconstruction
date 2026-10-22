# Minecraft Reconstruction (mcr)

Minecraft Reconstruction is a personal project I'm working on to reconstruct Minecraft structures from one or more 2D images and eventually export the result as an actual Minecraft build/schematic.

I originally thought the problem would mostly be about figuring out where blocks are in 3D, but it gets complicated pretty quickly once you have occlusion, partial blocks like slabs and stairs, ambiguous camera angles, hidden geometry, and multiple block states that can explain the same pixels.

Because Minecraft already lives on a discrete grid with a finite set of legal block states, I'm trying to take advantage of that directly instead of first reconstructing a generic mesh or point cloud and converting it back into blocks afterward. The project is mainly a mix of computer vision, inverse graphics, computational geometry, and constraint solving.

The main implementation is in C++20.

## Where the project is at

This is still early-stage work, so it is not an end-to-end screenshot-to-schematic tool yet.

A **working PNG-to-scene demonstration** is now available: an exact-color PNG plus known camera/grid metadata produces a legal A2 scene witness and exact supported block states. The [demo guide](docs/png-reconstruction-demo.md) includes a one-command invocation, committed examples and end-to-end validation. It distinguishes uniquely recovered cells from hidden or ambiguous states.

A [controlled Minecraft Java 1.21.1 capture kit and diagnostic](docs/minecraft-capture-compatibility.md) now prepare the bridge to real screenshots: exact scene commands, flat slab resource pack, known camera/crop metadata and pixel-level discrepancy reports. The tools are validated with independent synthetic fixtures and injected errors. **A genuine Minecraft-generated capture has not yet been tested**; lighting, sky and rasterization compatibility remain explicit open checks.

Right now I'm building the reconstruction side under known cameras first. The idea is to get the geometry, visibility, ambiguity handling, and solver behavior right in small controlled cases before adding camera recovery and real Minecraft screenshots on top.

The first C++ milestone is complete. It uses a tiny 2 x 2 x 2 world where each cell can be air, a stone cube, or an oak cube. The point of the milestone was to build a reconstruction core that I can actually check exhaustively rather than immediately scaling up something I don't fully trust.

For that milestone, every one of the 6,561 possible worlds is tested against an independent Python reference implementation. The C++ renderer, ray traversal, constraint propagation, feasible-world decisions, and supported block states all have to agree with the reference.

The detailed acceptance record is in [docs/milestone-1.md](docs/milestone-1.md), and the higher-level C++ design notes are in [docs/design.md](docs/design.md).

The [second C++ milestone is complete](docs/milestone-2.md). It adds oak bottom slabs and exact A1 feasibility, simultaneous witnesses, requested state support and whole-scene supported domains, with zero combinatorial search. Both independent C++ rendering paths agree with frozen Python on all 65,536 four-state worlds.

The implementation progressed through [geometry](docs/milestone-2a.md), [single-ray constraints](docs/milestone-2b-rays.md), [shared propagation](docs/milestone-2b-propagation.md), [complete feasibility](docs/milestone-2b-feasibility.md), [requested support](docs/milestone-2b-support-query.md), and [exact projection](docs/milestone-2b-projection.md). The [2C correctness gate](docs/milestone-2c-correctness.md) checks all 195,620 observation families across eight camera/view suites. The [2D1 benchmarks](docs/milestone-2d1-benchmarks.md) record runtime, work, memory and ambiguity.

With six axis views at 8 x 8 pixels, 87.06% of cells are identifiable when averaging over all A1 truth worlds. The earlier 91.04% figure averages over cube-only truth worlds while still allowing slabs during inference. [The final record](docs/milestone-2.md#reconciliation-9104-versus-8706) reconciles these populations and explains the limits of the tiny experiment.

The [A2 development foundation](docs/a2-foundation.md) adds incomparable bottom/top slab geometry, both the original oak/stone palette and a same-material oak control, and exact branch-and-propagate inference. The solver now separates independent residual components and checks actual ray hit patterns before using a fixed-hit shortcut. Physical path and triangle fixtures retain the gap between local consistency and global feasibility in both palettes. The [A2 correctness gate](docs/a2-correctness.md) is complete for both palettes: every tiny-world camera family, all four optimization configurations, exact supports and witnesses, and Release/ASan/UBSan coverage.

The **fixed eight-cell A2 scaling experiment is complete**, with a [final report](docs/a2-scaling-8cell.md) synthesizing the preserved [scientific review](docs/a2-scaling-review.md) and [review evidence](results/a2-scaling-review/README.md). The [adoption record](docs/a2-scaling-adoption.md) establishes the canonical **491,199-case reconstructed exploratory 8-cell A2 scaling dataset**. It is not the recovered original 410,953-case run, is not proven equivalent, and is not a verified correction or superset. Its protocol was explicitly reconstructed after the original artifacts were lost; its exact protocol and provenance are preserved in the [artifact index](results/a2-scaling-8cell/README.md). No scientific claim depends on reproducing the historical count. Search in this tiny corpus depends on arrangement, visibility and material information as well as incomparable-cell count; the report separates occupancy sensitivity and branch reductions from causal or general runtime claims. **Larger-volume scaling is suspended indefinitely; collection has not started.**

The Python code under [reference/](reference/) is intentionally kept around as a frozen research/reference implementation. New solver work is happening in C++.

## What I'm exploring

The reconstruction is being treated as a discrete inverse-rendering problem: the unknown object is the actual Minecraft scene itself, not an intermediate mesh.

A screenshot gives evidence about which block states could be present along each camera ray. Foreground blocks can hide deeper blocks, so the constraints are coupled through visibility rather than being independent per voxel. When the images don't determine something, I'd rather keep that part of the reconstruction ambiguous than make up a block just to force a single answer.

The exact core now supports incomparable slab geometry. The current priority is connecting images to that core through a usable reconstruction pipeline, starting with synthetic first-hit colors and known cameras. Controlled real Minecraft captures are the next practical step; automatic camera/lattice recovery remains future work.

## Try the PNG demo

```sh
cmake --preset png-demo
cmake --build --preset png-demo --parallel 2
ctest --preset png-demo --parallel 2
./build/png-demo/mcr_reconstruct --image examples/a2-png/visible-mixed/image.png \
  --camera examples/a2-png/visible-mixed/camera.json --output reconstruction.json
```

This preset uses vendored PNG/JSON code and Python's standard library for tests. It runs the focused regressions and PNG end-to-end tests without historical acceptance-corpus generation or benchmarks. The [two examples](examples/a2-png/README.md) demonstrate complete recovery and genuine occlusion ambiguity; [validation evidence](results/png-reconstruction-demo/README.md) was produced in the development environment.

## Build and test

Requirements for the C++ project are a C++20 compiler, CMake 3.20+, and Ninja.

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release --parallel 2
```

The full A0, A1 and A2 correctness validation also uses Python 3.10+ and NumPy because it compares the C++ implementation against the frozen Python oracle.

For address/undefined-behavior sanitizer checks with GCC or Clang:

```sh
cmake --preset a2-sanitize
cmake --build --preset a2-sanitize --parallel 2
ctest --preset a2-sanitize --parallel 2
```

## Repository layout

- `include/mcr/` and `src/` — C++ geometry, cameras, grid traversal, rendering, and inference
- `apps/reconstruct/` — PNG/camera adapter and `mcr_reconstruct` executable
- `examples/a2-png/` — real PNG inputs, known scenes/cameras and reconstruction results
- `third_party/` — pinned, licensed PNG and JSON dependencies
- `tests/` — unit, adversarial, and cross-language correctness tests
- `experiments/` and `src/experiments/` — experiment and benchmark code
- `reference/python/` — frozen Python research implementation
- `docs/` — design notes and milestone acceptance records
- `results/` — recorded experiment/benchmark evidence

The suspended larger-volume study retains its [preregistered protocol](docs/a2-scaling-larger-protocol.md), [machine-readable specification](experiments/protocols/a2-scaling-larger-v1.json), [collector/reference preflight](docs/a2-scaling-larger-preflight.md), [72,148 frozen input slots](results/a2-scaling-larger-v1/inputs/input-manifest.json) and [deployment evidence](docs/a2-scaling-larger-deployment-amendment-2.md). These artifacts are preserved for provenance. No further benchmark infrastructure work or collection is planned; larger-volume performance remains unestablished.
