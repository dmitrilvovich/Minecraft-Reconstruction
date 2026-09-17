#!/usr/bin/env python3
"""Export test data from the unmodified oracle. This contains no new solver."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
import time

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / "reference" / "python"
sys.dont_write_bytecode = True
sys.path.insert(0, str(REFERENCE))
import phase_a as oracle

PALETTE = (("air", 0, False), ("stone_cube", 1, False), ("oak_cube", 2, False))


def u32(stream, value):
    stream.write(struct.pack("<I", int(value)))


def export_cases(output):
    cells, worlds, factors, images, cameras, _, _ = oracle.build_observations((2, 2, 2), PALETTE, 8)
    suites = oracle.view_suites(cameras)
    total = 0
    sizes = {}
    with (output / "cases.bin").open("wb") as out:
        out.write(b"MCRA0C1\n")
        u32(out, len(factors))
        for f in factors:
            u32(out, len(f.cells))
            for v in f.cells:
                u32(out, v)
        u32(out, len(suites))
        for name, indices, views in suites:
            groups = oracle.grouped(images, indices)
            sizes[name] = len(groups)
            encoded = name.encode("ascii")
            u32(out, len(encoded)); out.write(encoded)
            u32(out, views); u32(out, len(indices))
            for r in indices:
                u32(out, r)
            u32(out, len(groups))
            for signature, ids in groups.items():
                family = worlds[ids]
                expected = oracle.oracle_support(family)
                labels = list(signature)
                root, supported = oracle.fixed_supports(8, 3, [factors[r] for r in indices], labels, oracle.Stats())
                if supported != expected:
                    raise RuntimeError(("Python reference mismatch", name, ids[0]))
                u32(out, len(ids))
                for world_id in ids:
                    u32(out, world_id)
                out.write(bytes(root)); out.write(bytes(supported)); out.write(bytes(expected))
                out.write(bytes([oracle.freedom_count(np.array(ids, dtype=np.int64), worlds, 3)]))
                for v in range(8):
                    for state in range(3):
                        u32(out, np.count_nonzero(family[:, v] == state))
                total += 1
            print(f"Python oracle: {name}: {len(groups)} complete classes", flush=True)
        # Negative and conditioned fixtures. Only invoke existing reference routines.
        rng = np.random.default_rng(20260917)
        query_count = 512
        u32(out, query_count)
        infeasible = 0
        for case in range(query_count):
            indices = suites[case % len(suites)][1]
            world_id = int(rng.integers(0, len(worlds)))
            labels = images[world_id, indices].copy()
            if case % 3 == 0:
                j = int(rng.integers(0, len(labels)))
                labels[j] = (labels[j]+1) % 3
            domains = [int(rng.integers(1, 8)) for _ in cells]
            if case % 4 == 0:
                domains = [mask | (1 << int(s)) for mask, s in zip(domains, worlds[world_id])]
            selected = np.all(images[:, indices] == labels, axis=1)
            for v, mask in enumerate(domains):
                selected &= ((1 << worlds[:, v]) & mask) != 0
            ids = np.flatnonzero(selected)
            expected = oracle.oracle_support(worlds[ids]) if len(ids) else [0]*8
            feasible = oracle.fixed_geometry(domains, [factors[r] for r in indices], labels, oracle.Stats()) is not None
            if feasible != bool(len(ids)):
                raise RuntimeError("Python restricted feasibility mismatch")
            # Existing complete feasibility is also the independent singleton-query oracle.
            python_support = []
            for v, mask in enumerate(domains):
                support = 0
                for state in oracle.values(mask):
                    probe = domains.copy(); probe[v] = 1 << state
                    if oracle.fixed_geometry(probe, [factors[r] for r in indices], labels, oracle.Stats()) is not None:
                        support |= 1 << state
                python_support.append(support)
            if python_support != expected:
                raise RuntimeError("Python restricted support mismatch")
            u32(out, len(indices))
            for r, label in zip(indices, labels):
                u32(out, r); out.write(bytes([int(label)]))
            out.write(bytes(domains)); out.write(bytes([feasible])); out.write(bytes(expected))
            u32(out, len(ids))
            for world_id in ids:
                u32(out, world_id)
            infeasible += not feasible
    return {"observation_classes": total, "classes_by_suite": sizes,
            "conditioned_queries": query_count, "infeasible_conditioned_queries": infeasible}


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
    case_meta = export_cases(output)
    meta = {"worlds": 6561, "raw_rays": 512, "ray_world_comparisons": 6561*512,
            **case_meta,
            "python": sys.version, "numpy": np.__version__,
            "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest(),
            "export_seconds": time.perf_counter()-start}
    meta["payload_sha256"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in (output/"rays.txt", output/"images.bin", output/"cases.bin")}
    (output / "metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    print(json.dumps(meta), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
