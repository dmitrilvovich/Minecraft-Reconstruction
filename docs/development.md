# Development rules

Keep the C++ library primary. Do not extend the frozen Python solver. Reference
changes require a separately explained correctness fix, regenerated hashes, and
regression evidence; adapters outside `reference/python/` may only export data
or invoke existing reference operations.

The mathematical gate takes precedence over reconstruction accuracy or speed:
any false infeasibility, false witness, unsupported claimed state, missing valid
state, or unsound deletion fails acceptance. Minimize the failing scene/query
and add a named regression before continuing. Do not turn timeouts or numeric
errors into infeasibility. Do not hide failures by weakening the observation
model, changing camera rays, or silently dropping contradictory duplicates.

Every solver reduction must use the observer interface when an observer is
present. New query assumptions start a new conditioned audit context. The audit
may throw on an invalid action but cannot return masks or influence decisions.

Geometry and observation semantics are frozen for this milestone. Keep the
AABB renderer independent of grid traversal. Exact material observations do not
contain hit depth, generating cell, normals, or other latent information.
Optional occupancy and full marginal support are not the same as independence.

Before committing an inference change, run complete Release acceptance. For
changes affecting indexing, arithmetic, or storage, also run the sanitizer
preset. Timings are meaningful only with audit/oracle work excluded, and only
after correctness passes. Benchmark output must state its sampling, root
propagation procedure, timed scope, build configuration, and memory scope.

Future milestone order is A1, A2, scaling. A1 must use the nested-envelope
feasibility theorem; do not mistake avoidable generic-search work for inherent
hardness. A2 requires its own sound propagation and complete search tests before
larger experiments. No camera recovery or broader block vocabulary is authorized
by the current milestone.
