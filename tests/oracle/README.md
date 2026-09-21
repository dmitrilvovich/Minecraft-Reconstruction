# Cross-language fixtures

`export_a0.py` verifies every imported file against `reference/SHA256.json`,
then imports the unmodified Python prototype. It generates only regression data;
it does not implement or extend reconstruction inference.

Outputs, under the CMake build directory:

* `rays.txt`: 512 origins and directions, each rational as numerator/denominator.
* `images.bin`: unsigned byte labels, ray-major, then world ID; 512 x 6,561 bytes.
* `cases.bin`: complete observation families, Python root/supported domains,
  exhaustive domains, freedom counts, and all cell/state occurrence counts.
  It also contains 512 restricted-domain or altered-observation fixtures.
* `metadata.json`: source and payload SHA-256 hashes, versions and case counts.

`cases.bin` uses magic `MCRA0C1\n`, little-endian unsigned 32-bit counts/IDs,
and unsigned byte state masks/labels. It first stores the ordered cell chains,
then the eight view suites. Each class lists all ascending world IDs, followed
by three eight-byte mask arrays (Python root, Python supports, exhaustive
supports), one freedom count byte, then 24 cell-major occurrence counts. Query
records contain factor IDs and labels, eight initial domains, a feasibility
byte, eight exhaustive support masks and the complete conditioned world IDs.

The C++ acceptance executable independently constructs AABB images and partitions
all worlds by observation signatures. It compares the **entire family membership**,
not just counts or marginal masks. The per-ray test separately checks all raw
pixels so interning identical A0 cell chains cannot conceal a rendering error.

`audit.hpp` is test code, never called for inference decisions. At every query
start it filters the original feasible family by the explicit domain assumptions.
Each domain reduction must preserve every supported literal in that conditioned
family; each contradiction must have an empty conditioned family. A deliberate
hidden-oak deletion and a false contradiction must both be rejected by the audit.

The same cases run again without the observer and must return the same result.
The benchmark independently disables both oracle access and audit work within
its timed regions. No C++ test can pass full acceptance by skipping Python export.


## Full-camera A1 correctness fixture

`export_a1_acceptance.py` calls the frozen A1 renderer, enumeration, GAC,
`nested_supports`, and conditioned `envelope_feasible` routines. It introduces
no Python solver. `acceptance_cases.bin` has magic `MCRA1C1\n` and follows the
A0 case layout with four states: each factor step is a u32 cell plus one slab-hit
byte; each family has three eight-byte mask arrays, one freedom-count byte and
32 cell-major u32 occurrence counts. All eight suites are complete (195,620
families), followed by 512 seeded restricted/altered/contradictory cases.
`acceptance_metadata.json` records input/payload hashes and case counts.
A cached payload is reused only when its adapter/reference and payload hashes
match; the complete frozen-reference manifest is always checked.

`fixtures/a1_corpus.*` builds a separate C++ AABB-rendered exhaustive corpus;
`a1_acceptance.cpp` compares complete families, standalone feasibility, GAC,
projection, direct support queries and actual witnesses. The test-only
`a1_acceptance_audit.hpp` checks every reported pruning/contradiction against the
exhaustive family restricted by the current assumptions. Existing focused
feasibility/projection audits additionally enforce the local rule and correct
query/projection context transitions. Enumeration data never enters the library.
See [the correctness gate](../../docs/milestone-2c-correctness.md) for exact counts,
coverage limits, source hashes, commands and sanitizer recovery evidence.
