#include "mcr/inference/gac.hpp"
#include "work_queue.hpp"
#include <utility>

namespace mcr {
PropagationResult gac(const Problem& problem,Domains domains,InferenceStats& stats,InferenceObserver* observer) {
    problem.validate_domains(domains);
    ++stats.gac_calls;
    if(observer) observer->begin(QueryKind::gac,domains);
    for(auto d:domains) if(d.empty()) {
        if(observer) observer->contradiction(ContradictionReason::empty_domain,{});
        return {false,std::move(domains)};
    }
    detail::WorkQueue queue(problem);
    while(!queue.empty()) {
        const auto r=queue.pop();
        const auto& factor=problem.constraints()[r];
        ++stats.factor_updates;
        const auto local=factor_supports(factor,domains);
        if(!local.feasible) {
            if(observer) observer->contradiction(ContradictionReason::no_ray_support,r);
            return {false,std::move(domains)};
        }
        const auto cells=factor.cells();
        for(std::size_t j=0;j<cells.size();++j) {
            const auto v=cells[j];
            const auto old=domains[v],next=old & local.masks[j];
            if(next==old) continue;
            if(observer) observer->prune({v,old,next,PruneReason::ray_arc_support,r});
            stats.deletions+=old.size()-next.size();
            domains[v]=next;
            queue.notify(v);
        }
    }
    return {true,std::move(domains)};
}
} // namespace mcr

