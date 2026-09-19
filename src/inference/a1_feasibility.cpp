#include "mcr/inference/a1_feasibility.hpp"
#include "work_queue.hpp"
#include <utility>

namespace mcr {
A1FeasibilityResult envelope_feasible(const A1Problem& problem,A1Domains domains,
                                      InferenceStats& stats,A1InferenceObserver* observer) {
    problem.validate_domains(domains);
    ++stats.envelope_calls;
    if(observer) observer->begin(QueryKind::nested_envelope,domains);
    for(auto domain:domains) if(domain.empty()) {
        if(observer) observer->contradiction(ContradictionReason::empty_domain,{});
        return {false,std::move(domains),{}};
    }
    detail::WorkQueue queue(problem);
    std::vector<std::size_t> front(problem.constraints().size(),0);
    while(!queue.empty()) {
        const auto r=queue.pop();
        const auto& factor=problem.constraints()[r];
        const auto steps=factor.steps();
        auto& k=front[r];
        ++stats.factor_updates;
        // Domain shrinkage can only remove envelope hits, never create an earlier hit.
        while(k<steps.size() && steps[k].emission(maximal_geometry(domains[steps[k].cell]))==PixelLabel::background) {
            ++k; ++stats.envelope_advances;
        }
        if(k==steps.size()) {
            if(factor.target()!=PixelLabel::background) {
                if(observer) observer->contradiction(ContradictionReason::foreground_escapes,r);
                return {false,std::move(domains),{}};
            }
            continue;
        }
        const auto& step=steps[k];
        const auto old=domains[step.cell];
        unsigned retained=0;
        for(auto s:a1_states) if(old.contains(s)) {
            const auto emitted=step.emission(s);
            if(emitted==PixelLabel::background || emitted==factor.target()) retained |= 1U<<code(s);
        }
        const A1Domain next(retained);
        if(next==old) continue;
        if(observer) observer->prune({step.cell,old,next,PruneReason::envelope_first_hit,r});
        stats.deletions+=old.size()-next.size();
        domains[step.cell]=next;
        if(next.empty()) {
            if(observer) observer->contradiction(ContradictionReason::envelope_empty,r);
            return {false,std::move(domains),{}};
        }
        queue.notify(step.cell);
    }
    A1World witness;
    for(auto domain:domains) witness.push_back(maximal_geometry(domain));
    if(!contains(domains,witness) || !problem.accepts(witness))
        throw std::logic_error("nested-envelope witness invariant failed");
    return {true,std::move(domains),std::move(witness)};
}
} // namespace mcr
