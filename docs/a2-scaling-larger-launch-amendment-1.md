# Larger-volume A2: pre-collection launch amendment 1

**Infrastructure revision only. Collection is not cleared. No experimental
cases, solver calls, timing epochs or reference jobs were executed.** This
checkpoint follows the blocked launch at
`059699cf442676af77f920defd5000096f4b2049`. It preserves the original protocol,
collector, reference adapter, worker, inputs, preflight evidence and blocked
launch record. No scientific results exist for the new larger-volume study.

The [machine-readable amendment](../experiments/protocols/a2-scaling-larger-launch-amendment-1.json)
is prospective. The scientific population remains v1. It clarifies collection
host selection and adds a separately versioned, guarded subset launcher; it
does not overwrite the original freeze or change scientific case definitions.

## Timing host: one collection environment, distinct from preflight

The preregistration requires freezing the compiler, binary and host **before
timing**, with one recorded host and pinned CPU across volumes and modes.
Its scientific purpose is comparable measurements in the same environment.
It does not require that the separately bounded infrastructure preflight supply
that environment. The original collector records the actual collection host
and checks it on resume; the subsequent launch instruction additionally required
matching the preflight host. This amendment explicitly replaces that additional
requirement, rather than silently bypassing a failed check.

The policy now is:

1. Preflight may use a separate recorded host. Its timings never enter the
   scientific timing population.
2. Before any experimental worker invocation, including untimed root GAC,
   select and publish one stable collection environment and its reservation
   identity. Retain the original solver binary and recorded build/compiler.
   Freeze the host/CPU, selected CPU and affinity capability, kernel/OS,
   compiler and binary hashes, Python environment, resource limits,
   timing-relevant environment variables and revised launcher hashes.
3. Use that same environment for all volumes, configurations, batches and
   five epochs. Recheck it at launch and before each new worker. A change
   pauses collection before further worker calls. No heterogeneous-host
   timing pool or subsequent normalization is permitted.
4. Any later restart or timing-cohort decision requires a separate prospective
   checkpoint. This amendment does not authorize a collection.

No collection host is selected here. The recorded preflight node was
`ad6b63c043ba` (AMD EPYC 9V74). The blocked launch observed `8984c830f591`
(Intel Xeon Platinum 8573C). This review observes `44f7d8c0b704` (AMD EPYC
9V74). Returning to the same CPU model does not establish the same physical
machine. The [environment status record](../results/a2-scaling-larger-launch-amendment-1/collection-environment-status.json)
is explicitly **unselected**, not a new host freeze.

The available workspace does not establish a reservation or a means of restoring
the same host across future sessions. Stable timing across batches cannot be
certified here. Use a reserved physical host, or a dedicated persistent VM with
unchanged backing hardware and no migration, with durable storage and the
frozen build/runtime environment. Record its stable asset/reservation identity
as well as the runtime host fingerprint. A container name alone is insufficient.
Then publish that environment before the first experimental invocation. The
existing 20 GiB reserve, 4 GiB worker limit and all other resource checks remain.

## Exact batch membership and the anchors-first conflict

The new [selector](../experiments/a2_larger_launch/selection.py) consumes the
published 72,148-ID manifest and verifies its archives. It selects whole jobs
by declared `n` or an explicit complete protocol stage. It never renders,
generates, replaces or edits a scientific input. Its index contains the original
record/case ID, archive path, zero-based data-record ordinal and SHA-256 of the
original record bytes. Index order is archive/record order; timing order remains
the original hashed order.

| Selected volume | Camera | Restricted | Contradictory | Structured | Total | Jobs per epoch | Planned timed calls, five epochs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| N=4 | 3,744 | 72 | 72 | 0 | 3,888 | 12 | 181,440 |
| N=6 | 12,000 | 120 | 120 | 144 | 12,384 | 27 | 555,840 |
| Combined | 15,744 | 192 | 192 | 144 | **16,272** | **39** | **737,280** |

These are design/dispatch inventory counts, not measured outcomes. The selected
ID index resolves to eleven original archives (six case archives, including
the shared structured archive, and five rig archives). Reading a shared archive
does not assign its other volumes to this batch. All reference, query, palette,
configuration and epoch obligations for the selected jobs remain intact.

Structured cases belong to their explicitly declared N. There are no N=4
structured slots; all 144 N=6 structured slots are included. The four physical
anchors have explicit N=8 membership and job ID `["anchor",8]`, despite their
palette-independent slot IDs omitting N. They remain a separate anchors stage.
Selecting volume 8 includes that stage and ordinary N=8 jobs; selecting just
the anchors stage is also possible without reassigning its cases.

**An additional schedule conflict is now documented precisely.** The
authoritative v1 JSON says:

> Ascending N; legacy physical anchors first, then declared camera/structured
> slots at each N. Complete all five epochs of a volume before the next, unless
> a predeclared stop pauses the study.

Thus anchors-first was preregistered, not merely an accidental CLI behavior.
An N=4/6-only *first* batch cannot both obey that rule and exclude all N=8
execution when no anchors have run. This checkpoint preserves the rule and
does not defer, relabel, omit or execute the anchors. Missing prerequisite
stages cause the scoped launcher to stop before creating a worker. It will
never execute an out-of-batch prerequisite automatically.

On a stable frozen environment, the minimum decision is either to separately
authorize the original four-anchor stage first, then the N=4/6 batch, or to
publish an explicit prospective stage-order amendment before any timing. A
possible amendment would move the unchanged anchor stage immediately before
ordinary N=8 collection; that is a proposal, **not the policy adopted here**.
The current instruction to retain timing/order rules prevents silently doing so.

## Versioned launch path and resume

The new [scoped collector](../experiments/a2_larger_launch/collect.py) retains
the original collection body and imports the unchanged worker transport,
call-order functions, references, agreement checks, engineering screens and
atomic archive storage. Its changes are limited to explicit selection and
environment bindings, prerequisite verification, per-dispatch scope checks and
batch completion accounting. There is no new C++ binary. The original binary
SHA-256 remains
`882d5025cc9f1334e78a5c39cfdd46b80aa255fa97704d4e71abbd53234daf1f`.

Whole-job selection preserves the case-order index used for palette/mode/API
rotation. The five epochs, fresh processes, shuffled order, CPU pinning,
serial-worker lock, zero warmups, references outside API timing, budgets and
pause criteria are unchanged. The additional checks occur outside API timing.
Every G/F/P/Q dispatch checks the frozen case ID, volume, job and input digest;
for this selection an N>=8 case cannot reach the transport.

All batches must use **one shared study output directory**, environment identity
and administrative cost ledger. This preserves the study-wide 48 solver-hour
and 12 preparation/reference-hour pause thresholds. Earlier prerequisite jobs
are read only after archive/context/ordered-call-ID verification, never implicitly
run. The agreement index and operating screens are rebuilt from sealed evidence
as needed so later cross-volume screens retain the earlier data. Selected
resumed records must remain within the selection. Corrupt or interrupted output
blocks automatic execution; sealed orphan recovery does not rerun work.

Before any future launch, the environment record, selected-ID index, selection
descriptor, amendment and revised Python sources must match the published
authorization checkpoint. The original protocol, collector/reference sources,
master manifest and binary must still match their original hashes. This version
does not accept the current `unselected` environment status as launch authority.
The legacy unscoped CLI is preserved as historical evidence and is not the
partial-batch launch path.

## Nonexperimental validation and preserved evidence

The [validation record](../results/a2-scaling-larger-launch-amendment-1/n04-n06/validation.json)
contains 37 passing checks. The [dry-run inventory](../results/a2-scaling-larger-launch-amendment-1/n04-n06/dry-run.json)
contains hashes of the ordered call IDs for 195 planned jobs. These are plans;
zero timing epochs or API calls were executed.

Validation checked the exact set predicate, both per-volume counts, whole-job
and palette preservation, the original epoch/job ordering, all query counts,
archive/row resolution, explicit N=8 anchor ownership, rejection of foreign
volumes and modified inputs, resume scope, sealed/orphan recovery without reruns,
corruption/partial-file rejection and the missing-host/missing-anchor barriers.
Temporary synthetic archive records used `infrastructure-*` identities outside
the experimental manifest and contained no timing measurements. A no-op transport
received zero calls. The real worker constructor was disabled during validation.

All 40 frozen input archives passed their existing size/hash/decompression checks;
their compressed hashes and the master-manifest hash remained unchanged afterward.
The accepted source subtrees, frozen reference, original collector, solver binary,
scientific protocol and preflight archives are preserved. The original failed
JSON-wrapper preflight and corrected successful preflight were not rerun.
Only this new nonexperimental validation was run; an initial passing validation
was repeated after adding an explicit original-compiler guard, before freezing
these new artifacts. No performance observations informed case selection.

The [checkpoint integrity manifest](../results/a2-scaling-larger-launch-amendment-1/checkpoint-integrity.json)
freezes the revised Python sources, amendment, selected-ID archive, dry run and
validation evidence. **Collection remains blocked until a stable environment is
reserved/frozen and the first-batch/anchor-order decision is resolved.** No
scientific inputs, solver behavior, weighting or timing results changed.
