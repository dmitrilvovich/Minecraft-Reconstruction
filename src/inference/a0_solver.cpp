#include "mcr/inference/a0_solver.hpp"
#include "work_queue.hpp"
#include <utility>

namespace mcr {
FeasibilityResult fixed_geometry(const Problem& problem,Domains domains,
                                 InferenceStats& stats,InferenceObserver* observer) {
    problem.validate_domains(domains);
    ++stats.fixed_calls;
    if(observer) observer->begin(QueryKind::fixed_geometry,domains);
    for(auto d:domains) if(d.empty()) {
        if(observer) observer->contradiction(ContradictionReason::empty_domain,{});
        return {false,std::move(domains),{}};
    }
    detail::WorkQueue queue(problem);
    std::vector<std::size_t> front(problem.constraints().size(),0);
    while(!queue.empty()) {
        const auto r=queue.pop();
        const auto& factor=problem.constraints()[r];
        const auto cells=factor.cells();
        auto& k=front[r];
        ++stats.factor_updates;
        while(k<cells.size() && domains[cells[k]].occupied().empty()) ++k;
        if(k==cells.size()) {
            if(factor.target()!=PixelLabel::background) {
                if(observer) observer->contradiction(ContradictionReason::foreground_escapes,r);
                return {false,std::move(domains),{}};
            }
            continue;
        }
        const auto v=cells[k];
        const auto old=domains[v];
        const unsigned allowed=factor.target()==PixelLabel::background ? 0U : 1U<<code(factor.target());
        const Domain next((old.bits() & 1U) | (old.occupied().bits() & allowed));
        if(next==old) continue;
        if(observer) observer->prune({v,old,next,PruneReason::first_active_material,r});
        stats.deletions+=old.size()-next.size();
        domains[v]=next;
        if(next.empty()) {
            if(observer) observer->contradiction(ContradictionReason::required_cell_empty,r);
            return {false,std::move(domains),{}};
        }
        // Shrinking material labels alone does not alter visibility.
        if(next.occupied().empty()) queue.notify(v);
    }
    World witness(domains.size(),A0State::air);
    for(CellId v=0;v<domains.size();++v) {
        const auto occupied=domains[v].occupied();
        for(auto s:a0_states) if(occupied.contains(s)) { witness[v]=s; break; }
    }
    // A fixed point must construct an actual world, not merely plausible domains.
    if(!contains(domains,witness) || !problem.accepts(witness))
        throw std::logic_error("fixed-geometry witness invariant failed");
    return {true,std::move(domains),std::move(witness)};
}

SupportResult exact_supports(const Problem& problem,Domains initial,
                             InferenceStats& stats,InferenceObserver* observer) {
    auto fixed=fixed_geometry(problem,std::move(initial),stats,observer);
    if(!fixed.feasible) return {false,std::move(fixed.domains),{}, {}};
    auto root=gac(problem,std::move(fixed.domains),stats,observer);
    if(!root.feasible) throw std::logic_error("GAC rejected a fixed-geometry witness");
    Domains support(root.domains.size(),Domain(0));
    for(CellId v=0;v<support.size();++v) {
        unsigned bits=root.domains[v].occupied().bits();
        if(root.domains[v].contains(A0State::air)) {
            auto probe=root.domains;
            probe[v]=Domain::singleton(A0State::air);
            ++stats.literal_queries;
            if(fixed_geometry(problem,std::move(probe),stats,observer).feasible) bits|=1U;
            else ++stats.infeasible_queries;
        }
        support[v]=Domain(bits);
    }
    // Restore the root assumptions before auditing any globally excluded air.
    if(observer) observer->begin(QueryKind::support_projection,root.domains);
    for(CellId v=0;v<support.size();++v) if(support[v]!=root.domains[v]) {
        if(observer) observer->prune({v,root.domains[v],support[v],PruneReason::unsupported_air,{}});
        stats.deletions+=root.domains[v].size()-support[v].size();
    }
    if(!contains(support,*fixed.witness)) throw std::logic_error("support result lost a feasible witness");
    return {true,std::move(root.domains),std::move(support),std::move(fixed.witness)};
}
} // namespace mcr

