# Frozen inputs for larger-volume A2 scaling v1

**Inputs and infrastructure only. No main solver collection or timing epoch has run.**

- [Input manifest](inputs/input-manifest.json): exactly 72,148 design slots,
  21 case archives, 19 shared rational-ray archives, complete hashes and cohort mappings.
- [Build identity](build-identity.json): collector/reference source hashes, compiler,
  host, dependencies and the frozen binary hash.
- [Frozen worker](worker-linux-x86_64.gz): gzip-compressed x86-64 Linux executable;
  decompress to the intended `build/a2-larger/worker` path and verify its SHA-256
  against `build-identity.json` before use. Decompression does not execute it.
- [Preparation feasibility](preparation-feasibility.json): measured infrastructure
  costs, explicit extrapolation formulas, uncertainty and storage allowances.
- [Input-generation log](input-generation.log) and
  [cost receipt](inputs/preparation-receipt.json): input preparation without inference.
- [Integrity inventory](integrity.json): size, SHA-256 and Git blob identity of the
  published source, inputs, binary and preflight evidence. It excludes itself.
- [Temporary-file audit](temporary-file-audit.json): redundant interrupted-write
  files verified against their completed archives and quarantined before publication.
- [Preflight evidence](../a2-scaling-larger-preflight/README.md) and
  [implementation record](../../docs/a2-scaling-larger-preflight.md).

`input-manifest.json` is deterministic for the frozen binary and source. The
separate preparation receipt contains the measured, non-deterministic duration.
Every input archive has a `.complete.json` with its compressed/uncompressed hashes,
record count and provenance. The records retain duplicate truth/mask memberships;
only coincident resolution roles share a timing slot, as preregistered.

The cases are input declarations, not measured solver results. `expected_status`
records the constructive SAT truth or structured/contradiction design, not a solver
answer. Full references, root diagnostics and real timing epochs remain future work.
The 35,254 mandatory exhaustive checks are scheduled coverage, not checks claimed
to have been run in this checkpoint. Only the separate 40-case preflight was checked.

Collection requires a later authorized checkpoint, verified identities and the
declared resource controls. No analysis or architecture decision should use these
preflight solver timings as main-dataset measurements.
