#pragma once
#include "mcr/inference/a2_problem.hpp"
#include "mcr/inference/observer.hpp"

namespace mcr {
using A2InferenceObserver=BasicInferenceObserver<A2Domain>;
struct A2SearchDiagnostics {
    std::uint64_t decompositions=0,component_solves=0;
    std::uint64_t fixed_hit_checks=0,fixed_hit_rejections=0;
};
struct A2SearchOptions {
    // fixed_hit also restricts branching to cells with differing relevant hit
    // patterns among occupied states. Their residual fixed-hit siblings need
    // no separate shape decision. With both flags false, use the original policy.
    bool decompose=true,fixed_hit=true;
    // Optional cumulative A2-only instrumentation; never supplies solver decisions.
    A2SearchDiagnostics* diagnostics=nullptr;
};
struct A2PropagationResult {
    // Local consistency only. Even all nonempty domains can be globally infeasible.
    bool consistent;
    A2Domains domains;
};
struct A2FeasibilityResult { bool feasible; std::optional<A2World> witness; };
struct A2SupportQueryResult { bool supported; std::optional<A2World> witness; };
struct A2SupportResult {
    bool feasible;
    // GAC domains, not exact supports; diagnostic partial domains on contradiction.
    A2Domains root_domains;
    // One empty mask per cell on infeasibility. Otherwise exact marginals only.
    A2Domains supported;
    // Collectively cover every supported literal; not an enumeration of worlds.
    std::vector<A2World> witnesses;
};
[[nodiscard]] A2PropagationResult gac(const A2Problem&,A2Domains,InferenceStats&,A2InferenceObserver* = nullptr);
// Complete finite branch-and-propagate search. No cutoff is treated as UNSAT.
// Each search node begins a fresh GAC audit context with its branch assumptions;
// branching is conditioning, not an unconditional deletion. Observer events
// cover propagation and final projection, not a separate search proof trace.
// Component events retain original cell/factor IDs. Disabling both optimizations
// retains the initial A2 branching policy for controlled correctness comparisons.
[[nodiscard]] A2FeasibilityResult exact_feasible(const A2Problem&,A2Domains,InferenceStats&,A2InferenceObserver* = nullptr,A2SearchOptions = {});
[[nodiscard]] A2SupportQueryResult query_support(const A2Problem&,A2Domains,CellId,A2State,InferenceStats&,A2InferenceObserver* = nullptr,A2SearchOptions = {});
[[nodiscard]] A2SupportResult exact_supports(const A2Problem&,A2Domains,InferenceStats&,A2InferenceObserver* = nullptr,A2SearchOptions = {});
} // namespace mcr
