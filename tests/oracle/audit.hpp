#pragma once
#include "mcr/inference/observer.hpp"
#include "test.hpp"

namespace mcr::test {
inline Domains supports(std::span<const World> family,std::size_t n) {
    Domains result(n,Domain(0));
    for(const auto& w:family) for(CellId v=0;v<n;++v)
        result[v]=Domain(result[v].bits() | (1U<<code(w[v])));
    return result;
}
class ExhaustiveAudit final : public InferenceObserver {
public:
    explicit ExhaustiveAudit(std::span<const World> family) : family_(family) {}
    void begin(QueryKind,std::span<const Domain> assumptions) override {
        support_.assign(assumptions.size(),Domain(0));
        current_.assign(assumptions.begin(),assumptions.end());
        feasible_count_=0;
        for(const auto& world:family_) if(contains(assumptions,world)) {
            ++feasible_count_;
            for(CellId v=0;v<world.size();++v)
                support_[v]=Domain(support_[v].bits() | (1U<<code(world[v])));
        }
        ++queries;
    }
    void prune(const PruneEvent& event) override {
        CHECK(event.cell<support_.size());
        CHECK(current_[event.cell]==event.before);
        CHECK((event.after & event.before)==event.after);
        CHECK(((event.before.bits() ^ event.after.bits()) & support_[event.cell].bits())==0);
        current_[event.cell]=event.after;
        ++steps;
        literal_deletions+=event.before.size()-event.after.size();
    }
    void contradiction(ContradictionReason,std::optional<std::size_t>) override {
        CHECK(feasible_count_==0);
        ++contradictions;
    }
    std::uint64_t steps=0,literal_deletions=0,queries=0,contradictions=0;
private:
    std::span<const World> family_;
    Domains support_,current_;
    std::size_t feasible_count_=0;
};

// Independent scalar observation predicate for abstract ray fixtures.
inline bool direct_accepts(const Problem& problem,const World& world) {
    for(const auto& f:problem.constraints()) {
        unsigned label=0;
        for(auto v:f.cells()) if(world[v]!=A0State::air) { label=code(world[v]); break; }
        if(label!=code(f.target())) return false;
    }
    return true;
}
inline std::vector<World> enumerate(const Problem& problem) {
    std::vector<World> family;
    for(std::uint64_t id=0;id<a0_world_count(problem.cell_count());++id) {
        auto world=a0_world(problem.cell_count(),id);
        if(direct_accepts(problem,world)) family.push_back(std::move(world));
    }
    return family;
}
inline unsigned freedom_mask(std::span<const World> family,std::size_t n) {
    if(family.empty()) return 0;
    std::vector<bool> present(static_cast<std::size_t>(a0_world_count(n)),false);
    for(const auto& w:family) present.at(static_cast<std::size_t>(a0_world_id(w)))=true;
    unsigned mask=0;
    for(CellId v=0;v<n;++v) {
        bool free=true;
        for(const auto& world:family) {
            auto altered=world;
            for(auto s:a0_states) {
                altered[v]=s;
                if(!present[static_cast<std::size_t>(a0_world_id(altered))]) free=false;
            }
            if(!free) break;
        }
        if(free) mask |= 1U<<v;
    }
    return mask;
}
} // namespace mcr::test

