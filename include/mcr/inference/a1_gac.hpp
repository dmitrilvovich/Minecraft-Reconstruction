#pragma once
#include "mcr/inference/a1_problem.hpp"
#include "mcr/inference/observer.hpp"

namespace mcr {
using A1PruneEvent=BasicPruneEvent<A1Domain>;
using A1InferenceObserver=BasicInferenceObserver<A1Domain>;
struct A1PropagationResult {
    // True means a nonempty GAC fixed point, not a global-support certificate.
    bool consistent;
    // On contradiction these are diagnostic partial domains, possibly order-dependent.
    A1Domains domains;
};
// Inputs are copied; every reduction is reported before mutation when observed.
[[nodiscard]] A1PropagationResult gac(const A1Problem&,A1Domains,InferenceStats&,A1InferenceObserver* = nullptr);
} // namespace mcr
