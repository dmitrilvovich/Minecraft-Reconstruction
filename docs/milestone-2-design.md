# C++ Milestone 2: A1 nested geometry

Continue the uploaded published repository on main, initially at
`42e958c2e16c779bdf5c48c3841cced7ba33c580`. The existing
`cpp-milestone-1` tag resolves to `235971a41287e3e97ea058c5211912953886c67e`.
No existing commits or refs are rewritten. Commit author and committer both use
the repository's existing local Git identity. No push is part of this task.

The uploaded Windows checkout had 67 CRLF-only differences from HEAD and lacked
Git's standard empty-tree object. The checkout bytes were restored to their exact
committed versions after checking every difference; the known empty object was
restored without changing any commit or ref. Frozen-reference hashes match.
The new attributes preserve those reference bytes on subsequent checkouts.

## Scope

Keep the 2 x 2 x 2 volume, exact cameras, 8 x 8 pixel sampling and three observed
labels. States are air=0, stone full cube=1, oak full cube=2, oak bottom slab=3.
The slab occupies the lower half of its unit cell in y and emits the same oak
label as the full oak cube. Enumerate all 4^8 = 65,536 worlds.

Geometry is ordered by containment, with ranks (0,2,2,1). The two rank-two states
have identical geometry. Choosing a maximal remaining geometry always selects
an actual allowed state, never a union that is absent from the vocabulary.

## Modules and invariants

* Share exact coordinates, pinhole cameras and grid-plane traversal with A0.
* Introduce a distinct A1 state/domain type; keep A0's three-state API and tests.
  Share finite-domain utilities and the two-state automaton/GAC algorithms.
* Extend rendering in two independent paths. The reference intersects each
  state's AABB. The production path splits a visited cell interval at its
  half-height plane and tests interval midpoints. The slab has no special pixel
  label. Neither renderer borrows the other one's intersection logic.
* A1 ray steps store the cell and whether the bottom slab intersects this ray.
  This encodes exactly four emissions: background, stone, oak, and oak-or-miss.
  Interning requires equality of both the cell sequence and every slab-hit flag.
* GAC uses forward/backward automaton reachability. Envelope feasibility visits
  the frontmost maximal-geometry hit, removes only wrong-color states that hit,
  and preserves states that miss. Every deletion requeues incident rays.
* At a fixed point, maximal states form a legal simultaneous witness. Root GAC
  and root envelope domains are recorded separately. Exact supported domains
  use complete singleton feasibility queries and reuse witnesses to certify
  other literals. Query assumptions are not search branches.
* The envelope solver accepts only the declared nested A1 model. There is no
  generic shape search or A2 implementation in this milestone.

An optional read-only audit checks every root/query reduction against the
conditioned exhaustive family and every contradiction against an empty family.
Audit data cannot supply supports or decisions to inference. All feasible query
witnesses are checked independently; unaudited runs must return the same domains.

## Acceptance plan

1. Preserve all original A0 acceptance tests as regression gates.
2. Compare all 65,536 worlds on all 512 raw camera rays against Python and both
   C++ renderers: 33,554,432 ray/world tuples.
3. Check every distinct observation family in all eight existing view suites,
   including full family membership, cell/state counts, exact domains and
   witnesses. Use the frozen Python nested solver as a second inference oracle.
4. Exercise contradictory observations, restricted domains, empty domains,
   same-material cube/slab ambiguity, slab-surface tangencies, rank ties,
   envelope shrinkage and missing-state fault injection.
5. Record zero branches/search nodes, all pruning audits, complete Release and
   address/undefined-behavior checks, and C++ benchmark CSV/JSON. Timed solver
   calls exclude oracle, audit and problem construction; state memory scope.
6. Record acceptance, tag cpp-milestone-2 only after passing, and stop before A2.

Python source and its existing manifests/results remain frozen. New Python code
is limited to fixture export adapters calling existing oracle functions.
