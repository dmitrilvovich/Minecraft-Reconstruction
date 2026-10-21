# Deployment amendment 2 evidence

- `validation.json`: 48 passing bounded nonexperimental metadata/guard checks;
  source and original artifact SHA-256 records; no solver/reference/epoch work.
- `reported-wsl-blocker.json`: user-reported laptop setup and prior compiler
  block, explicitly distinguished from observations on this validation host.
- `checkpoint-integrity.json`: source/evidence hashes and preserved-tree checks.

The [amendment](../../docs/a2-scaling-larger-deployment-amendment-2.md) records the
reason for separating original build provenance from the installed execution
compiler. Build/compiler/binary provenance is unchanged. WSL is not qualified
for timing by this deployment; no stable collection host has been selected.

No scientific input archives or selections were regenerated. Earlier deployment,
preflight and blocked-launch records are preserved. Validation host metadata is
not a collection-host freeze; synthetic host records never leave the test's
temporary directory. The user's WSL machine was not accessed during this work.
