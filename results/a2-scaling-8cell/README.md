# Canonical reconstructed exploratory 8-cell A2 dataset

This directory preserves the **491,199-case reconstructed exploratory dataset**.
It is not the recovered original 410,953-case run, is not proven equivalent to
that run, and is not a verified correction or superset. The protocol was
explicitly reconstructed after the original artifacts were lost. No claim
depends on reproducing the historical count. See the authoritative
[adoption and reconciliation record](../../docs/a2-scaling-adoption.md).

## Reproduction and audit artifacts

- `protocol.json`, `provenance.json`, and `collection.json`: exact replacement
  choices, source/executable fingerprints, and completion/population metadata.
- `raw/manifest.json` and `raw/raw.tar.part-000` through `raw.tar.part-014`:
  hash-checked split archive. It contains job manifests, logs, saved spot-check
  inputs, and lossless columns for all 148 raw CSV tables. `transport.json`
  inside the archive records the 373 archived files and original table hashes.
- `collection.log`, `build.log`, `budget-check.log`, `smoke.json`,
  `python-check.json`, `python-check.log`, `validation.json`, `packing.log`,
  `unpacking.log`, and `summary-integrity.json`: existing collection, validation,
  and integrity evidence. These records describe earlier work; none of those
  solver checks is rerun by adoption.
- `adoption-integrity.json`: file-preservation, archive/hash, and population
  accounting checks for this publication checkpoint, without solver execution.
- `../../experiments/a2_scaling.cpp`, `build_a2_scaling.sh`,
  `run_a2_scaling.py`, `pack_a2_scaling.py`, `check_a2_scaling_python.py`, and
  `analyze_a2_scaling.py`: preserved collector and supporting scripts.

Population: 374,771 8x8 camera-family cases, 112,960 16x16 camera-family cases,
3,456 restricted/perturbed cases, and 12 structured controls: **491,199 cases**.

## Existing analysis: NOT YET SCIENTIFICALLY REVIEWED

`analysis.json`, `analysis.log`, `by-k.csv`, `by-placement-suite.csv`,
`sampled-queries.csv`, `structured.csv`, `worst-cases.csv`, `scaling.png`, and
`scaling.svg` are retained exactly as they existed before adoption. Their
preservation and hash verification do not endorse their scientific
interpretations. The preliminary narrative in `../../docs/a2-scaling-8cell.md`
has the same review status. Review belongs to a separate checkpoint.

Occupancy-balanced summaries only reweight existing measurements. Observation
perturbations can add impossible stone labels to the oak-only palette and remain
robustness controls, not clean material-relabeling comparisons. There is no
defensible subset matching the lost 410,953-case run.

To restore the saved files without inference, use `pack_a2_scaling.py unpack` as
documented in the preliminary note. `check_a2_scaling_python.py` **does invoke
the frozen solver**; it is a deliberate validation rerun, not a read-only
inspection command. `run_a2_scaling.py` runs measurements. Neither script is
executed for this publication.
