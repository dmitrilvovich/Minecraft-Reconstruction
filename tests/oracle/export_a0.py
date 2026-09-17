#!/usr/bin/env python3
"""Export test data from the unmodified oracle. This contains no new solver."""
from pathlib import Path
import argparse
import hashlib
import json
import sys
import time

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / "reference" / "python"
sys.dont_write_bytecode = True
sys.path.insert(0, str(REFERENCE))
import phase_a as oracle

PALETTE = (("air", 0, False), ("stone_cube", 1, False), ("oak_cube", 2, False))


def check_frozen_reference():
    manifest = json.loads((ROOT / "reference" / "SHA256.json").read_text())
    actual = {str(p.relative_to(REFERENCE)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(REFERENCE.rglob("*")) if p.is_file()
              and "__pycache__" not in p.parts and p.suffix != ".pyc"}
    if actual != manifest:
        raise RuntimeError("Frozen Python reference changed")


def export(output):
    check_frozen_reference()
    output.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    cells = oracle.cells_for((2, 2, 2))
    ids = np.arange(3**8, dtype=np.int64)
    worlds = ((ids[:, None] // (3**np.arange(8, dtype=np.int64))) % 3).astype(np.uint8)
    with (output / "rays.txt").open("w") as rays, (output / "images.bin").open("wb") as images:
        rays.write("MCR_A0_RAYS_V1 512 6561\n")
        for camera in oracle.camera_specs((2, 2, 2)):
            for c, d in oracle.pixels(camera, 8):
                rays.write(" ".join(f"{q.numerator} {q.denominator}" for q in (*c, *d)) + "\n")
                # Independent ray/AABB oracle, not the Python chain renderer.
                image = oracle.reference_ray(c, d, cells, worlds, PALETTE)
                images.write(image.tobytes())
    meta = {"worlds": 6561, "raw_rays": 512, "ray_world_comparisons": 6561*512,
            "python": sys.version, "numpy": np.__version__,
            "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest(),
            "export_seconds": time.perf_counter()-start}
    (output / "metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    print(json.dumps(meta), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)

