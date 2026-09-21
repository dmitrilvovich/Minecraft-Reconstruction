#!/usr/bin/env python3
"""Export every A1 camera family using only frozen reference routines.

This is test-data plumbing, not a second solver. The C++ gate independently
rebuilds each family with the AABB renderer and verifies all its members.
"""
from pathlib import Path
import argparse
import hashlib
import json
import sys
import time

import numpy as np
from export_a0 import oracle, check_frozen_reference, REFERENCE, u32

PALETTE = (("air", 0, False), ("stone_cube", 1, False),
           ("oak_cube", 2, False), ("oak_bottom_slab", 2, True))
RANKS = (0, 2, 2, 1)

def export_cases(output):
    cells, worlds, factors, images, cameras, _, _ = oracle.build_observations((2, 2, 2), PALETTE, 8)
    suites = oracle.view_suites(cameras)
    sizes = {}
    maximum_branches = maximum_nodes = 0
    with (output / "acceptance_cases.bin").open("wb") as out:
        out.write(b"MCRA1C1\n")
        u32(out, len(factors))
        for f in factors:
            u32(out, len(f.cells))
            for v, emissions in zip(f.cells, f.emissions):
                if emissions[:3] != (0, 1, 2) or emissions[3] not in (0, 2):
                    raise RuntimeError("Unexpected A1 emission table")
                u32(out, v); out.write(bytes([emissions[3] != 0]))
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
            selected_factors = [factors[r] for r in indices]
            for signature, members in groups.items():
                ids = np.array(members, dtype=np.int64)
                family = worlds[ids]
                expected = oracle.oracle_support(family)
                stats = oracle.Stats()
                root, supported, witnesses = oracle.nested_supports(
                    8, 4, selected_factors, list(signature), stats, ranks=RANKS)
                if supported != expected or not witnesses:
                    raise RuntimeError(("Python A1 mismatch", name, members[0]))
                maximum_branches = max(maximum_branches, stats.branches)
                maximum_nodes = max(maximum_nodes, stats.search_nodes)
                u32(out, len(ids)); out.write(ids.astype("<u4").tobytes())
                out.write(bytes(root)); out.write(bytes(supported)); out.write(bytes(expected))
                out.write(bytes([oracle.freedom_count(ids, worlds, 4)]))
                counts = np.array([[np.count_nonzero(family[:, v] == state) for state in range(4)]
                                   for v in range(8)], dtype="<u4")
                out.write(counts.tobytes())
            print(f"Python A1 oracle: {name}: {len(groups)} complete families", flush=True)
        rng = np.random.default_rng(20260917)
        query_count = 512
        infeasible = 0
        u32(out, query_count)
        for case in range(query_count):
            indices = list(suites[case % len(suites)][1])
            truth = int(rng.integers(0, len(worlds)))
            labels = [int(x) for x in images[truth, indices]]
            if case % 3 == 0:
                j = int(rng.integers(0, len(labels)))
                labels[j] = (labels[j]+1) % 3
            if case % 31 == 0:
                j = next(j for j, r in enumerate(indices) if factors[r].cells)
                indices.append(indices[j]); labels.append((labels[j]+1) % 3)
            domains = [int(rng.integers(1, 16)) for _ in cells]
            if case % 4 == 0:
                domains = [m | (1 << int(s)) for m, s in zip(domains, worlds[truth])]
            selected = np.all(images[:, indices] == labels, axis=1)
            for v, mask in enumerate(domains):
                selected &= ((1 << worlds[:, v]) & mask) != 0
            ids = np.flatnonzero(selected)
            expected = oracle.oracle_support(worlds[ids]) if len(ids) else [0]*8
            selected_factors = [factors[r] for r in indices]
            feasible = oracle.envelope_feasible(domains, selected_factors, labels, RANKS, oracle.Stats()) is not None
            if feasible != bool(len(ids)):
                raise RuntimeError(("Python A1 feasibility mismatch", case))
            python_support = []
            for v, mask in enumerate(domains):
                support = 0
                for state in oracle.values(mask):
                    assumption = domains.copy(); assumption[v] = 1 << state
                    stats = oracle.Stats()
                    witness = oracle.envelope_feasible(assumption, selected_factors, labels, RANKS, stats)
                    if witness is not None:
                        support |= 1 << state
                    maximum_branches = max(maximum_branches, stats.branches)
                    maximum_nodes = max(maximum_nodes, stats.search_nodes)
                python_support.append(support)
            if python_support != expected:
                raise RuntimeError(("Python A1 restricted supports mismatch", case))
            u32(out, len(indices))
            for r, label in zip(indices, labels):
                u32(out, r); out.write(bytes([label]))
            out.write(bytes(domains)); out.write(bytes([feasible])); out.write(bytes(expected))
            u32(out, len(ids)); out.write(ids.astype("<u4").tobytes())
            infeasible += not feasible
    if maximum_branches != 0 or maximum_nodes != 0:
        raise RuntimeError("Nested reference unexpectedly used search")
    return {"observation_classes": sum(sizes.values()), "classes_by_suite": sizes,
            "distinct_factors": len(factors), "conditioned_queries": query_count,
            "infeasible_conditioned_queries": infeasible,
            "maximum_python_branches": maximum_branches, "maximum_python_search_nodes": maximum_nodes}


def export(output):
    check_frozen_reference()
    output.mkdir(parents=True, exist_ok=True)
    fingerprint = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in (Path(__file__), Path(__file__).with_name("export_a0.py"), REFERENCE/"phase_a.py")}
    metadata = output / "acceptance_metadata.json"
    payload = output / "acceptance_cases.bin"
    if metadata.exists() and payload.exists():
        saved = json.loads(metadata.read_text())
        if (saved.get("input_sha256") == fingerprint and saved.get("schema") == "MCRA1C1"
                and saved.get("fixture_sha256") == hashlib.sha256(payload.read_bytes()).hexdigest()):
            print("Verified cached A1 acceptance fixture: 65,536 worlds, 195,620 complete families", flush=True)
            return
    start = time.perf_counter()
    meta = {**export_cases(output), "worlds": 65536, "schema": "MCRA1C1",
            "input_sha256": fingerprint, "python": sys.version, "numpy": np.__version__,
            "reference_sha256": fingerprint["phase_a.py"],
            "export_seconds": time.perf_counter()-start,
            "fixture_sha256": hashlib.sha256(payload.read_bytes()).hexdigest()}
    (output / "acceptance_metadata.json").write_text(json.dumps(meta, indent=2)+"\n")
    print(json.dumps(meta), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
