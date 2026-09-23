# Milestone 2C: A1 correctness gate

This checkpoint accepts the complete A1 correctness pipeline at the declared
2 x 2 x 2, 8 x 8 pixel camera setup. It is **not the final Milestone 2 acceptance
record**. Performance/identifiability benchmarks remained pending at this checkpoint. The base is `61ac426b26b905feb507819b8673ae4f1dcae96a`.

No correctness bug was found. Production headers/sources, the frozen Python
reference, and the existing 26 tests are unchanged. The added test-only camera
corpus and acceptance fixture adapter reuse preserved earlier validation work,
adapted to the current public APIs. No obsolete solver implementation was reused.

## Coverage and independent checks

All 65,536 worlds over air, stone cube, oak cube and oak bottom slab are rendered
on all 512 exact camera rays. Frozen Python ray/AABB rendering and the two C++
rendering paths agree on 33,554,432 ray/world tuples. The existing gate also
checks 14,352,384 automaton decisions, 98,304 exhaustive single-ray domain/label
cases, 8,918 boundary rays, 153 named geometry samples, and A0 compatibility.
Slab-top tangency, half-open faces, interior origins, first-hit depth/order,
occlusion, and same-material full-cube/slab ambiguity are included.

The new `a1_acceptance` executable independently renders an AABB corpus, then
compares **every member** of every observation family with frozen Python. It
checks all eight existing view suites, without sampling their feasible families:

| View suite | Complete observation families |
| --- | ---: |
| axis_1 | 625 |
| axis_2 | 5,625 |
| axis_3 | 24,705 |
| axis_4 | 32,738 |
| axis_5 | 38,514 |
| axis_6 | 45,270 |
| oblique_1 | 11,353 |
| oblique_2 | 36,790 |
| Total | 195,620 |

For each family, the gate checks GAC against the frozen reference, standalone
complete envelope feasibility, exact projection, and a rotating direct support
query. Audited and unobserved projections must agree, including witnesses.
There are 524,288 world/family membership comparisons and 6,259,840 cell/state
occurrence-count comparisons. The latter distinguish every/some/no valid worlds.
All 195,620 complete-freedom counts match Python: replacing the cell by **any**
state in **every** feasible context must preserve feasibility. Full marginal
domains alone do not imply this stronger freedom property. Adding successive
axis views passes 2,621,440 marginal-containment checks.

An additional 512 reproducible restricted-domain/altered-observation inputs
(seed 20260917) include 421 infeasible inputs and conflicting duplicate rays.
Their entire conditioned families are independently enumerated. Every one of
the 32 cell/state requests is checked, including states excluded by the input.
The 39 named scenes run in all eight suites (312 scene/view pairs), with original
and duplicated/reversed constraints. Their GAC fixed points are order-independent
and idempotent; exact projections agree. These scenes include the frozen A1
adversarial catalog, all isolated slabs, slabs in front of material walls,
full roofs over slabs, and full/slab checkerboards.

The existing component gates remain essential independent coverage:

| Check | Native exhaustive/audited cases | Frozen Python comparisons |
| --- | ---: | ---: |
| Single-ray domains | 98,304 | 98,304 |
| Shared propagation | 90,322 runs | 12,288 |
| Complete feasibility | 75,747 calls; 16,584 witnesses | 10,125 decisions/witnesses |
| Requested support | 194,134 queries; 17,482 witnesses | 64,800 decisions/witnesses |
| Whole-scene projection | 13,760 projections; 8,602 witnesses; 194,192 direct-query checks | 10,125 projections |

The three abstract shared-ray fixtures cover all 4,096 domain triples (empty
masks included); propagation and feasibility also cover all six factor orders.
The physical six-ray corpus enumerates all 65,536 worlds and all 729 observation
vectors, including unattainable vectors, under unrestricted and deterministic
restricted domains. There are 1,458 physical projections and 46,656 requested
support queries. Existing tests also cover zero-cell scenes, unconstrained and
unobserved cells, rank ties, envelope shrinkage, input validation, contradictory
rays, correlated ambiguity, root/query context isolation, and deliberate audit
fault injection. Marginal Cartesian products are explicitly **not** claimed to
be simultaneous feasible worlds.

## Witness and pruning evidence

The new full-camera gate alone records:

| Measurement | Count |
| --- | ---: |
| Audited pipeline calls | 196,756 |
| Independently verified projection witnesses | 346,958 |
| Independently verified standalone feasibility witnesses | 196,335 |
| Independently verified direct-query witnesses | 54,697 |
| Supported literal certificates covered by projection witnesses | 1,724,491 |
| Direct-query comparisons | 212,628 |
| Supported / unsupported direct queries | 54,697 / 157,931 |
| Audited domain reductions | 10,909,842 |
| Audited literal deletions | 15,197,542 |
| Audited contradiction events | 1,493,828 |
| Unsound deletions, false contradictions, support mismatches | 0 |
| A1 search branches / nodes | 0 / 0 |

Counts include named-scene repeats and restricted cases; they are not distinct
world counts. Contradiction events include conditioned support calls, not just
infeasible root scenes. The 196,756 calls comprise 195,620 complete families,
512 restricted cases, and two order/duplication variants of 312 named cases.
Unobserved projection replays are excluded from the audit counters.

Every observed reduction is checked against the exhaustive family filtered by
that query's assumptions. The removed bits must contain no supported literal;
a contradiction must have an empty conditioned family. This observer is read-only
and supplies no inference decisions. Existing focused audits additionally check
the local envelope rule and restoration of unconditioned projection context.
Every returned witness is checked for input-domain membership, full independently
rendered-family membership, and acceptance by a scalar first-hit evaluator. The
union of projection witnesses must equal the exact exhaustive supported masks.
Direct-query witnesses must also contain the requested state.

Both frozen Python and C++ continue to use zero branches/search nodes. Review of
the unchanged production implementation confirms monotone envelope propagation,
maximal-state witness construction, and conditioned feasibility calls; no world
enumeration or combinatorial search enters the inference library.

## Scope limits

This is exhaustive over **all worlds and feasible observation families for the
eight declared camera suites**. It does not enumerate every possible camera,
every contradictory 512-pixel image, or all 16^8 eight-cell domain assignments.
Restricted full-camera tests are a seeded corpus; the smaller abstract domain
spaces and the six-ray label space are exhaustive. These bounded checks support
the existing nested-geometry argument; they are not a new proof for arbitrary
geometry or arbitrary-scale performance. No timing or identifiability study is
claimed from this correctness gate.

## Reproduction and saved evidence

The standard commands run all 28 tests in each build, including both new tests:

```sh
cmake --preset release
cmake --build --preset release --clean-first --parallel 2
ctest --preset release --parallel 2 --output-junit release-junit.xml
cmake --preset sanitize
cmake --build --preset sanitize --clean-first --parallel 2
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --preset sanitize --parallel 2 --output-junit sanitize-junit.xml
```

Python 3.12.14 / NumPy 2.3.5, GCC 13.3.0, CMake 4.4.3 and Ninja 1.13.2 were used.
All 28 Release tests passed. The full sanitizer run passed 27/28 initially; the
remaining comparison passed after the fixture recovery described below, completing
28/28 ASan/UBSan coverage. In this traced runtime,
`ASAN_OPTIONS=detect_leaks=0` was required; address/undefined-behavior checking
remained enabled. No LeakSanitizer result is claimed. On an ordinary host, keep
the normal leak-checking default where supported.

The first sanitizer run found an unexpected EOF in `local_supports.bin`.
That generated file was 118,784 bytes, an exact prefix of the expected 393,228
bytes; its hash disagreed with the successful exporter's own recorded hash.
The native sanitizer constraint test and all other tests passed. Regenerating the
fixture with the unchanged exporter restored the expected hash, and rerunning
`python_a1_constraint_agreement` plus its automatic setup fixture passed 2/2.
The initial failure, file sizes/hashes, and focused recovery are retained in
`fixture-recovery.json` and the `sanitize-initial-*` / `sanitize-recovery-*` logs.
No source fix or weakened assertion was involved; unaffected expensive checks
were not repeated. The Release and sanitizer full-camera reports are byte-identical.

The camera fixture was regenerated from frozen Python during the Release gate;
its SHA-256 is
`44d96bd735875da137f8776f9ba6dc430061a8c14dc6a1591a79f1fec19c5076`,
identical to the intact preserved fixture. The sanitizer gate reused that fixture
after validating its source and payload hashes. Fresh builds regenerate it;
cache reuse never bypasses the frozen-reference manifest or payload hash checks.
Generated binary fixtures stay in the build directories, not Git.

[`results/milestone-2c-correctness/`](../results/milestone-2c-correctness/)
contains both complete CTest transcripts and per-test logs, JUnit results, clean
build logs, A0/A1 reports, fixture metadata, and a checkpoint manifest tying these
to the tested source/configuration and frozen-reference hashes. CI now runs and
uploads the full-camera A1 acceptance report and its metadata as well.

The remaining checkpoint is the final C++ performance/identifiability benchmark
study, evidence review and final Milestone 2 record. None was started here.
