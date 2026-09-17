#pragma once
#include "mcr/inference/observer.hpp"

namespace mcr {
[[nodiscard]] PropagationResult gac(const Problem&,Domains,InferenceStats&,InferenceObserver* = nullptr);
} // namespace mcr

