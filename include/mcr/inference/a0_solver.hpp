#pragma once
#include "mcr/inference/gac.hpp"

namespace mcr {
struct FeasibilityResult {
    bool feasible;
    Domains domains;
    std::optional<World> witness;
};
struct SupportResult {
    bool feasible;
    Domains root_domains;
    // Empty on infeasibility. Marginals are not a Cartesian description of solutions.
    Domains supported;
    std::optional<World> witness;
};
[[nodiscard]] FeasibilityResult fixed_geometry(const Problem&,Domains,InferenceStats&,InferenceObserver* = nullptr);
[[nodiscard]] SupportResult exact_supports(const Problem&,Domains,InferenceStats&,InferenceObserver* = nullptr);
} // namespace mcr

