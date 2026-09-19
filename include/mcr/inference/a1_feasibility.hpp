#pragma once
#include "mcr/inference/a1_gac.hpp"

namespace mcr {
struct A1FeasibilityResult {
    bool feasible;
    // Soundly reduced domains, not a claim of exact global support.
    // Infeasible results retain diagnostic partial domains.
    A1Domains domains;
    // Present iff feasible, including the empty world for a zero-cell problem.
    std::optional<A1World> witness;
};
// Complete for the fixed, nested A1 vocabulary; no branching or support queries.
// The input domains are copied. All reductions use the optional read-only observer.
[[nodiscard]] A1FeasibilityResult envelope_feasible(
    const A1Problem&,A1Domains,InferenceStats&,A1InferenceObserver* = nullptr);
} // namespace mcr
