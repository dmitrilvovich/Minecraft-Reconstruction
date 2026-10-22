# Two PNG reconstruction examples

Run either directory's `image.png` with its `camera.json`. `scene.json` records
the scene used to render the PNG and is **not passed to reconstruction**.
`reconstruction.json` is the actual deterministic CLI output.

```bash
./build/png-demo/mcr_reconstruct --image examples/a2-png/occluded-oak/image.png \
  --camera examples/a2-png/occluded-oak/camera.json --output reconstruction.json
```

| Cell ID | Position `(x,y,z)` | Visible mixed: uniquely recovered | Occluded oak: exact support | Occluded witness choice |
| --- | --- | --- | --- | --- |
| 0 | (0,0,0) | Oak bottom slab | Air / oak bottom / oak top | Air |
| 1 | (0,0,1) | Air | Air / oak bottom / oak top | Air |
| 2 | (0,1,0) | Air | Oak bottom slab only | Oak bottom slab |
| 3 | (0,1,1) | Air | Oak bottom slab only | Oak bottom slab |
| 4 | (1,0,0) | Air | Air / oak bottom / oak top | Air |
| 5 | (1,0,1) | Air | Air / oak bottom / oak top | Air |
| 6 | (1,1,0) | Air | Oak bottom slab only | Oak bottom slab |
| 7 | (1,1,1) | Stone top slab | Oak bottom slab only | Oak bottom slab |

Y is vertical. The oblique mixed-material PNG determines all eight states and
has one feasible scene. In the overhead oak PNG, the upper slabs hide all lower
cells: four independent hidden choices give 81 feasible scenes in this example.
The generating truth contains hidden slabs; the witness chooses hidden air.
Neither hidden choice is uniquely recovered. In general, supported states are
coupled marginals and cannot be combined arbitrarily; the returned witness is
always one jointly valid assignment.

![Exact synthetic mixed-material input](visible-mixed/image.png)
![Exact synthetic occlusion input](occluded-oak/image.png)

These are intentionally small 32×32 inputs; view at an integer zoom with nearest
neighbor scaling to inspect pixels. The [pipeline documentation](../../docs/png-reconstruction-demo.md)
specifies RGB labels, camera conventions, build/test commands and limitations.
