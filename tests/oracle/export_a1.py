#!/usr/bin/env python3
"""A1 fixture adapter. All inference/reference rendering stays in frozen Python."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
import time

import numpy as np
from export_a0 import oracle, check_frozen_reference, REFERENCE

PALETTE = (("air", 0, False), ("stone_cube", 1, False),
           ("oak_cube", 2, False), ("oak_bottom_slab", 2, True))
RANKS = (0, 2, 2, 1)


def export_local_supports(output):
    """Call the frozen routine; no inference implementation lives in this adapter."""
    count = 8 * 16**3 * 3
    with (output / "local_supports.bin").open("wb") as out:
        out.write(b"MCRA1F1\n" + struct.pack("<I", count))
        for flags in range(8):
            emissions = tuple((0, 1, 2, 2 if flags & (1 << j) else 0) for j in range(3))
            factor = oracle.RayFactor((2, 0, 1), emissions)
            for encoded in range(16**3):
                domains = [(encoded >> (4*v)) & 15 for v in range(3)]
                for target in range(3):
                    masks = oracle.factor_supports(factor, domains, target)
                    out.write(bytes([masks is not None, *(masks if masks is not None else [0]*3)]))
    return count


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
    local_cases = export_local_supports(output)
    meta = {"worlds": 65536, "raw_rays": 512, "ray_world_comparisons": 65536*512,
            "local_support_cases": local_cases,
            "local_support_sha256": hashlib.sha256((output / "local_supports.bin").read_bytes()).hexdigest(),
            "python": sys.version, "numpy": np.__version__,
            "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest(),
            "export_seconds": time.perf_counter()-start}
    (output / "metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    print(json.dumps(meta), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
