# Controlled Minecraft capture compatibility

**Status: capture-ready preparation, not yet validated with a Minecraft screenshot.**
Target: **unmodified Minecraft Java Edition 1.21.1**, with the authored resource
and data packs below. This checkpoint does not establish compatibility with
Bedrock, another Java version, shader mods or arbitrary screenshots. Larger-volume
scaling remains suspended indefinitely. The accepted C++ inference core, PNG
executable and frozen references are unchanged.

The [kit](../examples/minecraft-capture/kit/) builds a tiny known scene. The
[diagnostic](../tools/capture_diagnostic.py) compares an actual PNG's unchanged
samples against declared rays and truth, with explicit discrepancy categories.
All committed screenshot-shaped fixtures are **independently rasterized synthetic
images**, not captures from Minecraft. The [validation record](../results/minecraft-capture/README.md)
describes exactly what was executed here.

## What carries over, and what needs calibration

The accepted [PNG interface](png-reconstruction-demo.md) requires an exact-color
RGB8/opaque RGBA8 image of at most 64×64, a known unit 2×2×2 grid, and an exact
rational orthonormal pinhole camera. Every pixel, including background, is a
constraint. Its two palettes are air / oak bottom slab / stone top slab, and
air / oak bottom slab / oak top slab. It returns one simultaneous witness plus
exact marginal supports; arbitrary combinations of supported states need not
form a solution. Neither the diagnostic nor capture kit changes that contract.

| Issue | Geometric calibration or appearance? | Controlled treatment and remaining gap |
| --- | --- | --- |
| FOV | Geometry | Vanilla projection uses a vertical FOV and framebuffer width/height aspect ratio. Choose 90°; ideal square-pixel focal length is half the framebuffer height. Disable FOV effects and zoom; no sprint, bow, spyglass, fluid or portal effects. GPU projection uses floats, not exact rational arithmetic. |
| Camera center | Geometry | Camera origin is the eye, not teleport/feet position. Use first-person spectator, nominal feet `(1,79.38,14)`, standing eye height 1.62, giving nominal eye `(1,81,14)`. The actual float eye-height constant is 1.6200000047683716; camera interpolation must settle. |
| Orientation/handedness | Geometry | Yaw 180°, pitch 0° faces world −Z, screen right +X, screen up +Y. The current core requires `right × up = forward`, so a documented X reflection converts world coordinates to model coordinates. No image mirroring is performed. |
| Pixel centers/crop | Geometry | Top-left screenshot coordinates, center `(column+½,row+½)`. Use the saved framebuffer PNG, not window decorations or scaled desktop capture. Crop exact pixels without resizing and subtract the crop offset from the principal point. |
| Slabs/grid | Geometry | Unit blocks, Y vertical, bottom slab Y `[0,½]`, top slab `[½,1]`, dry and never double slabs. Model uses half-open positive-length ray intersections. GPU triangle coverage, face culling and depth rounding can differ on boundaries. |
| Textures/filtering/edges | Appearance and rasterization | Authored constant textures remove texture variation. Mipmaps 0; no shader/AA/supersampling mods or driver overrides. Solid textures do not eliminate silhouette sampling, blending, interpolation or edge-coverage differences. |
| Lighting/shading/gamma | Appearance | Pack sets slab model `ambientocclusion:false` and element `shade:false`; smooth lighting off, noon, clear weather, brightness Bright. Vanilla lightmap, color modulation, fog and display pipeline still exist. No core shader is replaced. |
| Sky/background | Appearance plus scene extent | Sky/fog may be a gradient, not `(135,206,235)`. A ray missing this grid means background only if there is no unmodelled geometry along it. Unknown sky samples are **unknown**, never inferred as background from truth. |
| HUD/effects | Appearance | First person, F1, clear effects, no held-item overlay, no outlines, clouds off, entity shadows off, no menu when F2 is pressed. No claim that a resource pack alone ensures these conditions. |

The first capture is a controlled compatibility measurement. A disagreement
cannot uniquely distinguish a wrong camera from a wrong scene or occluder.
Unknown colors also cannot identify their own material or establish a silhouette.
The report keeps these limitations explicit.

## Capture kit and exact setup

The two reproducible ZIPs are:

- `examples/minecraft-capture/kit/mcr-flat-slabs-1.21.1.zip`: resource pack format
  34, authored flat oak/stone textures and four slab models with vanilla extents.
- `examples/minecraft-capture/kit/mcr-capture-scene-1.21.1.zip`: data pack format
  48, scene functions and an opt-in per-tick camera lock.

These are prepared assets for the future capture session; no installation or
manual testing is required to validate this checkpoint. For that session, use a
**dedicated disposable single-player creative world with cheats**, preferably the
Superflat **The Void** preset, in a Java 1.21.1 profile. The setup clears
`X=-8..9,Y=76..85,Z=-4..18`; do not run it in an existing build. Install the two
packs through the normal resource-pack and world data-pack locations, enable the
resource pack, then `/reload`. No Mojang client, account data, copied textures or
shaders are included in the repository.

Run `/function mcr:setup_mixed` as the player. This places these exact states:

| Model ID | Model `(x,y,z)` | Minecraft `(X,Y,Z)` | Mixed-material state |
| --- | --- | --- | --- |
| 0 | 0,0,0 | 1,80,0 | oak_slab, type=bottom |
| 1 | 0,0,1 | 1,80,1 | stone_slab, type=top |
| 2 | 0,1,0 | 1,81,0 | stone_slab, type=top |
| 3 | 0,1,1 | 1,81,1 | air |
| 4 | 1,0,0 | 0,80,0 | air |
| 5 | 1,0,1 | 0,80,1 | oak_slab, type=bottom |
| 6 | 1,1,0 | 0,81,0 | oak_slab, type=bottom |
| 7 | 1,1,1 | 0,81,1 | stone_slab, type=top |

Every slab has `waterlogged=false`. These are the only non-air blocks the kit
places. `/function mcr:setup_same` substitutes oak for the top slabs and uses
`camera-same-material.json`; positions and geometry remain identical. The actual
commands, truth and position map are committed, rather than relying on placement
by eye. No truth file is passed to reconstruction.

The setup switches the player to spectator, clears status effects, freezes the
day/weather cycles, disables new mob spawning and sets clear noon. It tags the
player so the tick function repeats `/teleport @s 1 79.38 14 180 0`. Wait several
seconds after setup; do not move, scroll spectator speed, change pose or touch
the mouse during capture. `/function mcr:camera_off` removes the lock.

Set the following in the dedicated profile:

1. Launcher game resolution **640×480**, windowed; verify the dimensions of the
   actual F2 PNG. GUI scale is not framebuffer resolution. A different saved
   size is rejected; do not resize it to make it pass.
2. **FOV 90**, FOV Effects 0%, View Bobbing OFF, first-person view. The normalized
   saved `fov` value is 0.5, not 90. Use the UI; `options-reference.txt` documents
   only the intended settings and is not a complete options file.
3. Graphics Fast, Smooth Lighting OFF, Clouds OFF, Entity Shadows OFF, Mipmap
   Levels 0, Brightness Bright; no shaders/resource packs except this one, no
   OptiFine/Sodium or antialiasing/postprocessing overrides for the baseline.
4. Press F1 to hide HUD/crosshair, close chat and menus, wait for camera settling,
   and press F2. Keep the original PNG and the actual game/options/version record.
   These settings reduce complications; they do not promise exact palette samples.

## Explicit world-to-model mapping

For continuous coordinates:

```
model x = 2 - Minecraft X
model y = Minecraft Y - 80
model z = Minecraft Z
```

For block indices, `x = 1 - X`, `y = Y - 80`, `z = Z`. The difference of one in
X is intentional: reflecting the cell interval `[X,X+1]` gives `[1-X,2-X]`.
This reflection preserves vertical slab halves and legal block geometry. It
changes coordinate handedness to meet the existing core's basis convention,
without reflecting PNG pixels. Boundary ownership can differ on the reflected
half-open plane; boundary pixels are explicitly flagged.

The nominal eye becomes `(1,1,14)`, forward `(0,0,-1)`, right `(-1,0,0)`, up
`(0,1,0)`. At 640×480 and vertical 90° FOV, ideal `fx=fy=240`, full-image
`cx=320,cy=240`. Crop `[left=288,top=208,width=64,height=64]` yields `cx=cy=32`
without changing focal length. The **64-pixel crop does not itself have a 90°
FOV**. Recomputing its focal length from 64 would be wrong.

All camera inputs are rational integers here. The choice is an ideal model of
Minecraft's float projection and nominal eye height, not a claim that the engine
uses those values bit-for-bit. It avoids a general unknown-camera fit. The
asymmetric scene and injected camera/flip/crop errors test this mapping.

## Run the diagnostic

From a checkout, Python 3.10+ with its standard library is sufficient:

```bash
python3 tools/capture_diagnostic.py \
  --image examples/minecraft-capture/fixtures/ideal-synthetic.png \
  --capture examples/minecraft-capture/kit/capture.json \
  --camera examples/minecraft-capture/kit/camera.json \
  --truth examples/minecraft-capture/kit/truth.json \
  --output-dir capture-diagnostic
```

For the eventual real capture, replace only `--image` with its original F2 PNG.
Use a new output directory each time. Exit 0 means every crop sample agrees
exactly with the declared model/truth; exit 1 means interpretation is required;
exit 2 means invalid input or IO. **Exit 0 alone does not establish that the image
came from Minecraft or independently prove the camera calibration.**

Outputs:

| File | Meaning |
| --- | --- |
| `raw-crop.png`, `camera.json` | Unchanged pixel samples and declared metadata, the only possible inputs to the existing reconstruction executable. No recoloring, resampling, fitted camera, truth domains or dropped pixels. |
| `report.json` | Input hashes; exact-label alignment disagreements; foreground/background versus material discrepancies; unknown color histogram; boundary counts; conditional known-observation count. |
| `pixels.tsv` | Every crop pixel, original screenshot coordinates/RGB, expected and observed labels, expected cell, boundary flag and category. Unknown remains unknown. |
| `expected-from-truth.png` | Explicitly predicted diagnostic reference; **never an image for inference**. |
| `overlay.png`, `boundary-mask.png` | Category image and conservative one-pixel warning band around projected box edges, including hidden edges. |
| `comparison.png` | Left: unchanged crop. Middle: predicted truth. Right: discrepancy categories. |
| `manifest.json` | SHA-256 of every diagnostic output except itself. |

Overlay colors: green = stable exact match; yellow = matching edge warning;
red = recognized foreground/background disagreement; magenta = recognized
material disagreement; blue = unknown appearance. Mismatches take precedence
over edge yellow; `pixels.tsv` and the boundary mask retain both facts.

A known-observation candidate is an exact label matching the declared scene away
from the edge warning band. It is conditional on calibration and scene accuracy;
this is **not** permission to substitute truth labels, ignore unknown pixels,
or send only those candidates to the current PNG solver. The edge band is a
conservative diagnostic aid, not a certified bound on all GPU/model differences.

If every raw crop pixel is exact and the controlled comparison passes, the
existing executable can be invoked separately:

```bash
./build/png-demo/mcr_reconstruct --image capture-diagnostic/raw-crop.png \
  --camera capture-diagnostic/camera.json --output reconstruction.json
```

That process receives no truth file. Its witness and supports remain subject to
the accepted ambiguity semantics. The fixture handoff has been executed here for
both palettes and checked against exhaustive enumeration. An unknown-color real
capture must stop at diagnosis; no arbitrary screenshot is massaged into agreement.

## Validation and single missing real-game step

The preparation tests use a separate forward triangle rasterizer in Minecraft
world coordinates, a depth buffer and floating projection. The diagnostic uses
local rational rays and primitive slab AABBs. Tests cover the ideal image, both
palettes, known camera/crop errors, horizontal reflection, altered sky, shading,
wrong material labels, missing foreground and blended edges. They also cover
PNG filters/RGBA, malformed inputs, artifact hashes, determinism and ZIP/command
reproducibility. No solver, renderer, reference or research dataset is modified.

The environment had Java 17, no configured Minecraft installation or graphical
session; the target requires Java 21. Official client files were inspected as
read-only research, not executed or redistributed. No authenticated game capture
was available. **No actual Minecraft-generated screenshot was tested.**

The missing evidence step is one original F2 capture from this exact setup,
with recorded version/settings, evaluated by this diagnostic. If every sample
already meets the interface, run the unchanged solver on its raw crop and verify
the witness. If sky/lightmap/edges differ, that first capture identifies the next
bounded appearance or calibration change to specify and validate; this checkpoint
does not promise that one screenshot will already be reconstructible. A separate
explicit observation adapter may be needed. No user installation or local testing
is requested during this checkpoint.

## Source inspection and reproducibility

Version and pack formats were checked against Mojang's official 1.21.1 version
manifest and client archive, including `version.json`, slab/blockstate models,
solid shaders, and mapped Camera/GameRenderer/Options bytecode. Inspection facts
and download identities are in
[`source-inspection.json`](../results/minecraft-capture/source-inspection.json).
No proprietary client code is vendored. Relevant primary documentation:

- [Java 1.21.1 release](https://www.minecraft.net/en-us/article/minecraft-java-edition-1-21-1)
- [Java 1.21 technical changes and pack formats](https://www.minecraft.net/en-us/article/minecraft-java-edition-1-21)
- [Mojang 1.21.1 manifest](https://piston-meta.mojang.com/v1/packages/1757c8b1fbfc6b638504e1f6cceb05893f7389d5/1.21.1.json)
- [Fabric's 1.21.1 Camera mappings/API](https://maven.fabricmc.net/docs/yarn-1.21.1+build.3/net/minecraft/client/render/Camera.html)
- [JOML perspective parameters](https://joml-ci.github.io/JOML/apidocs/org/joml/Matrix4f.html#perspective(float,float,float,float))

Rebuild authored assets without a game or solver:

```bash
python3 tools/build_minecraft_capture_kit.py --output rebuilt-capture-kit
cmake --preset png-demo
cmake --build --preset png-demo --parallel 2
ctest --preset png-demo --parallel 2
```

The new `minecraft_capture_compatibility` CTest is bounded infrastructure and
correctness validation. It does not run benchmarks or historical research corpora.
