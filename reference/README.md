# Frozen Python research reference

`python/` is the complete original Phase A experiment archive, imported without
source changes. It includes the A0/A1/A2 research implementations, protocol,
fixtures, and recorded results. The current C++ milestone implements **A0 only**.

`SHA256.json` locks every imported file. New reference-export adapters belong in
`tests/oracle/`, not in this directory. The reference is never linked into the
C++ solver; it is an independent exhaustive regression oracle. Python and NumPy
are needed only for cross-language validation.

Provenance: `phase_a_experiment.zip`, SHA-256
`3884bf7d7440b543c5091818119322c4ff199dfd9d0e8497faf22a976d8e63cf`.
The protocol matches the latest supplied `Phase_A_protocol(1).md`.

