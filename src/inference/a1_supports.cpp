#include "mcr/inference/a1_supports.hpp"
#include <utility>

namespace mcr {
A1SupportResult exact_supports(const A1Problem& problem,A1Domains initial,
                               InferenceStats& stats,A1InferenceObserver* observer) {
    auto root=envelope_feasible(problem,std::move(initial),stats,observer);
    A1SupportResult result{root.feasible,A1Domains(problem.cell_count(),A1Domain(0)),{}};
    if(!root.feasible) return result;
    const auto certify=[&](A1World witness) {
        for(CellId v=0;v<witness.size();++v)
            result.supported[v]=A1Domain(result.supported[v].bits() | (1U<<code(witness[v])));
        result.witnesses.push_back(std::move(witness));
    };
    certify(std::move(*root.witness));
    for(CellId v=0;v<root.domains.size();++v) for(auto state:a1_states) {
        if(!root.domains[v].contains(state) || result.supported[v].contains(state)) continue;
        auto query=query_support(problem,root.domains,v,state,stats,observer);
        if(query.supported) certify(std::move(*query.witness));
    }
    // Conditioned query deletions must never be mistaken for global deletions.
    if(observer) observer->begin(QueryKind::support_projection,root.domains);
    for(CellId v=0;v<root.domains.size();++v) if(root.domains[v]!=result.supported[v]) {
        if(observer) observer->prune({v,root.domains[v],result.supported[v],PruneReason::unsupported_state,{}});
        stats.deletions+=root.domains[v].size()-result.supported[v].size();
    }
    for(const auto& witness:result.witnesses)
        if(!contains(result.supported,witness)) throw std::logic_error("A1 projection lost a witness");
    return result;
}
} // namespace mcr
