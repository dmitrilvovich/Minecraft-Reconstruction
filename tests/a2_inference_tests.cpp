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
    explicit Audit(const std::vector<A2World>& worlds,const A2Problem* p=nullptr) : family(worlds),problem(p) {}
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
        if(problem && event.factor) {
            CHECK(*event.factor<problem->constraints().size());
            const auto cells=problem->constraints()[*event.factor].cells();
            CHECK(std::find(cells.begin(),cells.end(),event.cell)!=cells.end());
        }
        current[event.cell]=event.after; ++prunes;
    }
    void contradiction(ContradictionReason,std::optional<std::size_t> factor) override {
        CHECK(!nonempty); ++contradictions;
        if(problem && factor) {
            CHECK(*factor<problem->constraints().size());
            CHECK(!factor_supports(problem->constraints()[*factor],current).feasible);
        }
    }
private:
    const std::vector<A2World>& family;
    const A2Problem* problem;
    A2Domains current,expected;
    bool nonempty=false;
};
void check_witness(const A2Problem& problem,const A2Domains& domains,const std::vector<A2World>& family,const A2World& w) {
    CHECK(contains(domains,w) && problem.accepts(w));
    CHECK(std::find(family.begin(),family.end(),w)!=family.end());
    ++witnesses;
}
void verify(const A2Problem& problem,const A2Domains& domains,const std::vector<A2World>& family,A2SearchOptions options={}) {
    std::vector<A2World> conditioned;
    for(const auto& w:family) if(contains(domains,w)) conditioned.push_back(w);
    const auto expected=supports(conditioned,domains.size());
    Audit audit(family,&problem);
    InferenceStats stats;
    const auto decision=exact_feasible(problem,domains,stats,&audit,options);
    CHECK(decision.feasible==!conditioned.empty());
    CHECK(decision.feasible==decision.witness.has_value());
    if(decision.witness) check_witness(problem,domains,conditioned,*decision.witness);
    const auto result=exact_supports(problem,domains,stats,&audit,options);
    CHECK(result.feasible==decision.feasible && result.supported==expected);
    if(result.feasible) {
        CHECK(!result.witnesses.empty());
        for(const auto& w:result.witnesses) check_witness(problem,domains,conditioned,w);
        CHECK(supports(result.witnesses,domains.size())==result.supported);
    } else CHECK(result.witnesses.empty());
    for(CellId v=0;v<domains.size();++v) for(auto s:a2_states) {
        const auto query=query_support(problem,domains,v,s,stats,&audit,options);
        CHECK(query.supported==expected[v].contains(s));
        CHECK(query.supported==query.witness.has_value());
        if(query.witness) {
            CHECK(query.witness->at(v)==s);
            check_witness(problem,domains,conditioned,*query.witness);
        }
        ++queries;
    }
    // Keep A1-specific counters honest; this solver never invokes its envelope.
    CHECK(stats.envelope_calls==0 && stats.envelope_advances==0);
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
    CHECK(stats.search_nodes==7 && stats.branches==4); // includes both component entry nodes
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
struct Measurement { InferenceStats work; A2SearchDiagnostics diagnostics; };
std::array<Measurement,4> compare_modes(const std::string& name,const A2Problem& problem,const A2Domains& domains) {
    const auto family=abstract_family(problem);
    std::array<Measurement,4> measurements{};
    for(unsigned mode=0;mode<4;++mode) {
        auto& m=measurements[mode];
        const A2SearchOptions options{(mode & 1U)!=0,(mode & 2U)!=0,&m.diagnostics};
        const auto decision=exact_feasible(problem,domains,m.work,nullptr,options);
        const bool expected=std::any_of(family.begin(),family.end(),[&](const auto& w){return contains(domains,w);});
        CHECK(decision.feasible==expected);
        if(decision.witness) check_witness(problem,domains,family,*decision.witness);
        // Full masks, every conditioned query, witness coverage and pruning must
        // agree with independent enumeration in every optimization configuration.
        verify(problem,domains,family,{options.decompose,options.fixed_hit});
    }
    CHECK(measurements[0].diagnostics.decompositions==0 && measurements[0].work.fixed_calls==0);
    CHECK(measurements[1].work.fixed_calls==0 && measurements[2].diagnostics.decompositions==0);
    std::cout << name << " branches [baseline, decomposition, fixed-hit, both] = [";
    for(unsigned i=0;i<4;++i) std::cout << (i ? ", " : "") << measurements[i].work.branches;
    std::cout << "]; optimized nodes=" << measurements[3].work.search_nodes
              << ", splits=" << measurements[3].diagnostics.decompositions
              << ", component solves=" << measurements[3].diagnostics.component_solves
              << ", fixed calls=" << measurements[3].work.fixed_calls << '\n';
    return measurements;
}
void opposite_pair(std::vector<A2Constraint>& factors,CellId a,CellId b,A2Palette palette) {
    factors.emplace_back(std::vector<A2RayCell>{{a,true,false},{b,true,false}},PixelLabel::oak,palette);
    factors.emplace_back(std::vector<A2RayCell>{{a,false,true},{b,false,true}},material(A2State::top_slab,palette),palette);
}
void graph(std::vector<A2Constraint>& factors,CellId base,bool triangle,A2Palette palette) {
    opposite_pair(factors,base,base+1,palette); opposite_pair(factors,base,base+2,palette);
    if(triangle) opposite_pair(factors,base+1,base+2,palette);
}
void optimizations() {
    for(auto palette:{A2Palette::split_material,A2Palette::same_material}) {
        const std::string label=palette==A2Palette::split_material ? "split: " : "same: ";
        const A2Problem fixed(2,{{{{0,true,true},{1,true,true}},PixelLabel::oak,palette}},palette);
        const auto fixed_run=compare_modes(label+"fixed-hit ray",fixed,A2Domains(2));
        CHECK(fixed_run[0].work.branches==1 && fixed_run[3].work.branches==0);
        CHECK(fixed_run[3].work.fixed_calls==1 && fixed_run[3].diagnostics.fixed_hit_rejections==0);

        std::vector<A2Constraint> factors;
        graph(factors,0,false,palette); graph(factors,3,false,palette);
        const auto two_paths=compare_modes(label+"two paths",A2Problem(6,factors,palette),A2Domains(6));
        CHECK(two_paths[3].diagnostics.decompositions==1 && two_paths[3].diagnostics.component_solves==2);
        CHECK(two_paths[0].work.branches==4 && two_paths[3].work.branches==4);
        // Interleave factors so a component observer must translate noncontiguous IDs.
        std::rotate(factors.begin()+1,factors.begin()+4,factors.end());
        opposite_pair(factors,4,5,palette);
        const auto impossible=compare_modes(label+"path plus triangle",A2Problem(6,factors,palette),A2Domains(6));
        CHECK(impossible[0].work.branches==9 && impossible[3].work.branches==5);
        CHECK(impossible[3].diagnostics.decompositions==1);
        CHECK(impossible[3].diagnostics.component_solves==2);

        factors.clear(); graph(factors,0,true,palette); graph(factors,3,false,palette);
        const auto early=compare_modes(label+"triangle before path",A2Problem(6,factors,palette),A2Domains(6));
        CHECK(early[3].diagnostics.component_solves==1); // never solve a sibling after failure

        factors={A2Constraint({{0,true,true},{1,true,true}},PixelLabel::oak,palette)};
        graph(factors,2,false,palette);
        const auto mixed=compare_modes(label+"fixed region plus path",A2Problem(5,factors,palette),A2Domains(5));
        CHECK(mixed[3].work.fixed_calls==1 && mixed[3].diagnostics.component_solves==2);
        CHECK(mixed[3].work.branches<mixed[0].work.branches);

        // A raw shared variable whose allowed states both miss cannot couple regions.
        factors.clear(); graph(factors,1,false,palette); graph(factors,4,false,palette);
        std::vector<A2Constraint> with_context;
        for(const auto& f:factors) {
            std::vector<A2RayCell> steps{{0,false,true}};
            steps.insert(steps.end(),f.steps().begin(),f.steps().end());
            with_context.emplace_back(steps,f.target(),palette);
        }
        A2Domains domains(7); domains[0]=A2Domain(3);
        const auto shared=compare_modes(label+"shared invariant context",A2Problem(7,with_context,palette),domains);
        CHECK(shared[3].diagnostics.decompositions==1 && shared[3].diagnostics.component_solves==2);

        // A ray spanning both paths is entailed by a required correct-color end.
        factors.clear(); graph(factors,0,false,palette); graph(factors,3,false,palette);
        factors.emplace_back(std::vector<A2RayCell>{{0,true,false},{3,true,false},{6,true,true}},PixelLabel::oak,palette);
        domains=A2Domains(7); domains[6]=A2Domain(palette==A2Palette::same_material ? 6 : 2);
        const auto entailed=compare_modes(label+"entailed bridge",A2Problem(7,factors,palette),domains);
        CHECK(entailed[3].diagnostics.decompositions==1 && entailed[3].diagnostics.component_solves==2);

        // Without that guarantee, the same two paths are truly coupled. An oak
        // label on each slab does not make lower/upper hit geometry equivalent.
        factors.pop_back();
        factors.emplace_back(std::vector<A2RayCell>{{0,true,false},{3,true,false}},PixelLabel::oak,palette);
        const auto coupled=compare_modes(label+"non-entailed bridge",A2Problem(6,factors,palette),A2Domains(6));
        CHECK(coupled[3].diagnostics.decompositions==0 && coupled[3].diagnostics.fixed_hit_rejections>0);
        CHECK(coupled[3].work.branches>0);

        // Conditioning a guard must recompute the graph; three edges become
        // independent only when the guard is guaranteed to cover their bridge.
        factors.clear();
        for(CellId a:{0U,2U,4U}) opposite_pair(factors,a,a+1,palette);
        factors.emplace_back(std::vector<A2RayCell>{{6,true,true},{0,true,false},{2,true,false},{4,true,false}},PixelLabel::oak,palette);
        const A2Problem guarded(7,factors,palette);
        domains=A2Domains(7); domains[6]=A2Domain(3);
        InferenceStats stats; A2SearchDiagnostics diagnostics;
        const auto query=query_support(guarded,domains,6,A2State::bottom_slab,stats,nullptr,{true,true,&diagnostics});
        CHECK(query.supported && query.witness->at(6)==A2State::bottom_slab && guarded.accepts(*query.witness));
        CHECK(diagnostics.decompositions==1 && diagnostics.component_solves==3);
        compare_modes(label+"conditional guard",guarded,domains);

        // A six-cell region is initially coupled by a lower-ray bridge, while
        // another edge is independent. Inside that region, one branch entails
        // the bridge and exposes two new components. Exercise observer mappings
        // through two component levels, with interleaved original factor IDs.
        factors.clear();
        for(CellId a:{0U,2U,4U,6U}) opposite_pair(factors,a,a+1,palette);
        factors.emplace_back(std::vector<A2RayCell>{{0,true,false},{2,true,false},{4,true,false}},PixelLabel::oak,palette);
        std::rotate(factors.begin()+1,factors.begin()+5,factors.end());
        const auto nested=compare_modes(label+"nested component split",A2Problem(8,factors,palette),A2Domains(8));
        CHECK(nested[3].diagnostics.decompositions==2 && nested[3].diagnostics.component_solves==4);
    }
    // A guaranteed opaque middle cell hides a raw cross-component reference.
    // Making it optional must restore that dependency. The wrong-color state
    // at cell 1 keeps this factor non-entailed despite its guaranteed oak end.
    const auto palette=A2Palette::split_material;
    std::vector<A2Constraint> factors{
        A2Constraint({{0,true,true},{1,true,true},{2,true,true},{3,true,false}},PixelLabel::oak,palette)};
    graph(factors,3,false,palette);
    const A2Problem hidden(6,factors,palette);
    A2Domains domains(6); domains[0]=A2Domain(3); domains[2]=A2Domain(2);
    const auto opaque=compare_modes("split: guaranteed hit hides bridge",hidden,domains);
    CHECK(opaque[3].diagnostics.decompositions==1);
    domains[2]=A2Domain(3);
    const auto optional=compare_modes("split: optional hit preserves bridge",hidden,domains);
    CHECK(optional[3].diagnostics.decompositions==0);
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
    fixtures(); edge_cases(); mixed_factors(); optimizations(); a1_regression();
    std::cout << cases << " exact projection/feasibility cases; " << queries << " direct support queries; "
              << witnesses << " verified witnesses; " << prunes << " audited reductions; "
              << contradictions << " audited propagation contradictions\n";
}); }
