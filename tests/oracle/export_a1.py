#!/usr/bin/env python3
"""A1 fixture adapter. All inference/reference rendering stays in frozen Python."""
from pathlib import Path
import argparse
import hashlib
import json
import sys
import time

import numpy as np
from export_a0 import oracle, check_frozen_reference, REFERENCE

PALETTE = (("air", 0, False), ("stone_cube", 1, False),
           ("oak_cube", 2, False), ("oak_bottom_slab", 2, True))
RANKS = (0, 2, 2, 1)


def export(output):
    check_frozen_reference()
    output.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    cells = oracle.cells_for((2, 2, 2))
    ids = np.arange(4**8, dtype=np.int64)
    worlds = ((ids[:, None] // (4**np.arange(8, dtype=np.int64))) % 4).astype(np.uint8)
    with (output / "rays.txt").open("w") as rays, (output / "images.bin").open("wb") as images:
        rays.write("MCR_A1_RAYS_V1 512 65536\n")
        for camera in oracle.camera_specs((2, 2, 2)):
            for c, d in oracle.pixels(camera, 8):
                rays.write(" ".join(f"{q.numerator} {q.denominator}" for q in (*c, *d)) + "\n")
                images.write(oracle.reference_ray(c, d, cells, worlds, PALETTE).tobytes())
    meta = {"worlds": 65536, "raw_rays": 512, "ray_world_comparisons": 65536*512,
            "python": sys.version, "numpy": np.__version__,
            "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest(),
            "export_seconds": time.perf_counter()-start}
    (output / "metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    print(json.dumps(meta), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
