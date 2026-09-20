#pragma once
#include "mcr/inference/a1_feasibility.hpp"

namespace mcr {
struct A1SupportQueryResult {
    bool supported;
    // Present iff supported; satisfies the original domains and X_cell=state.
    std::optional<A1World> witness;
};
// Decide one literal, not a projection of all supported domains. Inputs are copied.
// Intersect the requested singleton with the existing domain; never reintroduce it.
// The feasibility observer starts a fresh context under these conditioned domains.
// Invalid cell/state/domain count throws before counters or observer events change.
[[nodiscard]] A1SupportQueryResult query_support(
    const A1Problem&,A1Domains,CellId,A1State,InferenceStats&,A1InferenceObserver* = nullptr);
} // namespace mcr
