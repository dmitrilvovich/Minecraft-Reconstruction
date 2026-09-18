#include "mcr/inference/a1_gac.hpp"
#include "gac_impl.hpp"
#include <utility>

namespace mcr {
A1PropagationResult gac(const A1Problem& problem,A1Domains domains,
                        InferenceStats& stats,A1InferenceObserver* observer) {
    return detail::propagate_gac<A1PropagationResult>(problem,std::move(domains),stats,observer);
}
} // namespace mcr
