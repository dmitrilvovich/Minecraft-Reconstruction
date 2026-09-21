#pragma once
#include "test.hpp"
#include "mcr/inference/a1_supports.hpp"
#include <algorithm>

namespace mcr::test {
using A1Ids=std::vector<std::uint32_t>;
inline A1Domains a1_supports(std::span<const A1World> worlds,std::span<const std::uint32_t> ids,std::size_t n) {
    A1Domains support(n,A1Domain(0));
    for(auto id:ids) for(CellId v=0;v<n;++v)
        support[v]=A1Domain(support[v].bits() | (1U<<code(worlds[id][v])));
    return support;
}
class A1ExhaustiveAudit final : public A1InferenceObserver {
public:
    A1ExhaustiveAudit(std::span<const A1World> worlds,std::span<const std::uint32_t> ids) : worlds_(worlds),ids_(ids) {}
    void begin(QueryKind,std::span<const A1Domain> assumptions) override {
        current_.assign(assumptions.begin(),assumptions.end());
        support_.assign(assumptions.size(),A1Domain(0));
        feasible_count_=0;
        for(auto id:ids_) if(contains(assumptions,worlds_[id])) {
            ++feasible_count_;
            for(CellId v=0;v<assumptions.size();++v)
                support_[v]=A1Domain(support_[v].bits() | (1U<<code(worlds_[id][v])));
        }
        ++contexts;
    }
    void prune(const BasicPruneEvent<A1Domain>& event) override {
        CHECK(event.cell<support_.size());
        CHECK(current_[event.cell]==event.before);
        CHECK(event.after!=event.before && (event.after & event.before)==event.after);
        if((event.before.bits() ^ event.after.bits()) & support_[event.cell].bits())
            throw std::runtime_error("unsound A1 prune at cell "+std::to_string(event.cell)+
                " before="+std::to_string(event.before.bits())+" after="+std::to_string(event.after.bits())+
                " oracle="+std::to_string(support_[event.cell].bits()));
        current_[event.cell]=event.after;
        ++steps;
        literal_deletions+=event.before.size()-event.after.size();
    }
    void contradiction(ContradictionReason,std::optional<std::size_t>) override {
        CHECK(feasible_count_==0); ++contradictions;
    }
    std::uint64_t contexts=0,steps=0,literal_deletions=0,contradictions=0;
private:
    std::span<const A1World> worlds_;
    std::span<const std::uint32_t> ids_;
    A1Domains current_,support_;
    std::size_t feasible_count_=0;
};
// Scalar reference for abstract constraints; does not call the automaton or emit().
inline bool a1_direct_accepts(const A1Problem& problem,const A1World& world) {
    for(const auto& factor:problem.constraints()) {
        unsigned label=0;
        for(const auto& step:factor.steps()) {
            const auto s=code(world[step.cell]);
            if(s==0 || (s==3 && !step.slab_hit)) continue;
            label=s==1 ? 1U : 2U; break;
        }
        if(label!=code(factor.target())) return false;
    }
    return true;
}
inline unsigned a1_freedom_mask(std::span<const std::uint32_t> ids,std::size_t n) {
    if(ids.empty() || ids.size()%4!=0) return 0;
    unsigned free_mask=0;
    for(CellId v=0;v<n;++v) {
        bool free=true;
        const unsigned shift=2*static_cast<unsigned>(v);
        for(auto id:ids) {
            const auto context=id & ~(3U<<shift);
            for(unsigned s=0;s<4;++s)
                if(!std::binary_search(ids.begin(),ids.end(),context | (s<<shift))) free=false;
            if(!free) break;
        }
        if(free) free_mask |= 1U<<v;
    }
    return free_mask;
}
} // namespace mcr::test
