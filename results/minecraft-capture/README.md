# Controlled capture preparation: validation evidence

**PASS: 16/16 CTest tests, including 87 capture checks.** The existing 14 focused
A0/A1/A2 regressions and PNG file-pipeline suite passed alongside the new capture
suite. `ctest.log` and `ctest.xml` preserve the final execution, with
`validation.json` recording every capture check and injected discrepancy.

The final capture test includes four diagnostic CLI invocations, two separate
synthetic PNG-to-C++ reconstruction calls (one per palette), and independent
exhaustive support/witness comparisons over all 6,561 worlds for each palette.
The diagnostic itself never calls the solver. An earlier development run passed
72 checks before the static command/pack checks were added; the published final
suite has 87. These are bounded functional tests, not benchmark measurements.

| Synthetic test | Observed diagnostic result |
| --- | --- |
| Ideal | All 4,096 crop pixels match; 3,326 stable exact candidates and 770 conservative edge warnings |
| Three-pixel horizontal shift | 240 foreground/background disagreements and 3 material disagreements |
| Injected 4/5 foreground shading | 1,106 unexpected-color pixels; no false claim of a measured camera displacement |
| Changed sky color | 2,990 unknown pixels, not silently labelled background |
| Blended edges | 40 unexpected edge pixels |
| Horizontal reflection | 912 foreground/background and 38 material disagreements |
| Wrong focal length, principal point, eye height and crop | Each produces recognized-label spatial disagreement |
| Both palette handoffs | Exact supports and a valid simultaneous witness; two feasible scenes in each case |

`ideal-diagnostic/` and `shifted-diagnostic/` preserve complete example outputs:
unchanged crop, copied camera metadata, truth-reference image, category overlay,
edge mask, comparison image, per-pixel TSV, report and output hashes. The middle
panel/reference image is generated from truth and is never a reconstruction input.

`source-inspection.json` records official Minecraft client/mapping identities,
read-only inspection findings and environment limitations. `integrity.json`
records preserved source/evidence identities, tool hashes and the unchanged
accepted PNG executable hash. `checkpoint-manifest.json` hashes this publication's
new/changed files except itself. No game binaries or assets from the downloaded
client are distributed. All resource-pack textures/models here are authored.

**No actual Minecraft-generated screenshot was tested.** The environment had
Java 17, no configured game installation or graphical session; the target uses
Java 21. Published fixture images are independent synthetic triangle rasterization
with deliberate errors. Resource/data-pack syntax and contents were checked
statically, not loaded into a running game.

The remaining evidence step is an original 640×480 F2 capture from the documented
Java 1.21.1 setup, followed by this diagnostic. Lighting/sky/float rasterization
may still block exact-color input; any necessary interpretation must be explicit
and independently validated before claiming genuine in-game reconstruction.
Larger-volume scaling remains suspended; no research collection was performed.
