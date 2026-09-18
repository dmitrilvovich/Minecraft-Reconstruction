#include "mcr/inference/gac.hpp"
#include "gac_impl.hpp"
#include <utility>

namespace mcr {
PropagationResult gac(const Problem& problem,Domains domains,InferenceStats& stats,InferenceObserver* observer) {
    return detail::propagate_gac<PropagationResult>(problem,std::move(domains),stats,observer);
}
} // namespace mcr
