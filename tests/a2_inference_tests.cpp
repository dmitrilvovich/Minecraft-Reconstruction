#include "test.hpp"
#include "fixtures/a2_scenes.hpp"
#include "mcr/inference/a1_supports.hpp"
#include <algorithm>
#include <random>

using namespace mcr;
namespace {
std::uint64_t cases=0,queries=0,witnesses=0,prunes=0,contradictions=0;
A2Domains supports(const std::vector<A2World>& family,std::size_t n) {
    A2Domains result(n,A2Domain(0));
    for(const auto& w:family) for(CellId v=0;v<n;++v)
        result[v]=A2Domain(result[v].bits() | (1U<<code(w[v])));
    return result;
}
class Audit final : public A2InferenceObserver {
public:
    explicit Audit(const std::vector<A2World>& worlds) : family(worlds) {}
    void begin(QueryKind,std::span<const A2Domain> assumptions) override {
        current.assign(assumptions.begin(),assumptions.end());
        std::vector<A2World> matching;
        for(const auto& w:family) if(contains(assumptions,w)) matching.push_back(w);
        nonempty=!matching.empty(); expected=supports(matching,assumptions.size());
    }
    void prune(const BasicPruneEvent<A2Domain>& event) override {
        CHECK(current.at(event.cell)==event.before);
        CHECK((event.after & event.before)==event.after);
        CHECK((event.after & expected.at(event.cell))==expected[event.cell]);
        current[event.cell]=event.after; ++prunes;
    }
    void contradiction(ContradictionReason,std::optional<std::size_t>) override {
        CHECK(!nonempty); ++contradictions;
    }
private:
    const std::vector<A2World>& family;
    A2Domains current,expected;
    bool nonempty=false;
};
void check_witness(const A2Problem& problem,const A2Domains& domains,const std::vector<A2World>& family,const A2World& w) {
    CHECK(contains(domains,w) && problem.accepts(w));
    CHECK(std::find(family.begin(),family.end(),w)!=family.end());
    ++witnesses;
}
void verify(const A2Problem& problem,const A2Domains& domains,const std::vector<A2World>& family) {
    std::vector<A2World> conditioned;
    for(const auto& w:family) if(contains(domains,w)) conditioned.push_back(w);
    const auto expected=supports(conditioned,domains.size());
    Audit audit(family);
    InferenceStats stats;
    const auto decision=exact_feasible(problem,domains,stats,&audit);
    CHECK(decision.feasible==!conditioned.empty());
    CHECK(decision.feasible==decision.witness.has_value());
    if(decision.witness) check_witness(problem,domains,conditioned,*decision.witness);
    const auto result=exact_supports(problem,domains,stats,&audit);
    CHECK(result.feasible==decision.feasible && result.supported==expected);
    if(result.feasible) {
        CHECK(!result.witnesses.empty());
        for(const auto& w:result.witnesses) check_witness(problem,domains,conditioned,w);
        CHECK(supports(result.witnesses,domains.size())==result.supported);
    } else CHECK(result.witnesses.empty());
    for(CellId v=0;v<domains.size();++v) for(auto s:a2_states) {
        const auto query=query_support(problem,domains,v,s,stats,&audit);
        CHECK(query.supported==expected[v].contains(s));
        CHECK(query.supported==query.witness.has_value());
        if(query.witness) {
            CHECK(query.witness->at(v)==s);
            check_witness(problem,domains,conditioned,*query.witness);
        }
        ++queries;
    }
    // Keep A1-specific counters honest; this solver never invokes its envelope.
    CHECK(stats.envelope_calls==0 && stats.envelope_advances==0 && stats.fixed_calls==0);
    ++cases;
}
std::vector<A2World> physical_family(const test::A2Fixture& fixture) {
    std::vector<A2AabbReferenceRay> reference;
    std::vector<A2TraversalRay> traversal;
    for(const auto& ray:fixture.rays) {
        reference.emplace_back(fixture.grid,ray,fixture.problem.palette());
        traversal.emplace_back(fixture.grid,ray,fixture.problem.palette());
    }
    std::vector<A2World> family;
    for(unsigned id=0;id<27;++id) {
        const auto w=fixture.world(id);
        bool accepts=true;
        for(std::size_t r=0;r<reference.size();++r) {
            const auto label=reference[r].sample(w);
            CHECK(label==traversal[r].sample(w));
            CHECK(fixture.problem.constraints()[r].accepts(w)==(label==fixture.targets[r]));
            accepts=accepts && label==fixture.targets[r];
        }
        CHECK(fixture.problem.accepts(w)==accepts);
        if(accepts) family.push_back(w);
    }
    return family;
}
// Independent direct first-hit evaluation; does not use transition/GAC/search.
std::vector<A2World> abstract_family(const A2Problem& problem) {
    std::vector<A2World> family;
    for(std::uint64_t id=0;id<a2_world_count(problem.cell_count());++id) {
        const auto w=a2_world(problem.cell_count(),id);
        bool accepts=true;
        for(const auto& f:problem.constraints()) {
            auto label=PixelLabel::background;
            for(const auto& step:f.steps()) {
                const auto s=w[step.cell];
                if(s==A2State::bottom_slab && step.bottom_hit) label=PixelLabel::oak;
                if(s==A2State::top_slab && step.top_hit)
                    label=problem.palette()==A2Palette::same_material ? PixelLabel::oak : PixelLabel::stone;
                if(label!=PixelLabel::background) break;
            }
            accepts=accepts && label==f.target();
        }
        if(accepts) family.push_back(w);
    }
    return family;
}
void fixtures() {
    for(auto palette:{A2Palette::split_material,A2Palette::same_material}) for(bool triangle:{false,true}) {
        const auto fixture=test::a2_fixture(triangle,palette);
        const auto family=physical_family(fixture);
        CHECK(family.size()==(triangle ? 0U : 2U));
        auto domains=fixture.domains();
        InferenceStats stats;
        const auto root=gac(fixture.problem,domains,stats);
        CHECK(root.consistent && root.domains==domains);
        stats={};
        const auto decision=exact_feasible(fixture.problem,domains,stats);
        CHECK(decision.feasible==!triangle && stats.branches>0);
        CHECK(stats.search_nodes==(triangle ? 4U : 3U));
        CHECK(stats.branches==(triangle ? 3U : 2U));
        const auto exact=exact_supports(fixture.problem,domains,stats);
        for(auto v:fixture.active) CHECK(exact.supported[v].bits()==(triangle ? 0U : 6U));
        if(!triangle) {
            CHECK(family[0][fixture.active[0]]!=family[0][fixture.active[1]]);
            CHECK(family[0][fixture.active[1]]==family[0][fixture.active[2]]);
            auto incompatible=family[0]; incompatible[fixture.active[0]]=incompatible[fixture.active[1]];
            CHECK(contains(exact.supported,incompatible) && !fixture.problem.accepts(incompatible));
        }
        for(unsigned masks=0;masks<512;++masks) {
            for(unsigned j=0;j<3;++j) domains[fixture.active[j]]=A2Domain((masks>>(3*j)) & 7U);
            verify(fixture.problem,domains,family);
        }
        std::vector<A2Constraint> reordered(fixture.problem.constraints().begin(),fixture.problem.constraints().end());
        std::reverse(reordered.begin(),reordered.end());
        reordered.push_back(reordered.front());
        verify(A2Problem(8,reordered,palette),fixture.domains(),family);
        std::cout << (palette==A2Palette::split_material ? "split" : "same") << " material "
                  << (triangle ? "triangle: 0 worlds, 4 nodes / 3 branches" : "path: 2 worlds, 3 nodes / 2 branches")
                  << "; GAC retains all three active domains\n";
    }
}
void edge_cases() {
    InferenceStats stats;
    const A2Problem empty(0,{});
    verify(empty,{},{{}});
    verify(A2Problem(0,{{{},PixelLabel::oak}}),{},{});
    verify(A2Problem(0,{{{},PixelLabel::background}}),{},{{}});
    verify(A2Problem(3,{}),A2Domains(3),abstract_family(A2Problem(3,{})));
    verify(A2Problem(1,{}),{A2Domain(0)},abstract_family(A2Problem(1,{})));
    const A2Problem contradiction(1,{{{{0,true,false}},PixelLabel::oak},{{{0,false,true}},PixelLabel::stone}});
    verify(contradiction,A2Domains(1),{});
    const A2Problem same(1,{{{{0,true,true}},PixelLabel::oak,A2Palette::same_material}},A2Palette::same_material);
    const A2Problem split(1,{{{{0,true,true}},PixelLabel::oak}});
    CHECK(exact_supports(same,A2Domains(1),stats).supported[0].bits()==6);
    CHECK(exact_supports(split,A2Domains(1),stats).supported[0].bits()==2);
    verify(same,A2Domains(1),abstract_family(same));
    verify(split,A2Domains(1),abstract_family(split));
    // A hidden tail with incomparable shapes must not drive branching.
    const A2Problem hidden(2,{{{{0,true,true},{1,true,false}},PixelLabel::oak,A2Palette::same_material}},A2Palette::same_material);
    stats={};
    CHECK(exact_feasible(hidden,{A2Domain(6),A2Domain()},stats).feasible);
    CHECK(stats.branches==0);
    verify(hidden,{A2Domain(6),A2Domain()},abstract_family(hidden));
    // Two independent paths force more than one search level. Cell zero is
    // unconstrained and must not be selected before either active path.
    std::vector<A2Constraint> paths;
    for(CellId base:{1U,4U}) for(CellId other:{base+1,base+2}) {
        paths.emplace_back(std::vector<A2RayCell>{{base,true,false},{other,true,false}},PixelLabel::oak);
        paths.emplace_back(std::vector<A2RayCell>{{base,false,true},{other,false,true}},PixelLabel::stone);
    }
    const A2Problem disconnected(7,paths);
    stats={};
    CHECK(exact_feasible(disconnected,A2Domains(7),stats).feasible);
    CHECK(stats.search_nodes==5 && stats.branches==4);
    verify(disconnected,A2Domains(7),abstract_family(disconnected));
    check_throws<std::invalid_argument>([]{ (void)A2Constraint({{0,true,false},{0,false,true}},PixelLabel::oak); });
    check_throws<std::invalid_argument>([]{ (void)A2Constraint({},static_cast<PixelLabel>(3)); });
    check_throws<std::invalid_argument>([]{ (void)A2Problem(0,{{{{0,true,false}},PixelLabel::oak}}); });
    check_throws<std::invalid_argument>([&]{ (void)A2Problem(1,{same.constraints()[0]}); });
    check_throws<std::invalid_argument>([&]{ (void)exact_feasible(empty,A2Domains(1),stats); });
    check_throws<std::invalid_argument>([&]{ (void)query_support(split,A2Domains(1),0,static_cast<A2State>(3),stats); });
    check_throws<std::out_of_range>([&]{ (void)query_support(split,A2Domains(1),1,A2State::air,stats); });
    check_throws<std::invalid_argument>([&]{ (void)empty.accepts(A2World{A2State::air}); });
    // An observer is an independent checker: deliberately wrong pruning must fail.
    const auto family=abstract_family(same);
    Audit audit(family); audit.begin(QueryKind::gac,A2Domains(1));
    check_throws<std::runtime_error>([&]{ audit.prune({0,A2Domain(),A2Domain(2),PruneReason::unsupported_state,{}}); });
    check_throws<std::runtime_error>([&]{ audit.contradiction(ContradictionReason::no_ray_support,{}); });
}
void mixed_factors() {
    std::mt19937 rng(20260923);
    for(auto palette:{A2Palette::split_material,A2Palette::same_material}) for(unsigned i=0;i<160;++i) {
        std::vector<A2Constraint> factors;
        for(unsigned r=0;r<i%7;++r) {
            std::vector<CellId> cells{0,1,2,3}; std::shuffle(cells.begin(),cells.end(),rng);
            cells.resize(rng()%5);
            std::vector<A2RayCell> steps;
            for(auto v:cells) steps.push_back({v,(rng()%2)!=0,(rng()%2)!=0});
            factors.emplace_back(steps,static_cast<PixelLabel>(rng()%3),palette);
        }
        const A2Problem problem(4,factors,palette);
        A2Domains domains(4);
        for(auto& d:domains) d=A2Domain(static_cast<unsigned>(1+rng()%7));
        verify(problem,domains,abstract_family(problem));
    }
}
void a1_regression() {
    // The accepted nested kernel remains independently callable with its own
    // four-state domains after linking the A2 overloads into the same library.
    const A1Problem problem(2,{{{{0,true},{1,true}},PixelLabel::oak},
                              {{{0,false},{1,false}},PixelLabel::background}});
    InferenceStats stats;
    const auto result=exact_supports(problem,A1Domains(2),stats);
    CHECK(result.feasible && result.supported==A1Domains({A1Domain(9),A1Domain(9)}));
    for(const auto& w:result.witnesses) CHECK(problem.accepts(w));
    CHECK(!query_support(problem,A1Domains(2),0,A1State::stone,stats).supported);
    const auto air=query_support(problem,A1Domains(2),0,A1State::air,stats);
    CHECK(air.supported && air.witness->at(1)==A1State::oak_slab);
    CHECK(!envelope_feasible(problem,A1Domains(2,A1Domain(1)),stats).feasible);
    CHECK(stats.branches==0 && stats.search_nodes==0);
}
}
int main() { return run_tests([] {
    fixtures(); edge_cases(); mixed_factors(); a1_regression();
    std::cout << cases << " exact projection/feasibility cases; " << queries << " direct support queries; "
              << witnesses << " verified witnesses; " << prunes << " audited reductions; "
              << contradictions << " audited propagation contradictions\n";
}); }
