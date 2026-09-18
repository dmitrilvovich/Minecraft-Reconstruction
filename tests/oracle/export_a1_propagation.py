#!/usr/bin/env python3
"""Export existing frozen GAC results for the shared multi-ray test corpus."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
from export_a0 import ROOT, oracle, check_frozen_reference, REFERENCE


def export(output):
    check_frozen_reference()
    tokens = iter((ROOT / "tests/fixtures/a1_propagation.txt").read_text().split())
    assert next(tokens) == "MCRA1P1"
    scenes = []
    for _ in range(int(next(tokens))):
        name, n, count = next(tokens), int(next(tokens)), int(next(tokens))
        factors, labels = [], []
        for _ in range(count):
            label, length = int(next(tokens)), int(next(tokens))
            steps = [(int(next(tokens)), int(next(tokens))) for _ in range(length)]
            factors.append(oracle.RayFactor(tuple(v for v, _ in steps),
                           tuple((0, 1, 2, 2 if hit else 0) for _, hit in steps)))
            labels.append(label)
        # Frozen gac has no separate guard for an empty domain in an unused cell.
        # Every cell in this comparison corpus is in a ray; C++ tests cover that
        # additional scene-level guard independently without modifying the oracle.
        assert {v for f in factors for v in f.cells} == set(range(n))
        scenes.append((name, n, factors, labels))
    assert next(tokens, None) is None
    output.mkdir(parents=True, exist_ok=True)
    count = sum(16**n for _, n, _, _ in scenes)
    consistent = 0
    path = output / "propagation.bin"
    with path.open("wb") as stream:
        stream.write(b"MCRA1G1\n" + struct.pack("<I", count))
        for _, n, factors, labels in scenes:
            for encoded in range(16**n):
                domains = [(encoded >> (4*v)) & 15 for v in range(n)]
                result = oracle.gac(domains, factors, labels, oracle.Stats())
                consistent += result is not None
                stream.write(bytes([result is not None, *(result if result is not None else [0]*n)]))
    metadata = {"cases": count, "consistent": consistent, "inconsistent": count-consistent,
                "scene_names": [name for name, *_ in scenes],
                "fixture_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest()}
    (output / "propagation_metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
