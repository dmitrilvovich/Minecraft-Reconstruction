#!/usr/bin/env python3
"""A2 gate adapter: call frozen Python, never modify or reimplement its solver.

Each fixture is closed before hashing. Cache hits require all input/output hashes.
The C++ gate rebuilds the full families with independent AABB rendering.
"""
from pathlib import Path
from fractions import Fraction as Q
import argparse
import hashlib
import itertools
import json
import sys
import time

import numpy as np
from export_a0 import oracle, check_frozen_reference, REFERENCE, u32

PALETTES = {
    "split": (("air", 0, False), ("oak_bottom_slab", 2, True), ("stone_top_slab", 1, 2)),
    "same": (("air", 0, False), ("oak_bottom_slab", 2, True), ("oak_top_slab", 2, 2)),
}


def write_text(out, value):
    encoded = value.encode("ascii")
    u32(out, len(encoded)); out.write(encoded)


def write_factor(out, factor):
    u32(out, len(factor.cells))
    for v, emissions in zip(factor.cells, factor.emissions):
        assert emissions[0] == 0
        u32(out, v); out.write(bytes([emissions[1] != 0, emissions[2] != 0]))


def check_case(out, worlds, factors, labels, domains, ids, totals, full=False):
    family = worlds[ids]
    expected = oracle.oracle_support(family) if len(ids) else [0] * 8
    audit = oracle.Audit(family)
    stats = oracle.Stats()
    # Frozen GAC/fixed_geometry assumes nonempty input domains and can miss an
    # empty isolated cell. Keep that historical file unchanged. The adapter
    # explicitly discharges this trivial infeasibility case by enumeration.
    root = oracle.gac(domains, factors, labels, stats, audit) if all(domains) else None
    witness = oracle.solve(domains, factors, labels, stats, audit) if all(domains) else None
    totals["empty_domain_inputs"] += not all(domains)
    assert (witness is not None) == bool(len(ids))

    def check_witness(w):
        assert any(np.array_equal(w, x) for x in family)
        assert all(domains[v] & (1 << s) for v, s in enumerate(w))
        totals["witnesses"] += 1

    if witness is not None:
        check_witness(witness)
    if full:
        actual_root, support, witnesses = oracle.shape_supports(8, 3, factors, labels, stats, audit)
        assert actual_root == root and support == expected
        covered = [0] * 8
        for w in witnesses:
            check_witness(w)
            for v, s in enumerate(w):
                covered[v] |= 1 << s
        assert covered == expected
    else:
        # Restricted domains cannot use shape_supports' all-states initializer.
        support = [0] * 8
        for v in range(8):
            for s in range(3):
                assumption = domains.copy(); assumption[v] &= 1 << s
                w = oracle.solve(assumption, factors, labels, stats, audit) if all(assumption) else None
                totals["empty_domain_query_inputs"] += not all(assumption)
                assert (w is not None) == bool(expected[v] & (1 << s))
                if w is not None:
                    check_witness(w); assert w[v] == s
                    support[v] |= 1 << s
                totals["direct_queries"] += 1
        assert support == expected
    totals["cases"] += 1
    totals["infeasible"] += not bool(len(ids))
    totals["prunes"] += audit.steps
    totals["maximum_case_nodes"] = max(totals["maximum_case_nodes"], stats.search_nodes)
    totals["maximum_case_branches"] = max(totals["maximum_case_branches"], stats.branches)
    u32(out, len(ids)); out.write(np.asarray(ids, dtype="<u4").tobytes())
    out.write(bytes(domains)); out.write(bytes([root is not None]))
    out.write(bytes(root if root is not None else [0] * 8)); out.write(bytes(expected))


def export_palette(output, name, palette):
    cells, worlds, factors, images, cameras, raw_rays, _ = oracle.build_observations((2, 2, 2), palette, 8)
    suites = oracle.view_suites(cameras)
    totals = dict.fromkeys(("cases", "infeasible", "witnesses", "direct_queries", "prunes",
                           "maximum_case_nodes", "maximum_case_branches", "empty_domain_inputs",
                           "empty_domain_query_inputs"), 0)
    sizes = {}
    with (output / f"{name}_images.bin").open("wb") as out:
        # Raw ray-major outputs, including duplicates; compare all 512 in C++.
        for spec in oracle.camera_specs((2, 2, 2)):
            for c, d in oracle.pixels(spec, 8):
                out.write(oracle.reference_ray(c, d, cells, worlds, palette).tobytes())
    with (output / f"{name}_cases.bin").open("wb") as out:
        out.write(b"MCRA2C1\n"); u32(out, len(factors))
        for f in factors:
            write_factor(out, f)
        u32(out, len(suites))
        for name_suite, indices, views in suites:
            groups = oracle.grouped(images, indices)
            sizes[name_suite] = len(groups)
            write_text(out, name_suite); u32(out, views); u32(out, len(indices))
            for r in indices:
                u32(out, r)
            u32(out, len(groups))
            chosen = [factors[r] for r in indices]
            for signature, ids in groups.items():
                check_case(out, worlds, chosen, list(signature), [7]*8, ids, totals, full=True)
            print(f"A2 {name} {name_suite}: {len(groups)} complete families", flush=True)
        rng = np.random.default_rng(20260927)
        u32(out, 1024)
        for case in range(1024):
            indices = list(suites[case % 8][1])
            truth = int(rng.integers(6561))
            labels = [int(x) for x in images[truth, indices]]
            if case % 3 == 0:
                j = int(rng.integers(len(indices))); labels[j] = (labels[j]+1) % 3
            if case % 31 == 0:
                j = next(j for j, r in enumerate(indices) if factors[r].cells)
                indices.append(indices[j]); labels.append((labels[j]+1) % 3)
            domains = [int(rng.integers(1, 8)) for _ in cells]
            if case % 4 == 0:
                domains = [m | (1 << int(s)) for m, s in zip(domains, worlds[truth])]
            if case % 37 == 0:
                domains[case % 8] = 0
            selected = np.all(images[:, indices] == labels, axis=1)
            for v, mask in enumerate(domains):
                selected &= ((1 << worlds[:, v]) & mask) != 0
            ids = np.flatnonzero(selected)
            u32(out, len(indices))
            for r, label in zip(indices, labels):
                u32(out, r); out.write(bytes([label]))
            check_case(out, worlds, [factors[r] for r in indices], labels, domains, ids, totals)
        # Frozen physical AB/AC/BC geometry, lifted to all eight cells with five air domains.
        rays = []
        for pair in ("AB", "AC", "BC"):
            for height, target in ((Q(1,4), 2), (Q(3,4), palette[2][1])):
                if pair == "AB": c, d = (Q(-1),height,Q(1,2)), (Q(1),Q(0),Q(0))
                elif pair == "AC": c, d = (Q(1,2),height,Q(-1)), (Q(0),Q(0),Q(1))
                else: c, d = (Q(3),height,Q(-1)), (Q(-1),Q(0),Q(1))
                rays.append((oracle.make_factor(c,d,(2,2,2),cells,palette),
                             oracle.reference_ray(c,d,cells,worlds,palette), target))
        for count in (4, 6):
            chosen = [x[0] for x in rays[:count]]
            labels = [x[2] for x in rays[:count]]
            selected = np.all(np.stack([x[1] for x in rays[:count]], axis=1) == labels, axis=1)
            active = [cells.index(p) for p in ((0,0,0),(1,0,0),(0,0,1))]
            for encoded in range(512):
                domains = [1]*8
                for j, v in enumerate(active): domains[v] = (encoded >> (3*j)) & 7
                keep = selected.copy()
                for v, mask in enumerate(domains): keep &= ((1 << worlds[:, v]) & mask) != 0
                ids = np.flatnonzero(keep)
                if encoded == 511:
                    assert len(ids) == (2 if count == 4 else 0)
                    assert oracle.gac(domains, chosen, labels, oracle.Stats()) == domains
                check_case(out, worlds, chosen, labels, domains, ids, totals)
    return {**totals, "worlds": len(worlds), "raw_rays": raw_rays, "distinct_factors": len(factors),
            "classes_by_suite": sizes, "camera_families": sum(sizes.values()),
            "restricted_camera_cases": 1024, "physical_domain_cases": 1024}


def export_boundaries(output):
    xs = tuple(map(Q, (-1, 0, Q(1,2), 1, 2)))
    ys = tuple(map(Q, (-1, 0, Q(499,1000), Q(1,2), Q(501,1000), 1, 2)))
    directions = [d for d in itertools.product((-1,0,1), repeat=3) if any(d)]
    worlds = np.array([[0],[1],[2]], dtype=np.uint8)
    count = len(xs)**2 * len(ys) * len(directions)
    with (output / "boundaries.txt").open("w") as out:
        out.write(f"MCRA2B1 {count}\n")
        for palette in PALETTES.values():
            for c in itertools.product(xs, ys, xs):
                for direction in directions:
                    d = tuple(map(Q, direction))
                    labels = oracle.reference_ray(c,d,((0,0,0),),worlds,palette)
                    f = oracle.make_factor(c,d,(1,1,1),((0,0,0),),palette)
                    assert np.array_equal(labels, oracle.chain_render(f,worlds))
                    out.write(" ".join(str(i) for q in (*c,*d) for i in (q.numerator,q.denominator)))
                    out.write(" " + " ".join(map(str, labels)) + "\n")
    return {"rays_per_palette": count, "ray_world_palette_comparisons": 2*count*3}


def export_local(output):
    with (output / "local_supports.bin").open("wb") as out:
        out.write(b"MCRA2F1\n"); u32(out, 2*64*512*3)
        for palette in PALETTES.values():
            for flags in range(64):
                emissions = tuple((0, 2 if flags & (1 << (2*j)) else 0,
                                   palette[2][1] if flags & (2 << (2*j)) else 0) for j in range(3))
                factor = oracle.RayFactor((2,0,1), emissions)
                for encoded in range(512):
                    domains = [(encoded >> (3*v)) & 7 for v in range(3)]
                    for target in range(3):
                        masks = oracle.factor_supports(factor, domains, target)
                        out.write(bytes([masks is not None, *(masks if masks is not None else [0]*3)]))
    return 2*64*512*3


def export(output):
    check_frozen_reference()
    output.mkdir(parents=True, exist_ok=True)
    fingerprint = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in (Path(__file__), Path(__file__).with_name("export_a0.py"), REFERENCE/"phase_a.py")}
    metadata = output / "metadata.json"
    if metadata.exists():
        saved = json.loads(metadata.read_text())
        if (saved.get("input_sha256") == fingerprint and saved.get("schema") == "MCRA2C1"
                and all((output/p).exists() and hashlib.sha256((output/p).read_bytes()).hexdigest() == h
                        for p, h in saved.get("fixtures_sha256", {}).items())
                and len(saved.get("fixtures_sha256", {})) == 6):
            print("Verified cached complete two-palette A2 oracle", flush=True)
            return
    start = time.perf_counter()
    meta = {"schema": "MCRA2C1", "input_sha256": fingerprint, "python": sys.version, "numpy": np.__version__}
    f = oracle.RayFactor((0,), ((0,2,1),))
    raw_witness = oracle.solve([2,0], [f], [2], oracle.Stats())
    assert raw_witness == (1,0)  # documented frozen-reference limitation
    meta["frozen_empty_domain_regression"] = {
        "domains": [2,0], "factor_cells": [0], "emissions": [[0,2,1]], "target": 2,
        "raw_python_witness": raw_witness, "exhaustive_feasible_worlds": 0,
        "adapter_handling": "Reject any empty input domain before calling the frozen inference routines."}
    meta["palettes"] = {name: export_palette(output, name, palette) for name, palette in PALETTES.items()}
    meta["boundaries"] = export_boundaries(output)
    meta["local_support_cases"] = export_local(output)
    files = [f"{p}_{s}.bin" for p in PALETTES for s in ("cases", "images")] + ["boundaries.txt", "local_supports.bin"]
    meta["fixtures_sha256"] = {p: hashlib.sha256((output/p).read_bytes()).hexdigest() for p in files}
    meta["export_seconds"] = time.perf_counter()-start
    temporary = metadata.with_suffix(".tmp")
    temporary.write_text(json.dumps(meta, indent=2)+"\n"); temporary.replace(metadata)
    print(json.dumps(meta), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
