# Milestone 2B, checkpoint 1: exact A1 ray constraints

Base: published `main` at `caabe86bca01e9e260630651cbbe00dc38c17223`.
This checkpoint connects the validated A1 renderer to inference by implementing
the exact relation and supported-state masks of one observed ray. It does not
implement scene-wide propagation, envelope feasibility, or global supports.

## Why this increment

The frozen protocol, section 7, defines each ray by its ordered cells and
state-dependent emissions. Both root propagation and the complete nested
feasibility procedure need this representation. It can be validated separately
against direct enumeration, Python's `factor_supports`, and the independent
renderers, before adding interactions among rays.

The interrupted working directory retained a larger A1 implementation. This
checkpoint reuses its shared automaton and A1 constraint code, while excluding
its scene container, GAC, envelope solver, support-query solver and benchmarks.
Malformed A1 world inputs now receive a complete check of referenced cells
before evaluation, so an earlier rejected hit cannot conceal an invalid index
or state later in the ray.

## API and mathematical contract

`A1Constraint(steps, target)` combines the existing `A1RayCell` sequence with a
background, stone or oak observation. Duplicate cells and invalid labels are
rejected. Slabs retain their existing per-ray hit/miss flag; there is no new
observed slab color.

`accepts(world)` evaluates the first-hit relation. `factor_supports(factor,
domains)` returns feasibility and one mask **per ray position**, in traversal
order. Domains are indexed by scene cell and are not modified. Only cells
appearing in the ray are considered; an empty domain elsewhere is a concern
for a future scene-level routine. An empty domain on the ray makes the factor
infeasible, even behind an occluder, because no assignment to that variable
exists. Infeasible results contain no masks; a feasible empty ray also has an
empty mask vector, distinguished by the feasibility flag.

The shared automaton tracks whether a correct first hit has occurred. Forward
reachability and backward acceptance identify every allowed state on an
accepting path. Since a cell occurs only once, such a path is an actual factor
assignment. This proves exact local support; it does not establish global
support in a conjunction of rays. With four A1 states and two automaton modes,
time and temporary memory are linear in ray length. There is no search tree.

The A0 public API is retained and uses the same emission-based transition and
support kernel. This avoids maintaining two copies of the recurrence. The
renderer and frozen Python solver remain independent of that C++ kernel.

## Validation scope

* 98,304 local cases: all eight slab-hit patterns, all `16^3` three-cell domain
  combinations including empty masks, and all three observations. Each result
  is compared with direct enumeration of all 64 assignments and the frozen
  Python routine. Exact masks check both missing supports and unsound retention
  or deletion; input immutability, contraction and idempotence are also checked.
* Explicit cube/slab ambiguity, slab misses, occlusion, empty rays, foreground
  escape, malformed inputs, and the distinction between ray and scene scope.
* 14,352,384 automaton decisions: all 65,536 worlds and all three target labels
  for each of the 73 distinct camera-ray programs, checked against independent
  AABB rendering. Program identity includes slab-hit flags.
* The complete A0/2A regression suite, including all 33,554,432 A1 ray/world
  renderer comparisons, the 2A geometry cases, A0 exact inference, and A0
  pruning audits. Release and address/undefined-behavior sanitizer gates apply.

The complete Release suite passed **14/14** tests (30.13 seconds). The complete
ASan/UBSan suite passed **14/14** tests (399.73 seconds), with leak detection
disabled for this traced execution environment. These are validation timings,
including oracle generation and exhaustive auditing, not solver benchmarks.

The new Python adapter only invokes the existing frozen routine and exports
its results. `reference/python/` and its hash manifest are unchanged.
Recorded outcomes, commands and tested source hashes are in
[`results/milestone-2b-rays/`](../results/milestone-2b-rays/).

## Next increment

Combine A1 ray factors into a scene problem and add audited multi-ray domain
propagation. Treat its fixed point as local consistency, not proof of global
support. Later increments must establish complete nested-envelope feasibility,
simultaneous witnesses and exact supported-state queries, with all pruning
audited and zero search branches/nodes. No A2 work is included.
