# Milestone 2B, checkpoint 4: one exact A1 support query

Base: published `main` at `4f005bfa46d615d8415db6ba6637001dbbc177ea`.
This checkpoint decides support for one requested cell/state under the caller's
input domains. It adds no whole-scene support projection or search.

## API and contract

```cpp
#include "mcr/inference/a1_support_query.hpp"

auto result = mcr::query_support(problem, domains, cell, state, stats, observer);
// result.supported: exists a scene satisfying the observations, domains and X_cell=state
// result.witness: that simultaneous scene, present exactly when supported
```

`A1SupportQueryResult` contains only the support decision and an optional witness.
It exposes no domains that could be mistaken for a complete support projection.
Inputs are copied. Conditioning intersects the requested singleton with the
existing domain. An excluded state therefore produces an empty conditioned
domain and an unsupported result; the call never reintroduces a forbidden state.
Any other empty input domain also yields unsupported, including unused cells.

Wrong domain counts or invalid state codes throw `std::invalid_argument`; an
out-of-range cell throws `std::out_of_range`. These malformed requests leave
counters and observers untouched. A zero-cell problem has no valid support query.
An unsupported query does not imply the unconditioned problem is infeasible.

Each valid call increments `literal_queries` once and calls `envelope_feasible`
exactly once, including excluded candidates. A failed decision increments
`infeasible_queries`. Existing counters accumulate; they are not reset. No root
GAC pass, extra feasibility call, loop over requested literals, or search is hidden
in this API.

## Exactness and audit context

Let `D'[cell] = D[cell] intersect {state}` and keep every other domain unchanged.
The feasible worlds under `D'` are exactly the feasible worlds under `D` whose
chosen cell has that state. The already-proven complete A1 envelope kernel thus
decides support, under the same nested-shape, opaque first-hit assumptions.

The kernel constructs and checks its simultaneous witness against the conditioned
domains and observations. The query wrapper additionally checks the requested
literal before returning. Independent tests filter exhaustive feasible families
using the original domains and literal, and require each witness to belong to the
resulting family. Physical witnesses are also re-rendered through independent
ray/AABB intersections.

The optional read-only observer is passed directly to feasibility. Its `begin`
event supplies the full **conditioned** domains in a fresh `nested_envelope`
context. Conditioning itself is an assumption, not a globally justified root
pruning event. Subsequent deletions and contradictions are audited relative to
that conditioned family. Tests explicitly check this boundary and reuse one
observer across alternating successful and unsuccessful queries.

The feasibility kernel is unchanged. A query adds one domain copy and a singleton
intersection to its `O(n + m + L)` work bound. Tests assert one kernel call, zero
branches/nodes, and the existing front-advance and factor-update bounds per query.
Survival in reduced domains alone is never used as evidence of support: a named
one-cell oak case leaves air after envelope propagation but rejects its air query.

## Validation

* **194,134 audited queries:** 17,482 supported with independently verified
  witnesses, 176,652 unsupported. Of these, 84,422 request a state excluded by
  the input; the other negative cases include conditioned contradictions.
* **256,074 audited reductions / 368,030 state deletions:** every reduction
  preserves the conditioned exhaustive family; every negative result has an
  audited contradiction. All observed and unobserved calls agree exactly.
* Three shared-ray fixtures, all `16^3` input-domain triples including empty
  masks, every cell and every requested state: 147,456 queries.
* All 65,536 physical worlds partitioned by six rays into 729 possible observation
  vectors; both unrestricted and deterministic restricted domains; every requested
  literal: 46,656 queries. Every returned physical witness is independently rendered.
* Twenty-two named audited queries cover occlusion, cube/slab separation,
  unsupported survivors, forbidden candidates, unused cells and empty domains.
  Separate checks cover invalid arguments, accumulating counters and audit reuse.
* **64,800 frozen-Python comparisons:** 9,856 supported and 54,944 unsupported;
  exact decisions and deterministic witnesses agree. The adapter calls existing
  `envelope_feasible` under singleton assumptions. Python's nonempty-domain
  precondition is respected; excluded states and empty masks are covered above.
  All eleven frozen reference files remain byte-for-byte unchanged.
* **Zero search branches and nodes** on every query. The production operation
  performs exactly one complete feasibility call, not a support projection.

The complete 23-test Release suite was exercised. Its initial run passed 22 tests;
the A0 renderer agreement found an empty generated ray file and truncated image
file. Regenerating the unchanged Python fixture and rerunning that agreement
passed both tests. All 23 Release tests therefore passed with the same source.

Eight focused ASan/UBSan tests cover the new operation, Python agreement, A1
feasibility/propagation/geometry, A0 inference and adversarial regressions. Seven
passed initially; the old feasibility executable could not start and was found
to lack an ELF header and executable permission. Relinking only that generated
binary and rerunning its test passed. All eight checks passed; no source changes
were needed for either artifact repair. Leak detection was disabled in the traced
execution environment. The unchanged full renderer sanitizer suite was not rerun.
The Release support-query fixture was later found to be a truncated prefix of
the complete sanitizer copy. It was restored from that copy, its recorded SHA-256
was verified, and the Release Python-agreement executable passed again.

The original and targeted-recheck logs, query counts, reference metadata and
tested-source hashes are recorded in `results/milestone-2b-support-query/`.
Validation times are not solver benchmarks. This is checkpoint acceptance only.

## Remaining scope

No whole-scene support masks, batch query API, final A1 benchmarks/acceptance/tag,
or A2 implementation is included. The next small checkpoint is exact whole-scene
supported-domain projection built on this operation, audited against exhaustive
families; keep final acceptance and benchmarking separate.
