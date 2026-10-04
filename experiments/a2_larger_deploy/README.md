# Stable-host deployment v1

See the [deployment instructions](../../docs/a2-scaling-larger-stable-host.md).
The original experiment and scoped-launch directories remain frozen.

- `pre_collection.py`: single nonexperimental bootstrap/check/optional host-record
  command. Verifies the checkout, restores the packaged ELF by hash, verifies
  inputs and records machine/runtime/resources. Never starts the worker or oracle.
- `host.py`: metadata, library, resource and published-identity guards.
- `capability_probe.py`: small Python-only child for affinity, virtual-address
  ceiling and timeout termination checks; no experimental case IDs or API calls.
- `collect.py`: separately authorized future execution through the preserved
  scoped collection body, with the stronger host guard and shared-output binding.
- `validate.py`: nonexperimental anchor/deployment checks, with the real worker
  disabled. It is not the bounded solver preflight.
- `spec.json`: prospective deployment requirements and unchanged collection order.

Use the published anchor selection at
`results/a2-scaling-larger-stable-host/anchors/`. Do not regenerate its inputs.
The prepared host record must be published and verified before authorizing
anchors. Then publish/verify anchor evidence before authorizing N=4/6. All batches
share one stable environment, study directory and cumulative budget ledger.
