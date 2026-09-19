#!/usr/bin/env python3
"""Export one witness from frozen envelope_feasible; no support queries or solver changes."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
from export_a0 import oracle, check_frozen_reference, REFERENCE
from export_a1_propagation import load_scenes


def export(output):
    check_frozen_reference()
    scenes = load_scenes()
    output.mkdir(parents=True, exist_ok=True)
    count = sum(15**n for _, n, _, _ in scenes)
    feasible = 0
    path = output / "feasibility.bin"
    with path.open("wb") as stream:
        stream.write(b"MCRA1E1\n" + struct.pack("<I", count))
        for _, n, factors, labels in scenes:
            for encoded in range(15**n):
                # Frozen envelope_feasible expects nonempty masks. Empty masks
                # are exhaustively covered by the independent C++ world oracle.
                domains = [1 + (encoded // 15**v) % 15 for v in range(n)]
                stats = oracle.Stats()
                witness = oracle.envelope_feasible(domains, factors, labels, (0, 2, 2, 1), stats)
                assert stats.branches == stats.search_nodes == stats.literal_queries == 0
                feasible += witness is not None
                stream.write(bytes([witness is not None, *(witness if witness is not None else [0]*n)]))
    metadata = {"cases": count, "feasible": feasible, "infeasible": count-feasible,
                "scene_names": [name for name, *_ in scenes], "domain_masks": "all nonempty triples",
                "branches": 0, "search_nodes": 0, "support_queries": 0,
                "fixture_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest()}
    (output / "feasibility_metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
