# Deployment v2: execution identity distinct from build provenance

See the [prospective amendment](../../docs/a2-scaling-larger-deployment-amendment-2.md)
and [machine-readable policy](spec.json). This is the current deployment entry
point; v1 sources and evidence remain preserved.

`pre_collection.py` is the nonexperimental inspection command. It retains all
original hashes, runtime dependencies and resource checks. Installed `g++` is
optional inventory before host freezing, while the original build provenance
and executable remain immutable. The captured compiler/runtime environment
must then match exactly across batches. No compilation is performed.

WSL may be inspected but cannot be frozen or used for collection by this revision.
The user must establish a reviewed physical-core mapping and stable reservation
on a suitable host. No `taskset`-only physical pinning claim is made.

The optional `--prepare-host-record` path requires the original reservation
arguments plus `--physical-placement-evidence`: public-safe evidence identifying
the Linux CPU, physical host/core/type, mapping method, sibling-load and power
controls. Native hosts must be reported as nonvirtualized; a dedicated VM requires
host-side binding and non-migration evidence. Preparing a record does not approve
it: review, publication and separate anchor authorization are still required.

`collect.py` is byte-identical to the v1 deployment collector; its local `host`
import selects the new guard. It is not invoked by inspection or validation.
Do not use an older collector to bypass v2 policy. The anchor and N=4/6 scopes
are reused unchanged. No collection is authorized by this checkpoint.

Bounded validation command (not the experimental preflight):

```bash
python3 experiments/a2_larger_deploy_v2/validate.py --output /new/path/validation.json
```

The published validation already ran. It uses metadata, hashes and synthetic
guard fixtures only, with the real worker constructor forbidden.
