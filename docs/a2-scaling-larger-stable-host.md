# Portable stable-host preparation and physical anchors

**Pre-collection infrastructure only. No experimental solver calls, reference
jobs or timing epochs were run. No collection host has been selected in this
workspace.** This checkpoint follows
`57a1a1a8a057b19812f801ed55775da6ee405c2a` and preserves all earlier freezes,
preflight evidence and blocked-launch records.

The scientific v1 protocol and its 72,148-design master manifest are unchanged.
The [host-policy amendment](a2-scaling-larger-launch-amendment-1.md) remains in
force. This checkpoint supplies a portable bootstrap, a stronger host identity
guard and the exact anchor-only selection. It does **not** amend anchors-first
ordering, budgets, configurations, seeds, inputs, references, branching or timing.

## Required collection sequence

1. Reserve a stable Linux host, run the nonexperimental checks below, and
   publish/verify its collection-host record before any experimental worker call.
2. Separately authorize the physical-anchor stage on that host.
3. Publish and verify the complete anchor evidence, including all five epochs
   and reference obligations.
4. Separately authorize N=4/6 using the existing 16,272-ID selection and the
   **same** frozen environment and shared study output directory.
5. Publish each batch; later volumes require that environment and the existing
   prerequisites, integrity rules and study-wide pause thresholds.

The original N=8 compatibility anchors are not reassigned to N=4/6. The new
anchor-only manifest resolves the scope issue by making the required first stage
separately launchable. It does not authorize executing that stage now.

## Frozen physical-anchor selection

The unchanged generic selector read the master manifest and selected its
`["anchor",8]` job. The manifest declares **four** designs and **128 calls per
epoch**, hence **640 planned timed calls in five jobs**. These are plan counts,
not executed measurements. Untimed root diagnostics and references retain the
original collector's separate accounting.

| Original case ID | Original record ID |
| --- | --- |
| `[["anchor","path"],"split_material"]` | `85b73bde2f1c4015b26387dc37d631b7130427b7c525665e6e6b388937fafc8b` |
| `[["anchor","path"],"same_material"]` | `dbb720b48a15476537e283ac883926fd20e3bab33fe11861fac03a6b6a18578a` |
| `[["anchor","triangle"],"split_material"]` | `8fb51d8a41fec968043e9316331d8ef596771f5148fa18e88253ec978f24e435` |
| `[["anchor","triangle"],"same_material"]` | `2454aa74e008084725cc25f2e3af145495477d98f9c709a6416974854c6431cf` |

All four have declared N=8. There are **zero non-anchor designs**, including
zero N=8 camera or abstract structured cases. The
[selection](../results/a2-scaling-larger-stable-host/anchors/selection.json),
ID index and dry-run inventory reference the original
`results/a2-scaling-larger-v1/inputs/cases/anchors.jsonl.gz`. Its SHA-256 is
`70c87ff4884932fa961ffd8cd2aeb4b9da0eae39887d0b57d60bc774a906f68e`.
The master manifest SHA-256 remains
`1cf1881337a649de17ab244fb70dd8f5fc421467e1e8a4efb433412d30e0378b`.
No input was regenerated or edited; the index records original archive record ordinals
and row hashes. Both palettes, all four configurations, original queries,
hashed ordering and all five epochs are retained.

## Compatible stable environment

Use a reserved physical Linux machine or a dedicated persistent VM whose backing
hardware will not migrate during the study. Maintain the host, kernel, software,
CPU topology/affinity, cgroup environment and runtime libraries across batches.
A persistent service/session helps keep those settings stable. Keep the checkout,
Python environment and one shared study-output directory at the same paths.
Do not use these ChatGPT workspaces as an assumed persistent timing host. Their
recorded node identities have changed; another node, `9eb9e7a1d275`, is observed
during this checkpoint. No reservation has been established for it.

The frozen executable is **ELF64 Linux x86-64**, declaring x86-64-baseline ISA.
Static metadata requires GLIBC symbols through **2.38**, GLIBCXX through
**3.4.32** and CXXABI through **1.3.9**. Direct dependencies are `libstdc++.so.6`,
`libgcc_s.so.1` and `libc.so.6`; the observed dependency chain also includes
`libm.so.6` and the `/lib64/ld-linux-x86-64.so.2` loader. All resolve on the
inspected Ubuntu 24.04 environment. No solver invocation was needed to inspect
these requirements. The binary appears portable to a compatible glibc Linux
x86-64 host; ARM, musl-only systems and older incompatible runtime libraries
will fail the checks. Portability to a particular new host is conditional on
its pre-collection check passing; this is not a solver correctness or timing run.

Provide these tools/dependencies before running the check:

- Git, GNU `readelf` (binutils), `ldd`, procfs, sysfs and unified cgroup v2.
- CPython **3.12** and NumPy **2.3.5**. A dedicated virtual environment may be
  used; activate the same interpreter/environment for every later batch.
- The original GCC driver accessible as `g++`: version
  `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`, SHA-256
  `52f1ddb33fe78b9441e0f42e9cd22c571f1101938e046c8a26582494e041cc73`.
  This retains the existing compiler-provenance check. The bootstrap does not
  invoke a compilation, install/downgrade packages or silently accept another
  compiler. Obtain the matching official package if needed; version text alone
  is not sufficient. Package availability on the eventual host has not been
  established here.
- Compatible C++/glibc runtime libraries; the script resolves the actual files,
  verifies required symbol versions and records hashes. Python and NumPy's
  shared-library dependencies are also recorded.
- At least **20 GiB free disk** at the durable study-output location and at
  least **4 GiB effective available RAM** after visible cgroup limits. These
  checks preserve the declared reserve/worker ceiling; they do not estimate
  the study's runtime or eliminate its existing memory/pause safeguards.

The published solver is restored from `worker-linux-x86_64.gz` to
`build/a2-larger/worker` only if absent. Both gzip and uncompressed hashes are
checked first; an existing mismatching binary is preserved and blocks progress.
No rebuild is needed or performed. The solver SHA-256 remains
`882d5025cc9f1334e78a5c39cfdd46b80aa255fa97704d4e71abbd53234daf1f`.

## One nonexperimental pre-collection command

Clone the repository, fetch and check out the **published deployment checkpoint
reported with this document**, and activate the intended Python environment.
Set `CHECKPOINT` to that full commit hash, `CPU` to an allowed logical CPU and
`STUDY_OUTPUT` to the durable directory that all batches will share. Supply a
public-safe stable asset identifier and reservation reference/attestation. For
a VM, use `dedicated_non_migrating_vm` instead of `reserved_physical_host`.

From the checkout root, run:

```bash
python experiments/a2_larger_deploy/pre_collection.py \
  --checkpoint "$CHECKPOINT" \
  --cpu "$CPU" \
  --study-output "$STUDY_OUTPUT" \
  --report results/a2-scaling-larger-stable-host/actual-host-check.json \
  --prepare-host-record results/a2-scaling-larger-stable-host/collection-host.json \
  --stable-kind reserved_physical_host \
  --asset-id "$ASSET_ID" \
  --reservation-evidence "$RESERVATION_EVIDENCE"
```

**This command never invokes the experimental worker, a solver API or an oracle.**
It exits after metadata, hashes and synthetic OS-capability checks. It creates
the study directory if necessary and restores the exact packaged binary when
absent. It does not install software or download/regenerate scientific inputs.
For inspection without preparing a freeze, omit `--prepare-host-record` and
the three reservation arguments. Existing reports and host records are never
overwritten; review a failure before choosing a new report path.

The command verifies:

1. Exact requested Git HEAD, ancestry from the prior checkpoint, tracked-file
   cleanliness, committed deployment/collector/selection/specification files
   and accepted production/reference subtree identities.
2. Frozen binary, build/compiler, original collector/reference, scientific
   protocol, amendment and master-manifest hashes; all 40 original archive
   size/hash/decompression records and exact selected-ID resolution.
3. Linux/x86-64, ELF loader and required runtime symbol versions; actual shared
   library hashes, interpreter and NumPy identities.
4. Hostname, hashed persistent machine identifier (and DMI UUID when readable),
   CPU model/features/microcode, logical/physical topology where exposed,
   online CPUs, visible governor settings, affinity set and pinned CPU.
5. Total and effective available RAM, free disk at the shared study path and
   visible cgroup limits, including ancestor memory constraints.
6. CPU pinning in a separate Python-only child, enforcement of a **4 GiB
   RLIMIT_AS**, rejection of an oversized *virtual* mapping without touching
   large memory, and termination/reaping of a short-lived waiting child.
   Its synthetic timeout is 0.2 seconds; the real **65-second watchdog is
   unchanged**. The existing preflight remains the evidence for the actual
   worker's budget/watchdog behavior and is not rerun.

## Publishing and enforcing the host freeze

On PASS, the optional machine-readable `collection-host.json` records the
prospective environment, checkpoint, timestamp, reservation identity, selected
CPU, topology, kernel/OS, library/compiler/binary/source hashes, protocol and
manifest identities, resource snapshot and capability evidence. Raw machine
identifiers are hashed. Use a public-safe reservation description, never account
credentials. Metadata cannot independently prove that a provider will never
migrate a VM; the stable reservation is an operator/provider obligation.

The record uses `status: frozen_for_collection` to express the intended fixed
identity, and explicitly requires publication before execution. Creating it
does **not** authorize collection. Publish the record and check report to main,
verify their hashes, then obtain the separate anchor-stage authorization. Do not
edit the record to make a later host mismatch pass. If checks fail, stop; do not
replace the compiler/binary or relax a resource threshold implicitly.

Free RAM, free disk and timestamps belong to the recorded resource snapshot;
their expected changes are not treated as host drift. Their thresholds are
rechecked. Static machine/topology/kernel/library/source identities and cgroup
limits are compared exactly. A changed kernel, library, CPU model, machine
identifier, affinity environment, compiler, source or binary blocks execution.
This is not a same-CPU-family check and does not permit retrospective timing
normalization. Boot/reprovision/session changes may require a review if they
change the frozen environment.

Future authorized execution must use
`experiments/a2_larger_deploy/collect.py`, with the published host record,
published selection, selected CPU and authorization checkpoint. It retains the
previous scoped collector's job/timing/reference body and calls the stronger
host guard before collection and before each fresh worker. It also enforces
the single frozen output directory. Its source hash joins the resume identity.
The original collectors and launch guards remain unchanged as preserved
versions. Advancing Git to publish raw evidence is allowed; frozen scientific
and deployment file identities must remain unchanged. A future authorization
checkpoint must be checked out exactly.

The N=4/6 selection remains valid without regeneration. The shared study ledger,
reference cache and sealed job archives preserve accumulated costs and anchor
prerequisites across batches. Later collection cannot silently run missing
out-of-scope stages. Publish/verify anchor evidence before authorizing N=4/6.

## Evidence and limitations of this checkpoint

The [anchor/deployment validation](../results/a2-scaling-larger-stable-host/anchor-deployment-validation.json)
records **43 passing nonexperimental checks**: exact four-ID membership, no
non-anchors, original hashes, unchanged five-epoch ordering, dispatch and resume
boundaries, restoration without overwriting, corruption/identity rejection,
RAM/disk stops and rejection of changed machine/topology/library/compiler/source
identities. Synthetic archive records were temporary, labeled infrastructure
only and never published as experimental observations. The no-op transport
received zero calls.

The [host-check evidence](../results/a2-scaling-larger-stable-host/workspace-host-check.json)
exercises the single bootstrap command on this workspace **without preparing a
collection-host record**. Its PASS establishes local dependency/capability checks,
not a stable reservation or clearance to collect here. The collector's stage-loop
AST matches the preserved scoped collector: solver APIs, references, budget
handling, timing order and engineering stop logic are unchanged.

No new C++ binary, input generator output, correctness gate, benchmark, scientific
analysis, bounded preflight rerun or experimental timing data was produced.
Before anchors can be authorized, the user must establish the actual stable
host, obtain a passing check there, and publish/verify its identity record.
