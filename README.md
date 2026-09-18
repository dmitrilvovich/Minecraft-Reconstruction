# Minecraft Reconstruction (mcr)

Minecraft Reconstruction is a personal project I'm working on to reconstruct Minecraft structures from one or more 2D images and eventually export the result as an actual Minecraft build/schematic.

I originally thought the problem would mostly be about figuring out where blocks are in 3D, but it gets complicated pretty quickly once you have occlusion, partial blocks like slabs and stairs, ambiguous camera angles, hidden geometry, and multiple block states that can explain the same pixels.

Because Minecraft already lives on a discrete grid with a finite set of legal block states, I'm trying to take advantage of that directly instead of first reconstructing a generic mesh or point cloud and converting it back into blocks afterward. The project is mainly a mix of computer vision, inverse graphics, computational geometry, and constraint solving.

The main implementation is in C++20.

## Where the project is at

This is still early-stage work, so it is not an end-to-end screenshot-to-schematic tool yet.

Right now I'm building the reconstruction side under known cameras first. The idea is to get the geometry, visibility, ambiguity handling, and solver behavior right in small controlled cases before adding camera recovery and real Minecraft screenshots on top.

The first C++ milestone is complete. It uses a tiny 2 x 2 x 2 world where each cell can be air, a stone cube, or an oak cube. That sounds small, but the point of the milestone was to build a reconstruction core that I can actually check exhaustively rather than immediately scaling up something I don't fully trust.

For that milestone, every one of the 6,561 possible worlds is tested against an independent Python reference implementation. The C++ renderer, ray traversal, constraint propagation, feasible-world decisions, and supported block states all have to agree with the reference.

The detailed acceptance record is in [docs/milestone-1.md](docs/milestone-1.md), and the higher-level C++ design notes are in [docs/design.md](docs/design.md).

The Python code under [reference/](reference/) is intentionally kept around as a frozen research/reference implementation. New solver work is happening in C++.

## What I'm exploring

The reconstruction is being treated as a discrete inverse-rendering problem: the unknown object is the actual Minecraft scene itself, not an intermediate mesh.

A screenshot gives evidence about which block states could be present along each camera ray. Foreground blocks can hide deeper blocks, so the constraints are coupled through visibility rather than being independent per voxel. When the images don't determine something, I'd rather keep that part of the reconstruction ambiguous than make up a block just to force a single answer.

I'm currently working through the problem in stages: full cubes first, then partial/nested geometry such as slabs, then block shapes whose geometry can compete in more complicated ways. After that I want to measure how the exact solver scales before moving on to recovering the camera and Minecraft lattice directly from images.

## Build and test

Requirements for the C++ project are a C++20 compiler, CMake 3.20+, and Ninja.

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release --parallel 2
```

The full Milestone 1 validation also uses Python 3.10+ and NumPy because it compares the C++ implementation against the frozen Python oracle.

For address/undefined-behavior sanitizer checks with GCC or Clang:

```sh
cmake --preset sanitize
cmake --build --preset sanitize --parallel 2
ctest --preset sanitize --parallel 2
```

## Repository layout

- `include/mcr/` and `src/` — C++ geometry, cameras, grid traversal, rendering, and inference
- `tests/` — unit, adversarial, and cross-language correctness tests
- `experiments/` and `src/experiments/` — experiment and benchmark code
- `reference/python/` — frozen Python research implementation
- `docs/` — design notes and milestone acceptance records
- `results/` — recorded experiment/benchmark evidence

The next milestones are adding nested partial-block geometry, then handling incomparable geometry with exact residual search, then doing a real scaling study. After that I'll move into camera/grid recovery and start connecting the solver to actual Minecraft screenshots.
