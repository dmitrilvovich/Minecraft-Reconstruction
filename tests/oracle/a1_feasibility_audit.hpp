#pragma once
#include "test.hpp"
#include "mcr/inference/a1_feasibility.hpp"
#include <algorithm>

namespace mcr::test {
// Independent scalar labels: no production emission, rank, envelope or automaton.
inline unsigned a1_scalar_emission(const A1RayCell& step,unsigned state) {
    if(state==0 || (state==3 && !step.slab_hit)) return 0;
    return state==1 ? 1U : 2U;
}
class A1FeasibilityAudit final : public A1InferenceObserver {
public:
    A1FeasibilityAudit(const A1Problem& problem,std::span<const A1World> worlds,
                       std::span<const std::size_t> family)
        : problem_(problem),worlds_(worlds),family_(family) {}
    void begin(QueryKind kind,std::span<const A1Domain> assumptions) override {
        CHECK(kind==QueryKind::nested_envelope);
        current.assign(assumptions.begin(),assumptions.end());
        support_.assign(current.size(),A1Domain(0)); conditioned.clear();
        last_cell_.reset(); last_factor_.reset();
        for(auto id:family_) if(contains(assumptions,worlds_[id])) {
            conditioned.push_back(id);
            for(CellId v=0;v<current.size();++v)
                support_[v]=A1Domain(support_[v].bits() | (1U<<code(worlds_[id][v])));
        }
        ++contexts;
    }
    void prune(const A1PruneEvent& event) override {
        CHECK(event.cell<current.size() && event.before==current[event.cell]);
        CHECK(event.after!=event.before && (event.after & event.before)==event.after);
        CHECK(event.reason==PruneReason::envelope_first_hit && event.factor.has_value());
        CHECK(*event.factor<problem_.constraints().size());
        const auto& factor=problem_.constraints()[*event.factor];
        const auto position=first_possible(factor);
        CHECK(position.has_value() && factor.steps()[*position].cell==event.cell);
        unsigned expected=0;
        for(unsigned s=0;s<4;++s) if(event.before.bits() & (1U<<s)) {
            const auto label=a1_scalar_emission(factor.steps()[*position],s);
            if(label==0 || label==code(factor.target())) expected|=1U<<s;
        }
        // Exact local justification, even when the global family is empty.
        CHECK(event.after.bits()==expected);
        // Preserve every globally feasible literal under the original assumptions.
        CHECK(((event.before.bits() ^ event.after.bits()) & support_[event.cell].bits())==0);
        current[event.cell]=event.after; last_cell_=event.cell; last_factor_=event.factor;
        ++steps; deletions+=event.before.size()-event.after.size();
    }
    void contradiction(ContradictionReason reason,std::optional<std::size_t> factor) override {
        CHECK(conditioned.empty());
        if(reason==ContradictionReason::empty_domain) {
            CHECK(!factor && std::any_of(current.begin(),current.end(),[](auto d){return d.empty();}));
        } else if(reason==ContradictionReason::envelope_empty) {
            CHECK(last_cell_ && factor && factor==last_factor_ && current[*last_cell_].empty());
        } else {
            CHECK(reason==ContradictionReason::foreground_escapes && factor);
            CHECK(*factor<problem_.constraints().size());
            const auto& ray=problem_.constraints()[*factor];
            CHECK(ray.target()!=PixelLabel::background && !first_possible(ray));
        }
        ++contradictions;
    }
    A1Domains current;
    std::vector<std::size_t> conditioned;
    std::uint64_t contexts=0,steps=0,deletions=0,contradictions=0;
private:
    std::optional<std::size_t> first_possible(const A1Constraint& factor) const {
        for(std::size_t j=0;j<factor.steps().size();++j) {
            const auto& step=factor.steps()[j];
            for(unsigned s=0;s<4;++s)
                if((current[step.cell].bits() & (1U<<s)) && a1_scalar_emission(step,s)!=0) return j;
        }
        return {};
    }
    const A1Problem& problem_;
    std::span<const A1World> worlds_;
    std::span<const std::size_t> family_;
    A1Domains support_;
    std::optional<CellId> last_cell_;
    std::optional<std::size_t> last_factor_;
};
} // namespace mcr::test
