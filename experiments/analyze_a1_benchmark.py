#!/usr/bin/env python3
"""Validate C++ benchmark rows against accepted frozen fixtures and summarize data.

No reconstruction solver is implemented or extended here. Timings come solely
from C++; reference parsing, statistics, comparison and compression are offline.
"""
from pathlib import Path
import argparse
import csv
import gzip
import hashlib
import json
import math
import shutil
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests/oracle"))
from export_a0 import check_frozen_reference

NAMES = [f"axis_{v}" for v in range(1, 7)] + ["oblique_1", "oblique_2"]
WORK = ["gac_calls", "fixed_calls", "factor_updates", "deletions", "literal_queries",
        "infeasible_queries", "envelope_calls", "envelope_advances", "branches", "search_nodes"]


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unpack(n):
    return [(int(n) >> (4*v)) & 15 for v in range(8)]


def packed(masks):
    return sum(int(m) << (4*v) for v, m in enumerate(masks))


def distribution(values, scale=1):
    a = np.sort(np.asarray(values, dtype=np.float64) / scale)
    require(len(a) > 0 and np.all(np.isfinite(a)), "bad distribution")
    return {"min": float(a[0]), "median": float(a[int(.5*(len(a)-1))]),
            "p95": float(a[int(.95*(len(a)-1))]), "p99": float(a[int(.99*(len(a)-1))]),
            "max": float(a[-1]), "mean": float(np.mean(a))}


def table(paths, repetitions):
    require(bool(paths), "missing raw CSV parts")
    with paths[0].open() as f:
        names = next(csv.reader(f))
    parts = []
    for path in paths:
        with path.open() as f:
            require(next(csv.reader(f)) == names, "part header mismatch")
        parts.append(np.loadtxt(path, delimiter=",", skiprows=1, dtype=np.int64, ndmin=2))
    data = np.concatenate(parts)
    require(data.shape[1] == len(names) and len(set(names)) == len(names), "bad CSV schema")
    require(names[-repetitions:] == [f"solver_ns_{r}" for r in range(repetitions)], "missing timing samples")
    require(np.all(data[:, -repetitions:] > 0), "nonpositive duration")
    return data, {name: j for j, name in enumerate(names)}


def main(args):
    check_frozen_reference()
    raw, oracle_dir, output = args.raw, args.oracle, args.output
    output.mkdir(parents=True, exist_ok=True)
    run = json.loads((raw / "run.json").read_text())
    require(run["status"] == "pass" and run["build_type"] == "Release" and run["limit_per_suite"] == 0, "not a complete Release run")
    require(run["rendered_cases"] == 195620 and run["restricted_cases"] == 512, "incomplete benchmark")
    require(run["search_branches"] == run["search_nodes"] == 0 and run["steady_clock_is_steady"], "bad execution mode")
    repetitions = run["repetitions"]
    meta = json.loads((oracle_dir / "acceptance_metadata.json").read_text())
    accepted = json.loads((ROOT / "results/milestone-2c-correctness/release-oracle/oracle_a1-acceptance_metadata.json").read_text())
    require(sha(oracle_dir / "acceptance_cases.bin") == meta["fixture_sha256"] == accepted["fixture_sha256"], "fixture is not the accepted data")
    summaries, files = [], []
    total, witnesses, timed_ns = 0, 0, 0

    with (oracle_dir / "acceptance_cases.bin").open("rb") as fixture:
        def read(n):
            x = fixture.read(n)
            require(len(x) == n, "truncated reference fixture")
            return x

        def u32():
            return struct.unpack("<I", read(4))[0]

        require(read(8) == b"MCRA1C1\n", "wrong fixture schema")
        for _ in range(u32()):
            read(5*u32())
        require(u32() == 8, "wrong view count")
        for suite_index in range(9):
            restricted = suite_index == 8
            if restricted:
                name, views, count = "restricted", None, u32()
                require(count == 512, "wrong restricted count")
            else:
                name = read(u32()).decode("ascii")
                views = u32()
                read(4*u32())
                count = u32()
                require(name == NAMES[suite_index], "wrong suite order")
            paths = sorted(raw.glob(name+"_part*.csv"))
            data, col = table(paths, repetitions)
            require(len(data) == count and len(set(map(int, data[:, col["case"]]))) == count, "duplicate or missing rows")
            rows = {int(row[col["case"]]): row for row in data}
            for case in range(count):
                if restricted:
                    read(5*u32())
                    initial = list(read(8))
                    feasible = bool(read(1)[0])
                    support = list(read(8))
                    ids = np.frombuffer(read(4*u32()), dtype="<u4")
                    python_gac = None
                    free = 0
                    if len(ids):
                        # Product/fiber definition relative to original restricted domains.
                        for v, domain in enumerate(initial):
                            states = domain.bit_count()
                            _, sizes = np.unique(ids & np.uint32(~(3 << (2*v)) & 0xffffffff), return_counts=True)
                            free += bool(np.all(sizes == states))
                    key = case
                else:
                    ids = np.frombuffer(read(4*u32()), dtype="<u4")
                    python_gac, python_support, support = list(read(8)), list(read(8)), list(read(8))
                    free = read(1)[0]
                    occurrences = np.frombuffer(read(32*4), dtype="<u4").reshape(8, 4)
                    require(support == python_support, "frozen support mismatch")
                    require(all(sum(int(occurrences[v, s]) > 0 for s in range(4)) == support[v].bit_count() for v in range(8)), "occurrence support mismatch")
                    initial, feasible, key = [15]*8, True, int(ids[0])
                row = rows.pop(key)
                value = lambda k: int(row[col[k]])
                require(feasible == bool(len(ids)) and value("feasible") == feasible, "wrong feasibility")
                require(value("feasible_worlds") == len(ids) and value("input_masks") == packed(initial) and value("supported_masks") == packed(support), "wrong family/domains")
                gac, envelope = unpack(value("gac_masks")), unpack(value("envelope_masks"))
                if python_gac is not None:
                    require(gac == python_gac, "wrong GAC masks")
                require(all(s & i == s and g & i == g and e & i == e for s, g, e, i in zip(support, gac, envelope, initial)), "domain expansion")
                require(all(s & e == s and s & g == s for s, e, g in zip(support, envelope, gac)), "lost support")
                ranks = [{(0, 2, 2, 1)[s] for s in range(4) if m & (1 << s)} for m in support]
                expected = {"initial_candidates": sum(m.bit_count() for m in initial),
                            "gac_candidates": sum(m.bit_count() for m in gac),
                            "envelope_candidates": sum(m.bit_count() for m in envelope),
                            "exact_candidates": sum(m.bit_count() for m in support),
                            "identified_cells": sum(m.bit_count() == 1 for m in support),
                            "free_cells": free, "geometry_cells": sum(len(r) == 1 for r in ranks),
                            "cube_slab_cells": sum(bool(m & 8) and bool(m & 6) for m in support)}
                for k, expected_value in expected.items():
                    require(value(k) == expected_value, f"wrong {name}/{key}/{k}")
                if feasible:
                    require(value("gac_consistent") == 1, "GAC rejected feasible family")
                for stage in ("gac", "envelope", "projection"):
                    require(value(stage + "_branches") == value(stage + "_search_nodes") == 0, "unexpected search")
                require(value("envelope_envelope_calls") == 1 and value("envelope_literal_queries") == 0, "wrong feasibility work")
                require(value("projection_gac_calls") == 0 and value("projection_envelope_calls") == 1 + value("projection_literal_queries"), "wrong projection work")
                require(value("witnesses") == (1 + value("projection_literal_queries") - value("projection_infeasible_queries") if feasible else 0), "wrong witnesses")
                slab_counts = np.zeros(len(ids), dtype=np.uint8)
                for v in range(8):
                    slab_counts += ((ids >> (2*v)) & 3) == 3
                weights = np.bincount(slab_counts, minlength=9)
                require(all(value(f"truth_slabs_{k}") == weights[k] for k in range(9)), "wrong slab stratum weights")
                witnesses += value("witnesses")
            require(not rows, "extra benchmark rows")
            total += len(data)
            field = lambda k: data[:, col[k]]
            times = data[:, -repetitions:]
            medians = np.sort(times, axis=1)[:, (repetitions-1)//2]
            timed_ns += int(times.sum())
            support_work = {}
            for stage in ["gac", "envelope", "projection"]:
                support_work[stage] = {k: {**distribution(field(stage+"_"+k)), "sum": int(field(stage+"_"+k).sum())} for k in WORK}
            item = {"name": name, "views": views, "cases": count, "feasible_cases": int(field("feasible").sum()),
                    "projection_us_per_case_medians": distribution(medians, 1000),
                    "projection_us_all_repetitions": distribution(times.ravel(), 1000),
                    "median_within_case_min_us": float(np.median(times.min(axis=1)) / 1000),
                    "median_within_case_max_us": float(np.median(times.max(axis=1)) / 1000),
                    "work_per_case": support_work,
                    "candidates_per_case": {k: distribution(field(k+"_candidates")) for k in ["initial", "gac", "envelope", "exact"]}}
            if not restricted:
                weights = field("feasible_worlds")
                require(int(weights.sum()) == 65536, "wrong truth weighting")
                def weighted(w):
                    worlds = int(w.sum())
                    return {"worlds": worlds, **{k: float(np.dot(w, field(column))/(8*worlds)) for k, column in [
                        ("identifiable_cell_fraction", "identified_cells"), ("freely_variable_cell_fraction", "free_cells"),
                        ("identifiable_geometry_fraction", "geometry_cells"), ("cube_slab_ambiguous_cell_fraction", "cube_slab_cells")]},
                        "unique_world_fraction": float(np.dot(w, weights == 1)/worlds)}
                item.update(weighted(weights))
                item["feasible_family_size_class_weighted"] = distribution(weights)
                item["identified_cells_class_weighted_histogram"] = np.bincount(field("identified_cells"), minlength=9).tolist()
                item["by_true_slab_count"] = []
                for k in range(9):
                    w = field(f"truth_slabs_{k}")
                    require(int(w.sum()) == math.comb(8, k)*3**(8-k), "incomplete stratum")
                    item["by_true_slab_count"].append({"slabs": k, **weighted(w)})
                item["unsupported_after_standalone_gac_total"] = int((field("gac_candidates")-field("exact_candidates")).sum())
                item["unsupported_after_envelope_total"] = int((field("envelope_candidates")-field("exact_candidates")).sum())
            else:
                item["by_feasibility"] = [{"feasible": bool(f), "cases": int((field("feasible")==f).sum()),
                    "projection_us_per_case_medians": distribution(medians[field("feasible")==f], 1000)} for f in [0, 1]]
                item["identifiability_note"] = "No population fractions: these are seeded restricted/altered instances, not a truth prior. Empty-family row counts of zero are not identification claims."
            summaries.append(item)
            files.extend(paths)
        require(fixture.read() == b"", "unconsumed reference bytes")
    require(total == 196132 and witnesses == run["verified_warmup_projection_witnesses"], "wrong total coverage")
    require(math.isclose(timed_ns*1e-9, run["timed_projection_seconds"], rel_tol=1e-9), "timing accounting mismatch")
    historical = json.loads((ROOT / "reference/python/results/results.json").read_text())
    baseline = json.loads((ROOT / "reference/python/baseline_search/results.json").read_text())
    a1 = next(p for p in historical["phases"] if p["phase"] == "A1")
    search = next(p for p in baseline["phases"] if p["phase"] == "A1")
    a0 = json.loads((args.a0 / "summary.json").read_text())
    require(a0["status"] == "pass" and a0["classes"] == 29032 and a0["repetitions"] == repetitions, "incomplete A0 comparison")
    comparison = []
    for new, old, zero, cpp0 in zip(summaries[:8], a1["suites"], search["suites"], a0["suites"]):
        require(new["name"] == old["suite"] == zero["suite"] == cpp0["name"], "comparison suite mismatch")
        for new_key, old_key in [("identifiable_cell_fraction", "oracle_identifiable_fraction_uniform_world"),
                                  ("freely_variable_cell_fraction", "oracle_freely_variable_fraction_uniform_world"),
                                  ("unique_world_fraction", "oracle_unique_world_fraction")]:
            require(math.isclose(new[new_key], old[old_key], abs_tol=1e-12), "historical oracle disagreement")
        for k, stratum in enumerate(new["by_true_slab_count"]):
            old_s = old["oracle_by_true_slab_count"][str(k)]
            require(stratum["worlds"] == old_s["worlds"] and math.isclose(stratum["identifiable_cell_fraction"], old_s["identifiable_fraction"], abs_tol=1e-12) and math.isclose(stratum["unique_world_fraction"], old_s["unique_world_fraction"], abs_tol=1e-12), "historical slab-stratum disagreement")
        comparison.append({"suite": new["name"], "cpp_a0_median_us": cpp0["median_solver_us"],
            "cpp_a1_median_us": new["projection_us_per_case_medians"]["median"],
            "a0_identifiable_fraction": cpp0["identifiable_cell_fraction"],
            "a1_same_cube_truths_identifiable_fraction": new["by_true_slab_count"][0]["identifiable_cell_fraction"],
            "python_nested_tested_cases": old["tested_classes"], "python_nested_median_us": old["solver_seconds_median"]*1e6,
            "python_nested_max_branches": old["tested_max_branches"], "python_generic_max_branches": zero["tested_max_branches"]})
    report = {"status": "pass", "repetitions": repetitions, "validated_rows": total, "timed_calls": total*repetitions,
        "timing_quantiles": "floor(p*(n-1)); across observation-class medians unless all_repetitions is stated",
        "identifiability_weighting": "uniform over all 65,536 truth worlds; also stratified by true slab count; inference always uses full A1 vocabulary",
        "root_stage": run["root_stage"], "run": run, "suites": summaries,
        "comparison": comparison, "comparison_limit": "A0/A1 timings use different populations and algorithms. Historical Python uses different sampling, implementation, repetitions and environment. No controlled language speedup or scalability claim.",
        "reference_fixture_sha256": meta["fixture_sha256"], "historical_oracle_agreement": True}
    (output / "summary.json").write_text(json.dumps(report, indent=2)+"\n")
    shutil.copyfile(raw / "run.json", output / "run.json")
    # Retain every raw repetition. Deterministic gzip keeps the finite exhaustive
    # corpus compact without discarding cases, samples, masks or work counters.
    raw_meta = {}
    for path in files:
        target = output / (path.name+".gz")
        temporary = target.with_suffix(target.suffix+".tmp")
        with temporary.open("wb") as dst, gzip.GzipFile(filename="", mode="wb", fileobj=dst, mtime=0) as packed_file, path.open("rb") as src:
            shutil.copyfileobj(src, packed_file)
        require(hashlib.sha256(gzip.decompress(temporary.read_bytes())).hexdigest() == sha(path), "compressed raw data changed")
        temporary.replace(target)
        raw_meta[target.name] = {"sha256": sha(target), "uncompressed_sha256": sha(path), "uncompressed_bytes": path.stat().st_size}
    (output / "raw-sha256.json").write_text(json.dumps(raw_meta, indent=2)+"\n")
    print(json.dumps({"validated_rows": total, "timed_calls": total*repetitions, "verified_warmup_projection_witnesses": witnesses, "frozen_oracle_and_strata_agreement": True, "zero_search": True}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ["raw", "oracle", "a0", "output"]:
        parser.add_argument("--"+name, type=Path, required=True)
    main(parser.parse_args())
