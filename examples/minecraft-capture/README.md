# Controlled capture kit — Java Edition 1.21.1

**No image here is a Minecraft-generated screenshot.** The kit is ready for the
first real capture; real-game compatibility has not yet been established.

See the [complete setup and diagnostic guide](../../docs/minecraft-capture-compatibility.md).

- `kit/`: reproducible scene/data pack, flat slab resource pack, exact block map,
  two palette camera files, known truth, capture dimensions/crop and hash manifest.
- `fixtures/ideal-synthetic.png`: independent forward triangle rasterization in
  Minecraft world coordinates, with ideal label colors and ideal pinhole projection.
- `fixtures/shifted-synthetic.png`: that image shifted three pixels horizontally.
- `fixtures/shaded-synthetic.png`: foreground RGB multiplied by 4/5 and rounded
  down. This deliberately injected variation is not a claim about Minecraft's
  exact lightmap/shading formula.

The fixture rasterizer is [tests/capture/fixtures.py](../../tests/capture/fixtures.py).
Both palette reconstructions have two feasible scenes in the exhaustive test;
this single view does not recover every hidden state uniquely.

A diagnostic command (no solver execution):

```bash
python3 tools/capture_diagnostic.py \
  --image examples/minecraft-capture/fixtures/ideal-synthetic.png \
  --capture examples/minecraft-capture/kit/capture.json \
  --camera examples/minecraft-capture/kit/camera.json \
  --truth examples/minecraft-capture/kit/truth.json \
  --output-dir capture-diagnostic
```

The crop, predicted truth and discrepancy image are shown left to right below.
Green is stable exact agreement, yellow is an edge warning, red/magenta indicates
recognized-label disagreement. Unknown appearance is blue. The diagnostic
preserves all original crop samples and does not feed truth to the solver.

Ideal **synthetic** fixture:

![Synthetic ideal crop, expected truth and categories](../../results/minecraft-capture/ideal-diagnostic/comparison.png)

Shifted **synthetic** fixture:

![Synthetic shifted crop, expected truth and discrepancies](../../results/minecraft-capture/shifted-diagnostic/comparison.png)
