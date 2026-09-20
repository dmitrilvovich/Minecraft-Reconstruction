#pragma once
#include "oracle/a1_feasibility_audit.hpp"

namespace mcr::test {
// Compose the validated conditioned audit with a separate global projection audit.
class A1ProjectionAudit final : public A1InferenceObserver {
public:
    A1ProjectionAudit(const A1Problem& problem,std::span<const A1World> worlds,
                      std::span<const std::size_t> family,const A1Domains& initial)
        : kernel(problem,worlds,family),expected(initial.size(),A1Domain(0)),initial_(initial) {
        for(auto id:family) {
            bool allowed=true;
            for(CellId v=0;v<initial.size();++v)
                allowed=allowed && (initial[v].bits() & (1U<<code(worlds[id][v])))!=0;
            if(!allowed) continue;
            feasible_family.push_back(id);
            for(CellId v=0;v<initial.size();++v)
                expected[v]=A1Domain(expected[v].bits() | (1U<<code(worlds[id][v])));
        }
    }
    void begin(QueryKind kind,std::span<const A1Domain> assumptions) override {
        CHECK(!projecting_);
        const A1Domains domains(assumptions.begin(),assumptions.end());
        if(kind==QueryKind::nested_envelope) {
            if(kernel.contexts==0) CHECK(domains==initial_);
            else {
                capture_root();
                CHECK(domains.size()==root.size()); unsigned changed=0;
                for(CellId v=0;v<root.size();++v) if(domains[v]!=root[v]) {
                    CHECK(domains[v].size()==1 && (domains[v] & root[v])==domains[v]); ++changed;
                }
                CHECK(changed==1); // exactly one singleton assumption, with no leaked query pruning
            }
            kernel.begin(kind,assumptions);
        } else {
            CHECK(kind==QueryKind::support_projection && kernel.contexts>0 && !feasible_family.empty());
            capture_root(); CHECK(domains==root);
            current=domains; projecting_=true; ++projection_contexts;
        }
    }
    void prune(const A1PruneEvent& event) override {
        if(!projecting_) { kernel.prune(event); return; }
        CHECK(event.reason==PruneReason::unsupported_state && !event.factor);
        CHECK(event.cell<current.size() && event.before==current[event.cell]);
        CHECK(event.after!=event.before && (event.after & event.before)==event.after);
        CHECK(event.after==expected[event.cell]); // exact global marginal, under original assumptions
        CHECK(((event.before.bits() ^ event.after.bits()) & expected[event.cell].bits())==0);
        current[event.cell]=event.after; ++projection_prunes;
        projection_deletions+=event.before.size()-event.after.size();
    }
    void contradiction(ContradictionReason reason,std::optional<std::size_t> factor) override {
        CHECK(!projecting_); kernel.contradiction(reason,factor);
    }
    A1FeasibilityAudit kernel;
    A1Domains expected,root,current;
    std::vector<std::size_t> feasible_family;
    std::uint64_t projection_contexts=0,projection_prunes=0,projection_deletions=0;
private:
    void capture_root() {
        if(root_captured_) return;
        root=kernel.current; root_captured_=true;
        CHECK(root.size()==initial_.size());
        for(CellId v=0;v<root.size();++v) {
            CHECK((root[v] & initial_[v])==root[v]);
            CHECK((root[v] & expected[v])==expected[v]);
        }
    }
    A1Domains initial_;
    bool projecting_=false,root_captured_=false;
};
} // namespace mcr::test
