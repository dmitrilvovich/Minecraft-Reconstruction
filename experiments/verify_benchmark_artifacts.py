#!/usr/bin/env python3
"""Check the saved 2D1 evidence without running a solver or benchmark.

For the independent frozen-reference replay, decompress the A1 CSVs and use
analyze_a1_benchmark.py as documented in docs/milestone-2d1-benchmarks.md.
"""
from pathlib import Path
import argparse
import csv
import gzip
import hashlib
import io
import json
import math


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def verify(directory):
    report = {}
    for model, expected in (("a0", 29032), ("a1", 196132)):
        root = directory / model
        manifest = json.loads((root / "raw-sha256.json").read_text())
        require(set(manifest) == {p.name for p in root.glob("*.csv.gz")}, "archive set mismatch")
        count, groups = 0, {}
        timed_ns, witnesses, feasible = 0, 0, 0
        for name, meta in manifest.items():
            compressed = (root / name).read_bytes()
            require(hashlib.sha256(compressed).hexdigest() == meta["sha256"], f"compressed hash: {name}")
            raw = gzip.decompress(compressed)  # also checks gzip CRC and length
            require(len(raw) == meta["uncompressed_bytes"], f"byte count: {name}")
            require(hashlib.sha256(raw).hexdigest() == meta["uncompressed_sha256"], f"raw hash: {name}")
            require(raw.endswith(b"\n"), f"unfinished final row: {name}")
            rows = csv.reader(io.StringIO(raw.decode("utf-8")))
            header = next(rows)
            require(len(header) == len(set(header)), f"duplicate columns: {name}")
            for row in rows:
                require(len(row) == len(header), f"row width: {name}")
                if model == "a1":
                    require(len(header) == 61, "wrong A1 schema/repetition count")
                    require(header[-5:] == [f"solver_ns_{r}" for r in range(5)], "missing repetitions")
                    value = dict(zip(header, map(int, row)))
                    samples = [value[f"solver_ns_{r}"] for r in range(5)]
                    require(min(samples) > 0, "nonpositive duration")
                    timed_ns += sum(samples)
                    witnesses += value["witnesses"]
                    feasible += value["feasible"]
                    for stage in ("gac", "envelope", "projection"):
                        require(value[stage+"_branches"] == value[stage+"_search_nodes"] == 0, "nonzero search")
                    require(value["projection_envelope_calls"] == 1 + value["projection_literal_queries"], "work accounting")
                    for stage, masks in (("initial", "input"), ("gac", "gac"), ("envelope", "envelope"), ("exact", "supported")):
                        require(value[stage+"_candidates"] == value[masks+"_masks"].bit_count(), "candidate accounting")
                    suite = name.split("_part")[0]
                    key = int(row[0])
                else:
                    suite, key = row[0], int(row[1])
                keys = groups.setdefault(suite, set())
                require(key not in keys, f"duplicate {suite}/{key}")
                keys.add(key)
                count += 1
        summary = json.loads((root / "summary.json").read_text())
        require(summary["status"] == "pass" and summary["repetitions"] == 5, "wrong run status")
        require(count == expected, f"wrong {model} row count")
        expected_groups = {s["name"]: s["cases" if model == "a1" else "classes"] for s in summary["suites"]}
        require({s: len(ids) for s, ids in groups.items()} == expected_groups, "suite counts disagree")
        report[model] = {"archives": len(manifest), "rows": count, "suites": expected_groups}
        if model == "a1":
            run = json.loads((root / "run.json").read_text())
            require(run == summary["run"], "run metadata differs from summary")
            require(summary["validated_rows"] == count and summary["timed_calls"] == 5*count, "coverage accounting")
            require(witnesses == run["verified_warmup_projection_witnesses"] and feasible == run["feasible_cases"], "validation accounting")
            require(math.isclose(timed_ns*1e-9, run["timed_projection_seconds"], rel_tol=1e-9), "timing accounting")
            report[model].update(timed_calls=5*count, verified_warmup_projection_witnesses=witnesses,
                                 feasible_cases=feasible, search_branches=0, search_nodes=0)
    report["status"] = "pass"
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.directory), indent=2))
