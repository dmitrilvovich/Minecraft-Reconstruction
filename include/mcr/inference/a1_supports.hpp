#pragma once
#include "mcr/inference/a1_support_query.hpp"

namespace mcr {
struct A1SupportResult {
    bool feasible;
    // One exact marginal per input cell; all masks are empty on infeasibility.
    // Their Cartesian product need not describe the feasible scenes.
    A1Domains supported;
    // Valid simultaneous scenes whose union covers every supported literal.
    // Nonempty iff feasible, including one empty world for a zero-cell problem.
    std::vector<A1World> witnesses;
};
// Root feasibility, then query_support for candidates not certified by witnesses.
// Inputs are copied. Query audits use conditioned domains; the final projection
// audit restores unconditioned root domains before reporting global exclusions.
[[nodiscard]] A1SupportResult exact_supports(
    const A1Problem&,A1Domains,InferenceStats&,A1InferenceObserver* = nullptr);
} // namespace mcr
