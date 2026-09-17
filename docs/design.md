# C++ Phase A design — Milestone 1

This design was recorded before implementation. The C++ library is the primary
project. The unmodified Python experiment remains an independent research oracle.
Only A0 is in scope: a 2 x 2 x 2 lattice, air / stone cube / oak cube, known
pinhole cameras, exact first-hit material observations.

## Mathematical objects and modules

| Module | Objects | Responsibility |
|---|---|---|
| `math` | `Rational`, `Vec3` | Exact arithmetic, signed floor, vector operations |
| `geometry` | `Ray`, `Aabb`, `HitInterval` | Independent positive-length ray/box intersection |
| `camera` | `PinholeCamera`, exact basis and intrinsics | Generate the frozen protocol's pixel-center rays |
| `grid` | `Grid`, `CellId`, `CellVisit` | Order grid-plane events and identify intervals by midpoint |
| `model` | `A0State`, `Domain`, `World` | Three-state alphabet, little-endian base-three world IDs |
| `render` | `AabbReferenceRay`, `TraversalRay` | Independent forward paths; no observations reveal depth or cell IDs |
| `inference` | `FirstHitConstraint`, `Problem`, `Domains` | Preserve correlated ray constraints and variable incidence |
| `inference` | GAC, fixed-geometry elimination, support queries | Separate local consistency, complete feasibility, exact projections |
| `experiments` | View suites and benchmark records | Reproducible A0 runs and machine-readable results |
| `tests` | Exhaustive families and audit observer | Independent validation, including every pruning event |

Small value types and free functions are sufficient. There is no block-shape
class hierarchy, plugin system, general search engine, or application layer in
Milestone 1. State and observed-label types are distinct even though their A0
numeric encodings coincide. AABB reference code must not call grid traversal;
grid traversal must not call the intersection routine. Sharing exact arithmetic
and the declared camera model is intentional; Python checks these as well.

## Exact representation

Coordinates and ray parameters use normalized rational numbers. The initial
implementation uses checked signed 64-bit numerator/denominator arithmetic:
overflow and division by zero throw, never round or silently wrap. Comparisons
must remain exact even when cross-products would overflow. This is sufficient
for the declared tiny camera model, and failures outside its arithmetic range
are explicit errors, never infeasibility. The numeric type is isolated so an
arbitrary-precision backend can be substituted later without changing inference.

Cells are half-open boxes. Parallel coordinates use `lo <= c < hi`; nonparallel
intersections require `enter < leave` after clipping to `t >= 0`. Zero-length
tangencies are misses. Traversal sorts exact positive grid-plane events together
with zero, then floors interval midpoints. Simultaneous crossings are one event.
Cell indices are `(x*ny+y)*nz+z`; world IDs use cell zero as the least-significant
base-three digit. No floating-point geometry or epsilon enters the model.

## Inference and proof boundary

A constraint stores an ordered list of distinct cells and an observed material.
A0 emissions are fixed: air misses; each non-air state emits its material. A
two-state automaton tracks `alive` and `done`. Forward/backward reachability gives
exact support within one ray. A work queue applies generalized arc consistency
until domains stop shrinking. GAC alone does not certify global support.

The complete A0 feasibility routine maintains the maximal candidate occupancy.
Only its first active cell on a ray can be pruned for a wrong material. No
earlier allowed geometry can hide that cell. Removing all materials from an
optional cell exposes later cells; removing them from a required cell proves a
contradiction. At a fixed point, selecting a retained material in every active
cell constructs a simultaneous feasible witness. Every retained non-air state
has a witness. Air is certified by a complete feasibility query with that cell
fixed to air. These are conditional queries, not search branches.

Public results distinguish feasible/infeasible and carry a witness, root domains,
and exact supported domains where requested. A result never presents the product
of marginal domains as the feasible world set. Empty input domains are explicitly
infeasible. Malformed inputs and numeric range errors are exceptions.

An optional observer receives query assumptions, reductions with reasons, and
contradictions. It returns no supports to the solver. Tests filter the exhaustive
family under each query's assumptions and reject any deletion of a supported
literal. Timing runs disable the observer and the oracle entirely.

## Validation and milestone commits

1. Freeze the research reference and record the design.
2. Exact arithmetic, pinhole rays, independent AABB geometry and grid traversal.
3. A0 state representation and both renderers; all 6,561 worlds agree with Python.
4. First-hit automata and queue-based GAC with exhaustive local-support tests.
5. Complete A0 inference, all observation classes, oracle audits and adversarial tests.
6. Reproducible results, CI instructions, and the Milestone 1 acceptance record.

For eight cameras at 8 x 8, compare all 512 raw pixel outputs for every world.
For each of the six axis-camera prefixes, one oblique view and the opposite
oblique pair, test **every** distinct observation class. Compare entire class
membership, exact supports, witnesses and Python solver results. Add deliberately
inconsistent observations and restricted-domain queries, since rendered truths
alone cannot test false feasibility. Boundary and fault-injection tests are
separate from this enumerated corpus. Do not proceed to A1 until A0 passes.

