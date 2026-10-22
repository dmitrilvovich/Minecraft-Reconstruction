# PNG to legal A2 scene: working eight-cell demonstration

The executable now accepts an actual PNG and known camera/grid JSON, constructs
the observed rays, calls the existing exact A2 solver and writes a legal scene
witness plus exact per-cell supported states. It handles both established A2
palettes. It does not read the example truth file or precomputed observations.

**Project priority:** larger-volume scaling is suspended indefinitely. Its
protocol, 72,148 frozen design slots, collectors, original binary, preflight and
blocked-launch evidence remain intact. No further scaling collection or benchmark
infrastructure work is planned. The active milestone is this reconstruction
pipeline. Earlier research documents' proposed next steps are historical plans.

## Run from a fresh checkout

Requirements: a C++20 compiler, CMake 3.20+, Ninja, and Python 3.10+ for tests.
PNG and JSON code is vendored with pinned hashes; no NumPy or downloaded library
is needed for this preset. The prototype has been built, executed and validated
in the development environment; local user testing is not needed to establish
this checkpoint.

```bash
cmake --preset png-demo
cmake --build --preset png-demo --parallel 2
ctest --preset png-demo --parallel 2

./build/png-demo/mcr_reconstruct \
  --image examples/a2-png/visible-mixed/image.png \
  --camera examples/a2-png/visible-mixed/camera.json \
  --output reconstruction.json
```

The program prints `SAT`; `reconstruction.json` contains the complete result.
Replace `visible-mixed` with `occluded-oak` to try the ambiguous example.
The preset builds only the app and focused regression tests. It excludes the
benchmark smoke test and historical Python acceptance-corpus generation. The
accepted full-test presets remain available and unchanged in scope.

For an executable-only build without Python:

```bash
cmake -S . -B build/demo -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/demo --target mcr_reconstruct --parallel 2
```

This application uses a normal build, with no timing-study host freeze or WSL
qualification requirement. Linux x86-64 is the environment validated here; this
checkpoint makes no new performance or cross-platform timing claims.

The [tested Linux x86-64 executable](../results/png-reconstruction-demo/mcr_reconstruct-linux-x86_64.tar.gz)
is also published with its dependency license notices. It requires a compatible
glibc/libstdc++ runtime; source builds remain available for other environments.
Its hash and build/runtime identity are recorded with the validation evidence.

## Input contract

The [example metadata](../examples/a2-png/visible-mixed/camera.json) is a complete
template. Schema `mcr-a2-camera-v1` requires precisely `schema`, `grid`, `palette`,
`pixel_encoding`, `image` and `camera`. Unknown/missing fields and duplicate keys
are rejected rather than interpreted as defaults.

- Grid: shape `[2,2,2]`, origin `[0,0,0]`, unit cell size. **Y is vertical**.
  Cell ID is `(x * 2 + y) * 2 + z`. Bottom and top slabs occupy the lower and
  upper Y halves respectively. Intersections retain the accepted half-open
  boundary and positive-length-hit semantics.
- Palette: `split_material` means air, oak bottom slab, stone top slab;
  `same_material` means air, oak bottom slab, oak top slab. Every cell initially
  permits all three states. No hand-supplied domains or truth hints are accepted.
- Image: PNG width and height each within 1–64, matching JSON exactly;
  noninterlaced 8-bit RGB or RGBA with every alpha value 255. Maximum file size
  is 1 MiB. Animated, indexed, grayscale, 16-bit and transparent inputs are
  outside this milestone. CRC, chunk structure and compressed data are checked.
- Encoding `mcr-a2-first-hit-rgb-v1`: exact sample triplets below. There is no
  nearest-color matching, gamma transformation, antialiasing tolerance or shading
  compensation. PNG ancillary color metadata does not change sample labels.
- Camera: exact rational `center`, `forward`, `right`, `up`, `fx`, `fy`, `cx`,
  `cy`. The basis must be orthonormal with `cross(right,up) == forward`, and
  focal lengths positive. Numbers must be JSON integer literals or strings such
  as `"2/3"`; decimal/exponent tokens are rejected before floating-point parsing.
  Numerator/denominator
  components are bounded by 1,000,000 and denominators must be positive.
  Rational overflow is an explicit error, never an infeasibility conclusion.
- Metadata is bounded to 64 KiB and 16 nesting levels. ASCII JSON is sufficient
  for all schema fields. No camera normalization or implicit fitting occurs.

| Pixel observation | Exact RGB |
| --- | --- |
| Background | `(135,206,235)` |
| Stone | `(128,128,128)` |
| Oak | `(160,112,64)` |

Stone is a recognized observation even with the oak-only palette: it makes the
constraints infeasible. An arbitrary unrecognized color is instead invalid
input. These two conditions produce different, explicit outcomes.

The image origin is `top_left`; pixel indices are `(column,row)` with rows
increasing downward. Intrinsics refer to that same origin. The unnormalized ray is

```
origin = center
direction = forward
          + ((column + 1/2 - cx) / fx) * right
          + ((cy - row - 1/2) / fy) * up
```

The adapter converts `cy` and reverses the row index when calling the existing
upward-row C++ pinhole API. The asymmetric oblique fixture has different `fx/fy`
and off-center `cx/cy`, so alignment is tested beyond a symmetric central view.

## Result semantics

Output schema `mcr-reconstruction-v1` includes the input metadata, observation
counts, legal Minecraft state catalog, ordered cells, feasibility and witness.
State names are `air`, `bottom_slab`, `top_slab`; `state_catalog` supplies actual
Minecraft block IDs and slab `type`/`waterlogged` properties for the chosen palette.

| Field | Meaning |
| --- | --- |
| `feasibility` | `SAT` or `UNSAT`, from completed exact inference. |
| `witness.states` | One **simultaneous** legal eight-cell assignment, when SAT. It is independently re-rendered against every original pixel before output. |
| `cells[].supported_states` | Exact union of states occurring at that cell over all feasible scenes. These are marginals, not permission to combine choices arbitrarily. |
| `cells[].unique_state` | The state only when that marginal is a singleton; otherwise `null`. |
| `joint_solution_unique` | True only when the feasible scene is unique; `null` for UNSAT. |
| `projection_complete` | True for every successfully written result. Partial results are not emitted as exact. |

UNSAT writes `witness: null`, eight empty supported-state lists and no claimed
unique blocks. Both SAT and UNSAT are successful evaluations (exit 0). Invalid
input/arguments exit 2; execution or output failures exit 3, with an explanation
on stderr. Errors do not fabricate a scene or map to UNSAT. There is no truncated
search mode: the supported eight-cell problem is solved to completion.

No timing, absolute file paths or timestamps enter reconstruction JSON, so the
same inputs/build produce identical bytes. Output is staged before rename.
An error preserves any earlier output; callers must check the exit status before
treating an existing file as a new result. Inputs cannot be overwritten by the
output path. An interrupted `.mcr-tmp` staging directory blocks reuse until reviewed.

## Committed examples

Each [example directory](../examples/a2-png/) contains `scene.json` (rendering truth
only), `camera.json`, `image.png`, and the actual CLI `reconstruction.json`.
The [example guide](../examples/a2-png/README.md) lists every recovered/ambiguous cell.

1. **Visible mixed materials:** a rational oblique view of an oak bottom slab at
   cell 0 and stone top slab at cell 7. Independent enumeration finds exactly
   **one feasible scene**; every cell, including the six air cells, is unique.
2. **Occluded oak:** four upper bottom slabs hide the four lower cells.
   Independent enumeration finds **81 feasible scenes**. Upper cells 2,3,6,7
   are uniquely bottom slabs; cells 0,1,4,5 each permit all three states. The
   chosen witness uses air in those hidden cells and differs from the generating
   truth. The JSON preserves that ambiguity instead of claiming truth recovery.

The example PNGs were generated with the independent rational AABB renderer in
the test support code, not by passing observation vectors into the CLI. To
reproduce the two PNG/result pairs from their committed metadata and truth:

```bash
python3 tools/generate_png_examples.py --binary build/png-demo/mcr_reconstruct
```

This explicitly overwrites those example PNG/result files. PNG compressed bytes
can depend on the Python zlib version; the exact decoded pixels are the contract.

## Verification evidence and limits

The [validation record](../results/png-reconstruction-demo/README.md) includes
**14/14 focused A0/A1/A2 regression tests** and a file-based end-to-end suite
with **141 checks / 70 CLI invocations**. Seven distinct input images are checked
against independent enumeration of all **6,561 legal worlds each**: both examples
in both palettes, two infeasible images, and an analytically defined single ray
whose marginals are all full although the all-air scene is impossible. Every
returned witness in those oracle cases is tested against every original pixel.

The oracle uses Python `Fraction`, the stated pixel-center formula and primitive
AABB intersection; it imports no production traversal, solver or frozen Python.
PNG reading/writing is independently implemented with standard-library zlib.
Other checks cover all five PNG filters, RGB/RGBA, split IDAT chunks, unsupported
colors/transparency, malformed/truncated images, metadata/schema errors, dimensions,
determinism, missing files and output failures. The new executable's build and
its example executions were completed in the development environment.

Accepted C++ inference/geometry/rendering sources and frozen Python are unchanged.
Historical acceptance corpora, benchmarks and suspended scaling collections were
not rerun. No scientific timing conclusions are drawn from test durations.

This is a tiny, known-camera synthetic-color pipeline. It provides no automatic
camera/grid recovery, texture recognition, lighting/shadow removal, expanded
palette, larger-scene support, schematic export or GUI. Exactness is relative to
the supplied camera/grid and this renderer's supported slab model.

The practical next step toward real screenshots is a controlled capture adapter:
capture a known tiny in-game scene with recorded camera/FOV, resolution and crop,
using a flat-color resource pack as a bridge. Check actual game-to-model pixel
alignment before adding texture/material labeling and explicit uncertainty for
shaded or mixed pixels. That work is a separate milestone.
