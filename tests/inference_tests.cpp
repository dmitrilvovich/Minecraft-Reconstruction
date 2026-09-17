#include "test.hpp"
#include "oracle/audit.hpp"
#include "mcr/inference/a0_solver.hpp"
#include <algorithm>
#include <random>

using namespace mcr;
void verify(const Problem& problem,const Domains& domains) {
    const auto family=test::enumerate(problem);
    std::vector<World> conditioned;
    for(const auto& w:family) if(contains(domains,w)) conditioned.push_back(w);
    test::ExhaustiveAudit audit(family);
    InferenceStats stats;
    const auto result=exact_supports(problem,domains,stats,&audit);
    CHECK(result.feasible==!conditioned.empty());
    InferenceStats plain_stats;
    const auto plain=exact_supports(problem,domains,plain_stats);
    CHECK(result.feasible==plain.feasible && result.supported==plain.supported);
    const auto decision=fixed_geometry(problem,domains,plain_stats,&audit);
    CHECK(decision.feasible==result.feasible);
    if(result.feasible) {
        CHECK(result.supported==test::supports(conditioned,domains.size()));
        CHECK(contains(domains,*result.witness));
        CHECK(std::find(conditioned.begin(),conditioned.end(),*result.witness)!=conditioned.end());
        // Every literal also agrees with an independent complete feasibility query.
        for(CellId v=0;v<domains.size();++v) for(auto s:a0_states) {
            auto probe=domains;
            probe[v]=probe[v] & Domain::singleton(s);
            const auto supported=fixed_geometry(problem,probe,plain_stats,&audit).feasible;
            CHECK(supported==result.supported[v].contains(s));
        }
    } else CHECK(!result.witness && result.supported.empty());
}
int main() { return run_tests([] {
    const Problem pair(2,{{{0,1},PixelLabel::stone}});
    const auto family=test::enumerate(pair);
    CHECK(family.size()==4);
    CHECK(test::supports(family,2)==Domains({Domain(3),Domain(7)}));
    CHECK(test::freedom_mask(family,2)==0); // full marginal at B is correlated
    std::vector<World> front_stone;
    for(const auto& w:family) if(w[0]==A0State::stone) front_stone.push_back(w);
    CHECK(test::freedom_mask(front_stone,2)==2);
    test::ExhaustiveAudit broken_pruner(family);
    broken_pruner.begin(QueryKind::gac,Domains(2));
    check_throws<std::runtime_error>([&]{ broken_pruner.prune({1,Domain(7),Domain(3),PruneReason::ray_arc_support,0}); });
    check_throws<std::runtime_error>([&]{ broken_pruner.contradiction(ContradictionReason::no_ray_support,0); });
    for(unsigned a=0;a<8;++a) for(unsigned b=0;b<8;++b) verify(pair,{Domain(a),Domain(b)});
    verify(Problem(8,{}),Domains(8));
    verify(Problem(0,{}),{});
    verify(Problem(8,{{{},PixelLabel::stone}}),Domains(8));
    verify(Problem(8,{{{},PixelLabel::background}}),Domains(8));
    verify(Problem(2,{{{0,1},PixelLabel::stone},{{0,1},PixelLabel::oak}}),Domains(2));
    verify(Problem(2,{{{0,1},PixelLabel::stone},{{0,1},PixelLabel::stone}}),Domains(2));
    verify(Problem(2,{{{1,0},PixelLabel::oak},{{0,1},PixelLabel::stone}}),Domains(2));
    // Bounded randomized abstract constraints exercise queue interactions and
    // restricted domains independently of the physical camera corpus.
    std::mt19937 random(20260917);
    for(unsigned i=0;i<1500;++i) {
        std::vector<FirstHitConstraint> constraints;
        for(unsigned r=0;r<i%7;++r) {
            std::vector<CellId> cells{0,1,2,3};
            std::shuffle(cells.begin(),cells.end(),random);
            cells.resize(random()%5);
            constraints.emplace_back(cells,static_cast<PixelLabel>(random()%3));
        }
        Domains domains(4);
        for(auto& d:domains) d=Domain(1+random()%7);
        verify(Problem(4,constraints),domains);
        std::reverse(constraints.begin(),constraints.end());
        verify(Problem(4,constraints),domains);
    }
    std::cout << "A0 feasibility, every literal, restricted domains, contradictions and audit fault injection checked\n";
}); }
