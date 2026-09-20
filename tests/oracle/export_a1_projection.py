#!/usr/bin/env python3
"""Aggregate the frozen reference's existing single-query fixture into marginals."""
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
    raw = (output / "support_query.bin").read_bytes()
    source = json.loads((output / "support_query_metadata.json").read_text())
    assert hashlib.sha256(raw).hexdigest() == source["fixture_sha256"]
    assert raw[:8] == b"MCRA1Q1\n" and struct.unpack_from("<I", raw, 8)[0] == source["queries"]
    assert source["branches"] == source["search_nodes"] == 0
    count = sum(15**n for _, n, _, _ in scenes)
    data = bytearray(b"MCRA1P2\n" + struct.pack("<I", count))
    position = 12
    feasible = literals = comparisons = 0
    for _, n, factors, labels in scenes:
        assert n > 0
        for encoded in range(15**n):
            domains = [1 + (encoded // 15**v) % 15 for v in range(n)]
            support = [0]*n
            for v in range(n):
                for s in range(4):
                    if not domains[v] & (1 << s):
                        continue
                    assert position + n + 1 <= len(raw)
                    present, witness = raw[position], raw[position+1:position+n+1]
                    position += n + 1
                    assert present in (0, 1)
                    if present:
                        assert witness[v] == s and all(domains[j] & (1 << t) for j, t in enumerate(witness))
                        support[v] |= 1 << s
                        literals += 1
            possible = any(support)
            assert not possible or all(support)
            feasible += possible
            data.extend([possible, *support])
            if encoded == 15**n - 1:
                # Also compare the original unrestricted research projection.
                stats = oracle.Stats()
                _, expected, _ = oracle.nested_supports(n, 4, factors, labels, stats, ranks=(0, 2, 2, 1))
                assert support == (expected if expected is not None else [0]*n)
                assert stats.branches == stats.search_nodes == 0
                comparisons += 1
    assert position == len(raw) and literals == source["supported"]
    path = output / "projection.bin"
    path.write_bytes(data)
    metadata = {"cases": count, "feasible": feasible, "infeasible": count-feasible,
                "supported_literals": literals, "source_single_queries": source["queries"],
                "unrestricted_nested_supports_comparisons": comparisons,
                "scope": "all nonempty domain triples in three shared-ray fixtures",
                "source_fixture_sha256": source["fixture_sha256"],
                "fixture_sha256": hashlib.sha256(data).hexdigest(),
                "reference_sha256": hashlib.sha256((REFERENCE / "phase_a.py").read_bytes()).hexdigest()}
    (output / "projection_metadata.json").write_text(json.dumps(metadata, indent=2)+"\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    export(parser.parse_args().output)
