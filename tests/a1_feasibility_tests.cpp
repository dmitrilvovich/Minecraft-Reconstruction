#include "fixtures/a1_problems.hpp"
#include "oracle/a1_propagation_audit.hpp"
#include "oracle/a1_feasibility_audit.hpp"
#include <array>
#include <random>

using namespace mcr;
struct Counts {
    std::uint64_t calls=0,feasible=0,infeasible=0,witnesses=0,steps=0,deletions=0,advances=0,updates=0;
};
A1FeasibilityResult verify(const A1Problem& problem,std::span<const A1World> worlds,
                           std::span<const std::size_t> family,const A1Domains& initial,Counts& counts) {
    const auto before=initial;
    test::A1FeasibilityAudit audit(problem,worlds,family);
    InferenceStats stats,plain_stats;
    const auto result=envelope_feasible(problem,initial,stats,&audit);
    const auto plain=envelope_feasible(problem,initial,plain_stats);
    CHECK(initial==before && result.feasible==!audit.conditioned.empty());
    CHECK(result.feasible==plain.feasible && result.domains==plain.domains && result.witness==plain.witness);
    CHECK(result.domains==audit.current && audit.contexts==1 && stats.envelope_calls==1);
    CHECK(audit.contradictions==static_cast<unsigned>(!result.feasible));
    CHECK(stats.deletions==audit.deletions && stats.deletions==plain_stats.deletions);
    CHECK(stats.factor_updates==plain_stats.factor_updates && stats.envelope_advances==plain_stats.envelope_advances);
    for(const auto& measured:{stats,plain_stats}) {
        CHECK(measured.branches==0 && measured.search_nodes==0 && measured.literal_queries==0);
        CHECK(measured.gac_calls==0 && measured.fixed_calls==0);
    }
    std::size_t total_steps=0,removed=0,update_bound=problem.constraints().size();
    for(const auto& factor:problem.constraints()) total_steps+=factor.steps().size();
    for(CellId v=0;v<initial.size();++v) {
        CHECK((result.domains[v] & initial[v])==result.domains[v]);
        const auto deleted=initial[v].size()-result.domains[v].size();
        removed+=deleted; update_bound+=deleted*problem.incident(v).size();
    }
    CHECK(stats.deletions==removed && stats.envelope_advances<=total_steps);
    CHECK(stats.factor_updates<=update_bound); // finite deletions bound all queue work
    if(result.feasible) {
        CHECK(result.witness && contains(initial,*result.witness) && contains(result.domains,*result.witness));
        const auto id=static_cast<std::size_t>(a1_world_id(*result.witness));
        CHECK(std::find(audit.conditioned.begin(),audit.conditioned.end(),id)!=audit.conditioned.end());
        // Independently check the specified maximal-geometry/tie policy.
        constexpr std::array<unsigned,4> rank{0,2,2,1};
        for(CellId v=0;v<initial.size();++v) {
            unsigned selected=4;
            for(unsigned s=0;s<4;++s) if(result.domains[v].bits() & (1U<<s))
                if(selected==4 || rank[s]>rank[selected]) selected=s;
            CHECK(code((*result.witness)[v])==selected);
        }
        ++counts.feasible; ++counts.witnesses;
    } else { CHECK(!result.witness); ++counts.infeasible; }
    ++counts.calls; counts.steps+=audit.steps; counts.deletions+=stats.deletions;
    counts.advances+=stats.envelope_advances; counts.updates+=stats.factor_updates;
    return result;
}
A1FeasibilityResult verify(const test::A1PropagationOracle& oracle,const A1Domains& initial,Counts& counts) {
    return verify(oracle.problem,oracle.worlds,oracle.family,initial,counts);
}
void corpus(const std::vector<test::Fixture>& scenes,Counts& counts) {
    for(const auto& scene:scenes) {
        std::vector<std::size_t> order{0,1,2};
        do {
            std::vector<A1Constraint> factors;
            for(auto r:order) factors.push_back(scene.problem.constraints()[r]);
            const test::A1PropagationOracle oracle(A1Problem(3,std::move(factors)));
            for(unsigned encoded=0;encoded<4096;++encoded) verify(oracle,test::domains_for(encoded),counts);
        } while(std::next_permutation(order.begin(),order.end()));
        std::cout << scene.name << ": exhaustive feasibility, pruning and witnesses for all 4096 masks and 6 orders\n";
    }
}
void python_agreement(const std::vector<test::Fixture>& scenes,const std::filesystem::path& directory) {
    std::ifstream in(directory/"feasibility.bin",std::ios::binary); CHECK(in.good());
    const auto byte=[&]() { const auto b=in.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); };
    std::string magic; for(unsigned i=0;i<8;++i) magic+=static_cast<char>(byte());
    CHECK(magic=="MCRA1E1\n");
    unsigned count=0; for(unsigned i=0;i<4;++i) count|=byte()<<(8*i); CHECK(count==10125);
    for(const auto& scene:scenes) {
        const test::A1PropagationOracle oracle(scene.problem);
        for(unsigned encoded=0;encoded<3375;++encoded) {
            A1Domains domains; unsigned digits=encoded;
            for(unsigned v=0;v<3;++v) { domains.emplace_back(1+digits%15); digits/=15; }
            InferenceStats stats; const auto result=envelope_feasible(scene.problem,domains,stats);
            const bool feasible=byte()!=0; A1World witness;
            for(unsigned v=0;v<3;++v) witness.push_back(static_cast<A1State>(byte()));
            CHECK(result.feasible==feasible && stats.branches==0 && stats.search_nodes==0);
            if(feasible) {
                CHECK(result.witness && *result.witness==witness && contains(domains,witness));
                const auto id=static_cast<std::size_t>(a1_world_id(witness));
                CHECK(std::find(oracle.family.begin(),oracle.family.end(),id)!=oracle.family.end());
            } else CHECK(!result.witness);
        }
    }
    CHECK(in.get()==std::char_traits<char>::eof());
    std::cout << count << " decisions and deterministic witnesses agree with frozen Python; zero search\n";
}
void named_cases(Counts& counts) {
    const auto check=[&](A1Problem p,A1Domains d) {
        return verify(test::A1PropagationOracle(std::move(p)),d,counts);
    };
    CHECK(check(A1Problem(0,{}),{}).witness==std::optional<A1World>(A1World{}));
    check(A1Problem(0,{{{},PixelLabel::oak}}),{});
    check(A1Problem(2,{}),A1Domains(2));
    check(A1Problem(2,{}),{A1Domain(15),A1Domain(0)});
    check(A1Problem(2,{{{{0,true}},PixelLabel::oak}}),{A1Domain(15),A1Domain(0)});
    check(A1Problem(2,{{{},PixelLabel::background}}),A1Domains(2));
    const A1Problem lower(1,{{{{0,true}},PixelLabel::oak}});
    const auto ambiguous=check(lower,A1Domains(1));
    CHECK(ambiguous.domains==A1Domains({A1Domain(13)}));
    // Reduced domains are not globally supported domains: air still survives here.
    CHECK(!lower.accepts(A1World{A1State::air}));
    check(lower,{A1Domain(12)}); // same-material cube/slab ambiguity
    check(lower,{A1Domain(6)});  // equal-geometry stone/oak tie
    check(lower,{A1Domain(1)});  // foreground escape
    check(lower,{A1Domain(2)});  // an envelope reduction empties the domain
    check(A1Problem(1,{{{{0,false}},PixelLabel::background}}),{A1Domain(8)});
    const A1Problem mixed(2,{{{{0,true},{1,true}},PixelLabel::oak},
                             {{{0,false},{1,true}},PixelLabel::stone}});
    const auto simultaneous=check(mixed,{A1Domain(12),A1Domain(2)});
    CHECK(simultaneous.witness==std::optional<A1World>(A1World{A1State::oak_slab,A1State::stone}));
    InferenceStats again_stats;
    const auto again=envelope_feasible(mixed,simultaneous.domains,again_stats);
    CHECK(again.witness==simultaneous.witness && again.domains==simultaneous.domains && again_stats.deletions==0);
    check(A1Problem(2,{{{{0,true},{1,true}},PixelLabel::stone}}),{A1Domain(2),A1Domain(6)});
    check(A1Problem(2,{{{{0,true},{1,true}},PixelLabel::stone},
                       {{{1,true},{0,true}},PixelLabel::oak}}),A1Domains(2));
    const A1Constraint stone({{0,true},{1,false}},PixelLabel::stone);
    check(A1Problem(2,{stone,stone}),A1Domains(2));
    check(A1Problem(2,{stone,A1Constraint({{0,true},{1,false}},PixelLabel::oak)}),A1Domains(2));
    for(bool hit:{false,true}) for(unsigned mask=1;mask<16;++mask) {
        bool any=false;
        for(unsigned s=0;s<4;++s) if(mask & (1U<<s)) any=any || test::a1_scalar_emission({0,hit},s)!=0;
        CHECK(any==(test::a1_scalar_emission({0,hit},code(maximal_geometry(A1Domain(mask))))!=0));
    }
    const test::A1PropagationOracle oracle(lower);
    test::A1FeasibilityAudit audit(lower,oracle.worlds,oracle.family);
    audit.begin(QueryKind::nested_envelope,A1Domains(1));
    check_throws<std::runtime_error>([&]{ audit.prune({0,A1Domain(15),A1Domain(7),PruneReason::envelope_first_hit,0}); });
    check_throws<std::runtime_error>([&]{ audit.contradiction(ContradictionReason::foreground_escapes,0); });
    audit.begin(QueryKind::nested_envelope,A1Domains({A1Domain(1)}));
    audit.contradiction(ContradictionReason::foreground_escapes,0);
    audit.begin(QueryKind::nested_envelope,A1Domains(1));
    check_throws<std::runtime_error>([&]{ audit.contradiction(ContradictionReason::foreground_escapes,0); });
    InferenceStats invalid_stats;
    check_throws<std::invalid_argument>([&]{ (void)envelope_feasible(lower,{},invalid_stats); });
    CHECK(invalid_stats.envelope_calls==0);
}
void random_cases(Counts& counts) {
    std::mt19937 random(20260919);
    for(unsigned i=0;i<256;++i) {
        const auto truth=a1_world(4,random()%256);
        std::vector<A1Constraint> factors;
        for(unsigned r=0;r<i%7;++r) {
            std::vector<CellId> cells{0,1,2,3}; std::shuffle(cells.begin(),cells.end(),random);
            cells.resize(random()%5); std::vector<A1RayCell> steps;
            for(auto v:cells) steps.push_back({v,random()%2!=0});
            unsigned label=random()%3;
            if(i%2==0) {
                label=0;
                for(const auto& step:steps) {
                    label=test::a1_scalar_emission(step,code(truth[step.cell]));
                    if(label!=0) break;
                }
            }
            factors.emplace_back(steps,static_cast<PixelLabel>(label));
        }
        A1Domains initial;
        for(CellId v=0;v<4;++v) initial.emplace_back((random()%16) | (i%2==0 ? 1U<<code(truth[v]) : 0U));
        const test::A1PropagationOracle oracle(A1Problem(4,factors));
        const auto direct=verify(oracle,initial,counts);
        if(i%8==0) {
            test::A1PropagationAudit audit(oracle); InferenceStats gac_stats;
            const auto local=gac(oracle.problem,initial,gac_stats,&audit);
            if(local.consistent) CHECK(verify(oracle,local.domains,counts).feasible==direct.feasible);
            else CHECK(!direct.feasible);
        }
        std::reverse(factors.begin(),factors.end());
        CHECK(verify(test::A1PropagationOracle(A1Problem(4,factors)),initial,counts).feasible==direct.feasible);
    }
}
void physical_corpus(Counts& counts) {
    const Grid grid;
    const std::vector<Ray> rays{
        {{-1,Rational(1,4),Rational(1,2)},{1,0,0}},
        {{3,Rational(3,4),Rational(1,2)},{-1,0,0}},
        {{-1,Rational(5,4),Rational(1,2)},{1,0,0}},
        {{3,Rational(1,4),Rational(3,2)},{-1,0,0}},
        {{-1,Rational(7,4),Rational(3,2)},{1,0,0}},
        {{Rational(1,2),-1,Rational(3,2)},{0,1,0}}};
    std::vector<A1AabbReferenceRay> reference;
    std::vector<std::vector<A1RayCell>> programs;
    for(const auto& ray:rays) {
        reference.emplace_back(grid,ray); const A1TraversalRay traversal(grid,ray);
        programs.emplace_back(traversal.steps().begin(),traversal.steps().end());
    }
    std::vector<A1World> worlds;
    std::array<std::vector<std::size_t>,729> families;
    for(std::size_t id=0;id<65536;++id) {
        worlds.push_back(a1_world(8,id)); unsigned signature=0,power=1;
        for(const auto& ray:reference) { signature+=code(ray.sample(worlds.back()))*power; power*=3; }
        families[signature].push_back(id);
    }
    unsigned feasible_signatures=0;
    for(unsigned signature=0;signature<729;++signature) {
        std::vector<A1Constraint> factors; std::vector<PixelLabel> labels; unsigned digits=signature;
        for(const auto& program:programs) {
            labels.push_back(static_cast<PixelLabel>(digits%3)); digits/=3;
            factors.emplace_back(program,labels.back());
        }
        if(!families[signature].empty()) ++feasible_signatures;
        const A1Problem problem(8,std::move(factors));
        for(unsigned variant=0;variant<2;++variant) {
            A1Domains initial(8);
            if(variant) for(CellId v=0;v<8;++v) initial[v]=A1Domain(1+(signature*5+v*7)%15);
            const auto result=verify(problem,worlds,families[signature],initial,counts);
            if(result.witness) for(std::size_t r=0;r<reference.size();++r)
                CHECK(reference[r].sample(*result.witness)==labels[r]);
        }
    }
    std::cout << "physical_worlds=65536 physical_observations=729 physical_feasible_observations="
              << feasible_signatures << " physical_domain_variants=2\n";
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==1 || argc==2); const auto scenes=test::fixtures(MCR_A1_PROPAGATION_FIXTURE);
    if(argc==2) { python_agreement(scenes,argv[1]); return; }
    Counts counts; corpus(scenes,counts); named_cases(counts); random_cases(counts); physical_corpus(counts);
    CHECK(counts.calls==counts.feasible+counts.infeasible && counts.feasible==counts.witnesses);
    CHECK(counts.steps>0 && counts.infeasible>0 && counts.witnesses>0);
    std::cout << "feasibility_calls=" << counts.calls << " feasible=" << counts.feasible << " infeasible=" << counts.infeasible
              << " verified_witnesses=" << counts.witnesses << " audited_reductions=" << counts.steps
              << " deleted_states=" << counts.deletions << " factor_updates=" << counts.updates
              << " envelope_advances=" << counts.advances << " branches=0 search_nodes=0 support_queries=0\n";
}); }
