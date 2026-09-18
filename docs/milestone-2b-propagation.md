# Milestone 2B, checkpoint 2: audited A1 scene propagation

Base: published `main` at `7256bfbbd1afa1afbf1062dd2ec91eefbc78bd06`.
This checkpoint joins the independently validated A1 ray relations through shared
scene domains. It implements generalized arc consistency (GAC), without adding
nested-envelope feasibility, simultaneous witnesses or global support queries.

## Representation and API

`A1Problem` owns the cell count, immutable ray constraints and cell-to-factor
incidence lists. It validates cell references, complete world inputs and domain
counts. Duplicate constraints remain present, including contradictory copies;
rays with identical cell sequences but different slab-hit flags remain distinct.

`gac(problem, domains, stats, observer)` copies the input domains and returns an
`A1PropagationResult`. Its `consistent` flag means that propagation reached a
nonempty GAC fixed point. This API does not certify global supported states or
construct a world. A false result proves contradiction. Its partial domains are
diagnostic and may depend on which contradiction was encountered first; only
successful fixed-point domains are required to be identical across factor orders.
An empty initial domain is a contradiction even if its cell occurs in no ray.

The preserved interrupted implementation supplied the scene container and shared
GAC loop. The scene now has its own `a1_problem` module, and A1 propagation has
its own `a1_gac` API rather than prematurely introducing a complete solver API.
A0 and A1 use the same private queue/kernel. Their public domain types remain
distinct. A0 retains its existing API and behavior. Domain-typed observer aliases
retain the existing A0 observer interface and add an A1 interface without copying
the audit protocol. No envelope or search code from the interrupted work is added.

## Fixed point and soundness

Each queue update computes exact supports for one ray under the current domains.
It intersects each participating cell with its supported mask and requeues all
incident factors whenever a domain shrinks. Enqueuing is deduplicated, but factors
are never discarded. Factors with empty scopes are still evaluated.

A globally feasible assignment must satisfy every individual factor, so removing
an unsupported factor literal preserves every globally feasible assignment.
Each revision contracts domains and is monotone with respect to domain inclusion.
There are finitely many literals; every affected factor is eventually revisited.
Consequently successful runs reach the greatest common GAC fixed point contained
in the initial domains, independent of fair processing order. Encountering an
empty domain or an unsupported factor proves contradiction, not a timeout.

The observer receives the original assumptions at the start of every invocation,
every actual reduction before mutation, and every reported contradiction. Its
methods return no domains or decisions. Exhaustive data exists only in tests.
Audited and unaudited executions must have identical results and update/deletion
counts. Final domains must also match the observer's complete reduction trace.

## Independent validation

The C++ test oracle enumerates worlds and evaluates scalar first-hit predicates
without using the production automaton or emission routine. A separate full-scan
closure projects enumerated factor assignments, without using the production work
queue. Every audited deletion is checked against both the original conditioned
global family and its current local factor justification. The latter check is
useful even when the global family is empty. Every contradiction is independently
checked. Fault injection verifies rejection of an unsound slab deletion and a
false contradiction, including after resetting the observer's assumptions.

* Three shared-ray scenes (cascade, cycle and slab occlusion), all `16^3` domain
  triples including empty masks, and all six factor permutations: 12,288 initial
  scene/domain cases. Successful domains and contradiction status must agree with
  the independent closure across every order. Re-running a fixed point must make
  zero deletions and leave every factor locally supported.
* The same 12,288 cases compared with frozen Python `gac`, through an export-only
  adapter. Every cell in this corpus is referenced: the Python routine does not
  separately reject an empty domain on an unused cell. C++ tests independently
  cover that additional scene-level check; Python remains unchanged.
* 256 seeded four-cell cases, mixing planted feasible cases with arbitrary labels
  and restrictions, tested in forward and reverse factor order.
* A `2 x 2 x 2` physical scene with six lower/upper, opposite and crossing rays.
  Enumerate all 65,536 worlds using independent AABB observations to audit the
  scene and its restricted-domain propagation, including reversed factor order.
* Empty scenes and rays, unused cells, conflicting and repeated observations,
  same-material ambiguity, slab-hit differences, occlusion, correlated domains,
  malformed inputs, and the cascade that requires revisiting an earlier factor.
* Complete Release and ASan/UBSan suites, preserving all A0, A1 geometry and
  single-ray acceptance gates.

Both complete gates passed: **17/17 Release tests** (15.20 seconds) and
**17/17 ASan/UBSan tests** (180.14 seconds), with leak detection disabled in
this traced environment. Across 12,554 scene/domain cases, 90,322 observed calls
(including idempotence replays) audited 78,340 reductions, 108,702 deleted states
and 58,180 contradiction reports. No audit failures or reference mismatches
occurred. These counts exclude unobserved comparison runs and fault injections.

The shared fixture text starts with a magic string and scene count. Each scene
has a name, cell count and factor count; each factor gives its target, step count
and `(cell, slab-hit)` pairs. Both export and C++ tests consume the same file.
Validation outcomes and tested source hashes are recorded in
[`results/milestone-2b-propagation/`](../results/milestone-2b-propagation/).
Validation timings include oracle construction and auditing, not solver benchmarks.

## Next smallest checkpoint

Implement the complete nested-envelope feasibility kernel and validate one
simultaneous maximal-geometry witness under restricted domains. Exact global
supported-state queries and final A1 acceptance/benchmarks should follow in later
checkpoints. This checkpoint adds no A2 work and does not mark Milestone 2B complete.
