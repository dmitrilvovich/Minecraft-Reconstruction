#!/usr/bin/env python3
"""Export individual conditioned feasibility calls from the frozen A1 reference."""
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
    # Each state occurs in eight of the fifteen nonempty masks. Excluded states
    # and empty masks are covered by C++ exhaustive tests, not fed to Python.
    count = sum(n * 4 * 8 * 15**(n-1) for _, n, _, _ in scenes)
    supported = calls = 0
    path = output / "support_query.bin"
    with path.open("wb") as stream:
        stream.write(b"MCRA1Q1\n" + struct.pack("<I", count))
        for _, n, factors, labels in scenes:
            for encoded in range(15**n):
                domains = [1 + (encoded // 15**v) % 15 for v in range(n)]
                for v in range(n):
                    for s in range(4):
                        if not domains[v] & (1 << s):
                            continue
                        probe = list(domains)
                        probe[v] = 1 << s
                        stats = oracle.Stats()
                        witness = oracle.envelope_feasible(probe, factors, labels, (0, 2, 2, 1), stats)
                        assert stats.envelope_calls == 1 and stats.branches == stats.search_nodes == 0
                        assert witness is None or witness[v] == s
                        supported += witness is not None
                        calls += 1
                        stream.write(bytes([witness is not None, *(witness if witness is not None else [0]*n)]))
    assert calls == count
    metadata = {"queries": calls, "supported": supported, "unsupported": calls-supported,
                "scene_names": [name for name, *_ in scenes],
                "scope": "all nonempty domain triples, all cells, every allowed state",
                "envelope_calls": calls, "branches": 0, "search_nodes": 0,
                "fixture_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest()}
    (output / "support_query_metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
