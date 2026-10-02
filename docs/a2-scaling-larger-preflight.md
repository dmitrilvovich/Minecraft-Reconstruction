# Larger-volume A2 collector and preflight

**Infrastructure complete; scientific collection has not started.** This checkpoint
implements the [preregistered protocol](a2-scaling-larger-protocol.md) at
`8abc7601cb26cf0b851a933e50682d87a266061f`. The authoritative protocol JSON is
unchanged. No accepted solver, renderer, test or frozen Python source was changed.
No timing epoch, main-dataset solver call, 27-cell case or camera recovery ran.

The [input manifest](../results/a2-scaling-larger-v1/inputs/input-manifest.json)
freezes **72,148 design slots**: 69,984 clean camera slots (including 3,168
higher-resolution controls), 792 restricted cases, 792 contradictory cases,
576 abstract structured cases and four physical anchors. They retain the
declared repeated-input memberships; unique design IDs do not imply unique
physical inputs. The future workload remains 3,240,640 timed API calls across
five epochs. The reconstructed historical eight-cell corpus remains unchanged.

## Implementation

The new [experiment package](../experiments/a2_larger/) is separate from production:

- `generate.py` implements the frozen SHA-256 rankings, exact occupancy schedule,
  nested masks, palette pairing, resolution membership merging, structured
  permutations, factor ordering, query selection and audit selection.
- `worker.cpp` links the accepted C++ sources unchanged. Variable-length arrays
  carry states, domains and constraints; there is no eight-cell bit packing.
  Its `rig` and `anchor` commands only prepare inputs. Both existing C++ rendering
  paths are checked on every generating truth, every raw rig ray and both palettes.
- `reference.py` uses the unchanged frozen Python primitive-AABB intersection
  on serialized rational rays, independently of C++ traversal and its automaton.
  Structured references use parity/component orientations and fixed-region
  counting, cross-checked by scalar finite-world evaluation on the declared inputs.
- `run.py` provides serial, pinned, fresh job processes; deterministic epoch,
  case, palette, mode and API ordering; independent reference/witness checks;
  integer-work/configuration agreement checks; and preregistered pause screens.
- `common.py` implements provenance binding, deterministic closed archives,
  decompression verification, atomic publication and hash-verified resume.
- `preflight-plan.json` and `preflight.py` keep infrastructure checks separate
  from scientific collection and cap the preflight at 400 worker requests.

The manifest includes source hashes, the binary hash, complete case records and
19 shared rational-ray dictionaries. Its 21 case archives contain explicit
domains, truth IDs/states, labels, ordered factor references, query literals,
audit requirements and cohort memberships. Pixel-to-factor mappings reconstruct
every original observation. Shared rigs include all eight available cameras;
each case's camera indices select exactly its declared suite. Unused dictionary
rays do not create extra experimental cases. Structured cases preserve their
distinction from physical camera scenes.

## Reference coverage and measurement boundaries

Every N≤8 input has complete domain-world enumeration support. The N=10/12
selection contains exactly 72 clean camera inputs (12 palette pairs per geometry)
and six restricted inputs (one pair per geometry). The complete design schedules
35,254 mandatory exhaustive inputs, including structured cross-checks and anchors.
All remaining structured and contradictory controls have independent proofs.
Other camera inputs retain explicitly partial external coverage: validated
witnesses prove positive claims, not omitted supports or arbitrary UNSAT claims.
Cross-configuration agreement is an additional check, not an external oracle.

Independent reference geometry is cached only after hashing its source rays and
reference implementation. Every returned witness is checked against its original
domains and original observations; a repeated identical witness may reuse that
independent check. Projection witnesses must cover exactly the reported supports.
No reference data or propagated root domains enter a measured solver call.

The API timer excludes input parsing, rendering, oracle work, validation and
output. Observer-budget overhead remains inside it. Standalone feasibility and
full projection are distinct operations, with no invented internal phase timing.
Root diagnostics run separately. Worker peak RSS includes input/transport buffers;
it is not presented as solver-only memory.

The node limit remains 1,000,000 entries, with an aborting attempted entry
potentially recorded as 1,000,001. The 60-second deadline covers an entire API
call, including every internal projection query and the final deadline check.
A shared monotonic start timestamp lets the supervisor apply the 65-second
watchdog to the actual API interval. The worker has a kernel 4 GiB address-space
limit (`RLIMIT_AS`) plus RSS supervision. Allocation failure, node/time exhaustion
and known watchdog/memory termination are `UNRESOLVED`. Unexplained termination,
missing output or malformed records are explicit invalid/interrupted coverage,
with no inferred UNSAT result. Killed counters stay unknown.

No warmups are hidden. A project-wide inference lock prevents simultaneous
collector workers. Environment hashes bind resume to the recorded host and CPU.
Administrative costs are recorded separately and enforce the 48 solver-hour and
12 preparation/reference/audit-hour pauses; the 20 GiB reserve is checked before
collection. The numerical architecture screens remain the frozen protocol's
engineering policies, not conclusions reached during preflight.

## Bounded preflight evidence

The [preflight evidence](../results/a2-scaling-larger-preflight/README.md) covers:

| Check | Executed scope |
| --- | --- |
| Ordinary APIs | 40 fixed cases, both palettes, all four modes: 368 feasibility/projection/query calls |
| Camera coverage | All seven geometries; all five cell counts; declared full-k 10/12-cell audit paths |
| Structured coverage | All nine recipes at selected sizes, plus both physical path/triangle anchors |
| Root diagnostics | Two separate calls |
| Node/time exhaustion | Four test calls using zero limits; explicit `UNRESOLVED` |
| Watchdog / memory / missing output | Three non-solver fault requests; 0.2-second watchdog, reduced test allocation ceiling, simulated process exit |
| References | 40 independent checks; exhaustive small/intermediate audits and graph proofs; every returned witness checked |
| Storage | Valid-job resume without execution, sealed-orphan recovery, corrupt hash rejection, partial preservation, identity mismatch rejection, deterministic compression |
| Scheduling | Epoch permutations checked without executing an epoch; repeat-work mismatch rejection exercised |

The successful attempt used 377 worker requests: 374 solver/GAC or budget-test
invocations and three non-solver faults. An earlier attempt stopped on its first
worker response because the new transport's JSON string helper conflicted with
`std::quoted` overload resolution. Its source snapshot, failure and partial output
are preserved. Renaming that experiment-only helper fixed the defect. Including
that failed request, the checkpoint used **378 worker requests**, below the
400-request ceiling. No case was selected, removed or changed in response to
solver difficulty, and no preflight result belongs to the main dataset.

## Frozen build and cost feasibility

The intended binary has SHA-256:

`882d5025cc9f1334e78a5c39cfdd46b80aa255fa97704d4e71abbd53234daf1f`

It was built with GCC 13.3.0 (`Ubuntu 13.3.0-6ubuntu2~24.04`), C++20,
`-O3 -DNDEBUG`, without PGO, LTO, native-architecture tuning or fast-math.
The preflight host was x86-64 Linux 6.18.44, AMD EPYC 9V74, pinned to logical
CPU 0, with an 8 GiB enclosing cgroup. Full compiler, host, source and binary
identities are preserved with the [freeze evidence](../results/a2-scaling-larger-v1/README.md).

Input generation took about 19.5 seconds. The inputs occupy 6,660,175 compressed
bytes. Preflight reference geometry took about 20.3 seconds; exhaustive/structured
reference work took about 0.37 seconds. These are infrastructure costs, not solver
performance results.

The [cost worksheet](../results/a2-scaling-larger-v1/preparation-feasibility.json)
uses all declared audit inputs, domain-world counts, ray/primitive work, a
conservative returned-witness bound, observed non-API process intervals and
separately measured ledger writes. It estimates about **1.3 preparation hours**;
a fourfold-cost scenario is about **5.3 hours**, below the 12-hour pause. These are
planning estimates, not guarantees. The storage allowance is about **11.3 GiB**
before compression: 3 KiB per timed record plus 2 GiB for inputs, references,
indexes and logs, below the 20 GiB reserve. Only one job-sized temporary output
copy is required. Full-collection solver time is unmeasured: completion within
48 solver-hours is not promised.

## Durability and next boundary

Publication resumed from the persisted workspace. The source, binary, both
preflight evidence archives and all 40 frozen input archives survived and passed
size/hash/decompression checks. The 72,148 design IDs were checked for uniqueness
and category counts. No solver case or preflight was rerun during this recovery.
Eight redundant temporary files matched their completed archives or exact prefixes
and were quarantined; the [audit](../results/a2-scaling-larger-v1/temporary-file-audit.json)
records their hashes. No completed input or preflight evidence was regenerated.

Each job writes a temporary JSONL file, closes and fsyncs it, validates its record
count and ordered call-ID digest, hashes the uncompressed bytes, writes deterministic
gzip, verifies decompression and then atomically publishes the archive and completion
record. Resume verifies both hashes and the complete protocol/input/source/binary/
environment binding before skipping valid work. A sealed archive missing only its
completion marker can recover that marker without executing cases. An unfinished
partial is preserved and blocks automatic retries; recovery requires a reviewed
amendment, as the protocol requires. A derived SQLite comparison index is not the
sole copy of evidence; immutable job archives remain authoritative.

The collector is ready to attempt the declared collection at a separately authorized
checkpoint. No scientific protocol inconsistency was found. The host/binary/source
identity and storage reserve must still be checked at launch. Actual unresolved
rates, solver costs and architecture decisions remain for collection and review;
there is no authorization here to start them or to extend to 27 cells.
