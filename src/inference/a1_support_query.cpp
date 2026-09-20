#include "mcr/inference/a1_support_query.hpp"
#include <utility>

namespace mcr {
A1SupportQueryResult query_support(const A1Problem& problem,A1Domains domains,
                                  CellId cell,A1State state,InferenceStats& stats,
                                  A1InferenceObserver* observer) {
    problem.validate_domains(domains);
    if(cell>=problem.cell_count()) throw std::out_of_range("invalid A1 support cell");
    const auto singleton=A1Domain::singleton(state);
    domains[cell]=domains[cell] & singleton;
    ++stats.literal_queries;
    auto result=envelope_feasible(problem,std::move(domains),stats,observer);
    if(!result.feasible) ++stats.infeasible_queries;
    if(result.feasible && (!result.witness || result.witness->at(cell)!=state))
        throw std::logic_error("A1 support witness violated its condition");
    return {result.feasible,std::move(result.witness)};
}
} // namespace mcr
