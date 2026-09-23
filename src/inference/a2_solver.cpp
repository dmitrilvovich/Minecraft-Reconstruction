#include "mcr/inference/a2_solver.hpp"
#include "gac_impl.hpp"
#include <bit>
#include <utility>

namespace mcr {
A2PropagationResult gac(const A2Problem& problem,A2Domains domains,InferenceStats& stats,A2InferenceObserver* observer) {
    return detail::propagate_gac<A2PropagationResult>(problem,std::move(domains),stats,observer);
}
namespace {
std::optional<CellId> residual_choice(const A2Problem& problem,const A2Domains& domains) {
    std::vector<std::size_t> degree(domains.size(),0);
    for(const auto& factor:problem.constraints()) {
        unsigned outputs=0;
        bool live=true;
        std::vector<CellId> scope;
        for(const auto& step:factor.steps()) {
            unsigned emissions=0;
            for(auto s:a2_states) if(domains[step.cell].contains(s))
                emissions |= 1U<<code(step.emission(s,problem.palette()));
            outputs |= emissions & ~1U;
            if(std::popcount(emissions)>1) scope.push_back(step.cell);
            if(!(emissions & 1U)) { live=false; break; }
        }
        if(live) outputs |= 1U;
        // Every allowed assignment satisfies this factor: it need not drive search.
        if(outputs==(1U<<code(factor.target()))) continue;
        for(auto v:scope) ++degree[v];
    }
    std::optional<CellId> choice;
    for(CellId v=0;v<domains.size();++v) if(degree[v] && domains[v].size()>1) {
        if(!choice || domains[v].size()<domains[*choice].size() ||
           (domains[v].size()==domains[*choice].size() && degree[v]>degree[*choice])) choice=v;
    }
    return choice;
}
std::optional<A2World> search(const A2Problem& problem,A2Domains domains,InferenceStats& stats,A2InferenceObserver* observer) {
    ++stats.search_nodes;
    auto root=gac(problem,std::move(domains),stats,observer);
    if(!root.consistent) return {};
    A2World candidate;
    for(auto d:root.domains) for(auto s:a2_states) if(d.contains(s)) { candidate.push_back(s); break; }
    // Accept a simultaneous assignment, never a collection of local supports.
    if(problem.accepts(candidate)) return candidate;
    const auto choice=residual_choice(problem,root.domains);
    if(!choice) throw std::logic_error("unsatisfied A2 factor has no residual variable");
    for(auto s:a2_states) if(root.domains[*choice].contains(s)) {
        ++stats.branches;
        auto child=root.domains;
        child[*choice]=A2Domain::singleton(s);
        if(auto witness=search(problem,std::move(child),stats,observer)) return witness;
    }
    return {}; // Every disjoint branch failed, so this conditioned scene is infeasible.
}
} // namespace
A2FeasibilityResult exact_feasible(const A2Problem& problem,A2Domains domains,InferenceStats& stats,A2InferenceObserver* observer) {
    problem.validate_domains(domains);
    auto witness=search(problem,std::move(domains),stats,observer);
    const bool feasible=witness.has_value();
    return {feasible,std::move(witness)};
}
A2SupportQueryResult query_support(const A2Problem& problem,A2Domains domains,CellId cell,A2State state,
                                  InferenceStats& stats,A2InferenceObserver* observer) {
    problem.validate_domains(domains);
    if(cell>=problem.cell_count()) throw std::out_of_range("invalid A2 support cell");
    domains[cell]=domains[cell] & A2Domain::singleton(state);
    ++stats.literal_queries;
    auto result=exact_feasible(problem,std::move(domains),stats,observer);
    if(!result.feasible) ++stats.infeasible_queries;
    if(result.feasible && result.witness->at(cell)!=state) throw std::logic_error("A2 query lost its condition");
    return {result.feasible,std::move(result.witness)};
}
A2SupportResult exact_supports(const A2Problem& problem,A2Domains initial,InferenceStats& stats,A2InferenceObserver* observer) {
    auto root=gac(problem,std::move(initial),stats,observer);
    A2SupportResult result{false,std::move(root.domains),A2Domains(problem.cell_count(),A2Domain(0)),{}};
    if(!root.consistent) return result;
    auto decision=exact_feasible(problem,result.root_domains,stats,observer);
    if(!decision.feasible) return result;
    result.feasible=true;
    const auto certify=[&](A2World witness) {
        for(CellId v=0;v<witness.size();++v)
            result.supported[v]=A2Domain(result.supported[v].bits() | (1U<<code(witness[v])));
        result.witnesses.push_back(std::move(witness));
    };
    certify(std::move(*decision.witness));
    for(CellId v=0;v<result.root_domains.size();++v) for(auto s:a2_states) {
        if(!result.root_domains[v].contains(s) || result.supported[v].contains(s)) continue;
        auto query=query_support(problem,result.root_domains,v,s,stats,observer);
        if(query.supported) certify(std::move(*query.witness));
    }
    if(observer) observer->begin(QueryKind::support_projection,result.root_domains);
    for(CellId v=0;v<result.root_domains.size();++v) if(result.root_domains[v]!=result.supported[v]) {
        if(observer) observer->prune({v,result.root_domains[v],result.supported[v],PruneReason::unsupported_state,{}});
        stats.deletions+=result.root_domains[v].size()-result.supported[v].size();
    }
    return result;
}
} // namespace mcr
