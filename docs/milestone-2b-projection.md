# Milestone 2B, checkpoint 5: exact A1 supported-domain projection

Base: published `main` at `914730f13470df0fc3f51f777014867a2285b64e`.
This checkpoint computes exact per-cell support sets and a collection of valid
simultaneous scenes covering every supported state. It is not final Milestone 2
acceptance or performance benchmarking.

## API and behavior

```cpp
#include "mcr/inference/a1_supports.hpp"

auto result = mcr::exact_supports(problem, domains, stats, observer);
// result.feasible: whether the original restricted scene has a solution
// result.supported[v]: exactly the states appearing at v in some solution
// result.witnesses: valid scenes collectively covering every supported literal
```

The A1 overload returns `A1SupportResult`. There is one supported mask per input
cell, including on infeasibility, when all masks are empty and there are no
witnesses. A feasible zero-cell problem returns empty masks and one empty world;
its explicit feasibility flag distinguishes it from a contradictory zero-cell
problem. Wrong domain counts throw before any observer event or counter update.
Inputs are copied, and every support mask respects the original restrictions.

The masks are exact marginals, not a Cartesian description of feasible scenes.
Multiple supported values preserve genuine ambiguity. Combining individually
supported choices can violate a shared ray. Returned witnesses are actual
simultaneous solutions, not arbitrary combinations of those masks. The witness
list is a covering collection, not a minimum cover or an enumeration of solutions.

## Construction and completeness

1. Run the existing complete `envelope_feasible` kernel once on the input domains.
   On failure, return infeasible with empty support masks for all cells.
2. On success, certify the root witness: each state in that scene is supported.
3. Visit each remaining root-domain literal not already certified by a witness.
   Call the existing `query_support` under the unconditioned root domains. On
   success, retain its witness and certify every literal in that scene. On failure,
   the requested literal has no feasible conditioned world.
4. Return the certified masks and witnesses. Report final global exclusions only
   after restoring the observer's unconditioned root-domain context.

This adapts the witness reuse in the preserved earlier research work while using
the validated public query primitive. No second support decision procedure or
mandatory GAC pass is introduced. The complete envelope kernel already provides
the necessary root feasibility result and sound domain bounds.

Sound root pruning preserves the entire feasible family. Consequently, support
under the reduced root domains equals support under the original restrictions.
Every retained state occurs in an actual witness. Every other state was either
soundly pruned at the root or rejected by a complete conditioned query. Those two
directions prove exact support projection. A witness can certify several literals
without querying them separately; its existence is the required certificate.

The proof inherits the fixed A1 nested geometry, opaque first-hit material
observations, fixed cell order and per-cell restrictions of the previous kernels.
The implementation does not extend that result to incomparable shapes or extra
relations between cells.

## Audit and work accounting

Root propagation and each conditioned query retain their existing
`nested_envelope` audit contexts. Query reductions are not applied to other
queries or treated as unconditional deletions. The final `support_projection`
context restores the root domains, then emits `unsupported_state` events for
the globally excluded values. The new reason is appended without changing prior
enum values. Infeasible roots report their existing contradiction; their empty
output marginals require no further queries or per-cell pruning events.

The test auditor independently checks the conditioned kernel events and the
final global exclusions against exhaustive families. It also verifies exact
singleton assumptions, restoration of the root context, and input immutability.
Fault controls reject a leaked query context and deletion of a supported slab.

If the feasible root has `K` candidates across `n` cells, its first witness covers
`n` literals. There are at most `K-n` single-state queries and exactly one more
envelope call than query calls. With `m` rays and `L` total ray steps, this gives
`O((1 + K-n)(n + m + L))` work under the prior kernel bound, and at most
`1 + K-n` stored witnesses. Witness output uses `O(n * witness_count)` space.
Tests check these call bounds and zero search branches/nodes. Timings below are
validation times, not performance measurements or scaling claims.

## Correctness evidence

* **13,760 audited projections:** 3,113 feasible and 10,647 infeasible, with exact
  agreement against independent exhaustive families.
* **8,602 independently verified returned witnesses** cover every one of the
  **17,498 reported supported literals**. The union of witness states equals the
  reported support masks exactly. Every physical witness is also independently
  re-rendered by the ray/AABB renderer.
* **194,192 public `query_support` comparisons** on the original input domains,
  including forbidden input states and values pruned at the root, agree exactly
  with the projection. Their successful witnesses also belong to the reference
  family.
* The native corpus includes all `16^3` domain triples for three shared-ray
  fixtures (12,288 projections), including empty masks; and all 65,536 physical
  worlds grouped by six rays into all 729 possible observation vectors, under
  full and deterministic restricted domains (1,458 projections).
* Fourteen named projections cover zero-cell scenes, unused cells, empty domains,
  restricted ambiguity, hidden states, a determined cube/slab scene, reversed ray
  order, contradictory observations and an unsupported air value surviving the
  envelope pass. Separate checks cover idempotence, malformed inputs and audits.
* **10,125 frozen-reference projections:** 2,624 feasible and 7,501 infeasible,
  with 9,856 supported literals. The test adapter aggregates the existing 64,800
  frozen-Python single-query decisions and additionally compares the three
  unrestricted scenes with the original `nested_supports` routine. It contains
  no new Python solver. All frozen reference files remain unchanged.
* **31,575 kernel pruning events** (40,524 deletions) and **4,290 final projection
  events** (5,270 deletions) passed independent auditing. The 10,759 internal
  queries include 5,270 unsupported results. There were 24,519 envelope calls,
  **zero search branches and zero search nodes**. Observed and plain runs agree
  in outputs and measured work.

The full 26-test Release suite was exercised. Twenty-three checks passed in its
initial run; an empty generated feasibility executable prevented two checks from
starting, and a truncated generated A0 ray file failed renderer agreement.
Relinking only that executable and regenerating the unchanged A0 reference
fixture made all affected checks pass. Source code did not change during repair.

Ten focused ASan/UBSan checks passed across projection, Python agreement, A1
query/feasibility/propagation/geometry, A0 inference and adversarial regressions.
The generated adversarial executable was also found empty, then relinked and its
single test rerun successfully. Leak detection was disabled in the traced runtime.
The unchanged full renderer sanitizer suite was not rerun in this checkpoint.

Detailed logs, counts, reference hashes and tested-source hashes are saved under
`results/milestone-2b-projection/`. Counts exclude plain duplicate runs, Python
comparisons and standalone audit/idempotence probes unless explicitly identified.

## Final milestone work still required

The next checkpoint should consolidate final A1 acceptance: exercise the complete
predefined camera/view and adversarial suites on all 65,536 worlds; verify exact
supports, restricted-domain contradictions, witness coverage and pruning soundness;
run the full sanitizer regression gate; collect the planned time, memory and
view-identifiability benchmarks; and only then write final acceptance and create
`cpp-milestone-2`. This checkpoint performs none of that final benchmarking or
tagging, and adds no A2, camera recovery or wider block vocabulary.
