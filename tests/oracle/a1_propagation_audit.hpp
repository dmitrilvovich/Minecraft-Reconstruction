#pragma once
#include "test.hpp"
#include "mcr/inference/a1_gac.hpp"
#include <algorithm>
#include <utility>

namespace mcr::test {
// Test-only exhaustive oracle. It uses neither production emissions nor automata.
struct A1PropagationOracle {
    A1Problem problem;
    std::vector<A1World> worlds;
    std::vector<std::vector<std::size_t>> factor_worlds;
    std::vector<std::size_t> family;

    explicit A1PropagationOracle(A1Problem p,std::span<const A1AabbReferenceRay> rays={})
        : problem(std::move(p)),factor_worlds(problem.constraints().size()) {
        CHECK(rays.empty() || rays.size()==problem.constraints().size());
        for(std::size_t id=0;id<a1_world_count(problem.cell_count());++id) {
            worlds.push_back(a1_world(problem.cell_count(),id));
            const auto& world=worlds.back(); bool accepted=true;
            for(std::size_t r=0;r<factor_worlds.size();++r) {
                const auto& factor=problem.constraints()[r];
                unsigned label=0;
                if(!rays.empty()) label=code(rays[r].sample(world));
                else for(const auto& step:factor.steps()) {
                    const auto s=code(world[step.cell]);
                    if(s==0 || (s==3 && !step.slab_hit)) continue;
                    label=s==1 ? 1U : 2U; break;
                }
                if(label==code(factor.target())) factor_worlds[r].push_back(id);
                else accepted=false;
            }
            CHECK(problem.accepts(world)==accepted);
            if(accepted) family.push_back(id);
        }
    }
    A1PropagationResult project(std::size_t r,const A1Domains& domains) const {
        A1Domains masks(domains.size(),A1Domain(0)); bool any=false;
        for(auto id:factor_worlds.at(r)) if(contains(domains,worlds[id])) {
            any=true;
            for(auto v:problem.constraints()[r].cells())
                masks[v]=A1Domain(masks[v].bits() | (1U<<code(worlds[id][v])));
        }
        return {any,std::move(masks)};
    }
    // Full scans and literal world enumeration, independent of the production queue.
    A1PropagationResult closure(A1Domains domains) const {
        for(auto d:domains) if(d.empty()) return {false,std::move(domains)};
        bool changed;
        do {
            changed=false;
            for(std::size_t r=0;r<factor_worlds.size();++r) {
                const auto local=project(r,domains);
                if(!local.consistent) return {false,std::move(domains)};
                for(auto v:problem.constraints()[r].cells()) {
                    const auto next=domains[v] & local.domains[v];
                    if(next!=domains[v]) { domains[v]=next; changed=true; }
                }
            }
        } while(changed);
        return {true,std::move(domains)};
    }
};

class A1PropagationAudit final : public A1InferenceObserver {
public:
    explicit A1PropagationAudit(const A1PropagationOracle& oracle) : oracle_(oracle) {}
    void begin(QueryKind kind,std::span<const A1Domain> assumptions) override {
        CHECK(kind==QueryKind::gac);
        current.assign(assumptions.begin(),assumptions.end());
        support_.assign(current.size(),A1Domain(0)); feasible_count_=0;
        for(auto id:oracle_.family) if(contains(assumptions,oracle_.worlds[id])) {
            ++feasible_count_;
            for(CellId v=0;v<current.size();++v)
                support_[v]=A1Domain(support_[v].bits() | (1U<<code(oracle_.worlds[id][v])));
        }
        ++contexts;
    }
    void prune(const A1PruneEvent& event) override {
        CHECK(event.cell<current.size() && event.before==current[event.cell]);
        CHECK(event.after!=event.before && (event.after & event.before)==event.after);
        CHECK(event.reason==PruneReason::ray_arc_support && event.factor.has_value());
        CHECK(*event.factor<oracle_.problem.constraints().size());
        const auto cells=oracle_.problem.constraints()[*event.factor].cells();
        CHECK(std::find(cells.begin(),cells.end(),event.cell)!=cells.end());
        // Global soundness under this call's original assumptions.
        CHECK(((event.before.bits() ^ event.after.bits()) & support_[event.cell].bits())==0);
        // Also audit the local justification, including globally inconsistent scenes.
        const auto local=oracle_.project(*event.factor,current);
        CHECK(local.consistent);
        CHECK((local.domains[event.cell] & event.after)==local.domains[event.cell]);
        current[event.cell]=event.after;
        ++steps; deletions+=event.before.size()-event.after.size();
    }
    void contradiction(ContradictionReason reason,std::optional<std::size_t> factor) override {
        CHECK(feasible_count_==0);
        if(reason==ContradictionReason::empty_domain) {
            CHECK(!factor);
            CHECK(std::any_of(current.begin(),current.end(),[](auto d){return d.empty();}));
        } else {
            CHECK(reason==ContradictionReason::no_ray_support && factor.has_value());
            CHECK(!oracle_.project(*factor,current).consistent);
        }
        ++contradictions;
    }
    A1Domains current;
    std::uint64_t contexts=0,steps=0,deletions=0,contradictions=0;
private:
    const A1PropagationOracle& oracle_;
    A1Domains support_;
    std::size_t feasible_count_=0;
};
} // namespace mcr::test
