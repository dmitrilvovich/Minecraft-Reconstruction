#include "fixtures/a1_problems.hpp"
#include "mcr/inference/a1_supports.hpp"
#include "oracle/a1_projection_audit.hpp"
#include "oracle/a1_propagation_audit.hpp"
#include <array>

using namespace mcr;
struct Counts {
    std::uint64_t projections=0,feasible=0,infeasible=0,witnesses=0,literals=0,query_checks=0;
    std::uint64_t kernel_prunes=0,kernel_deletions=0,projection_prunes=0,projection_deletions=0;
    std::uint64_t queries=0,negative_queries=0,envelope_calls=0;
};
A1SupportResult verify(const A1Problem& problem,std::span<const A1World> worlds,
                       std::span<const std::size_t> family,const A1Domains& initial,Counts& counts) {
    test::A1ProjectionAudit audit(problem,worlds,family,initial);
    InferenceStats stats,plain_stats; const auto before=initial;
    const auto result=exact_supports(problem,initial,stats,&audit);
    const auto plain=exact_supports(problem,initial,plain_stats);
    CHECK(initial==before && result.feasible==!audit.feasible_family.empty());
    CHECK(result.supported==audit.expected && result.supported.size()==initial.size());
    CHECK(result.feasible==plain.feasible && result.supported==plain.supported && result.witnesses==plain.witnesses);
    CHECK(stats.literal_queries==plain_stats.literal_queries && stats.infeasible_queries==plain_stats.infeasible_queries);
    CHECK(stats.deletions==plain_stats.deletions && stats.factor_updates==plain_stats.factor_updates);
    CHECK(stats.envelope_advances==plain_stats.envelope_advances);
    CHECK(stats.deletions==audit.kernel.deletions+audit.projection_deletions);
    CHECK(stats.envelope_calls==audit.kernel.contexts);
    CHECK(audit.kernel.contradictions==stats.infeasible_queries+static_cast<unsigned>(!result.feasible));
    for(const auto& measured:{stats,plain_stats}) {
        CHECK(measured.branches==0 && measured.search_nodes==0 && measured.gac_calls==0 && measured.fixed_calls==0);
        CHECK(measured.envelope_calls==1+measured.literal_queries);
    }
    A1Domains coverage(initial.size(),A1Domain(0));
    for(const auto& witness:result.witnesses) {
        CHECK(contains(initial,witness) && contains(result.supported,witness));
        const auto id=static_cast<std::size_t>(a1_world_id(witness));
        CHECK(std::find(audit.feasible_family.begin(),audit.feasible_family.end(),id)!=audit.feasible_family.end());
        for(CellId v=0;v<witness.size();++v)
            coverage[v]=A1Domain(coverage[v].bits() | (1U<<code(witness[v])));
    }
    CHECK(coverage==result.supported); // every reported literal has a verified simultaneous witness
    if(result.feasible) {
        CHECK(!result.witnesses.empty() && audit.projection_contexts==1 && audit.current==result.supported);
        CHECK(result.witnesses.size()==1+stats.literal_queries-stats.infeasible_queries);
        std::size_t candidates=0;
        for(auto d:audit.root) { CHECK(!d.empty()); candidates+=d.size(); }
        CHECK(stats.literal_queries<=candidates-initial.size()); // root witness already covers one per cell
        ++counts.feasible;
    } else {
        CHECK(result.witnesses.empty() && stats.literal_queries==0 && audit.projection_contexts==0);
        ++counts.infeasible;
    }
    // Compare with the public primitive on ORIGINAL domains, including states
    // pruned at the root and states already excluded by input restrictions.
    for(CellId v=0;v<initial.size();++v) for(auto s:a1_states) {
        InferenceStats query_stats;
        const auto query=query_support(problem,initial,v,s,query_stats);
        CHECK(query.supported==result.supported[v].contains(s));
        CHECK(query_stats.branches==0 && query_stats.search_nodes==0 && query_stats.envelope_calls==1);
        if(query.supported) {
            CHECK(query.witness && query.witness->at(v)==s && contains(initial,*query.witness));
            const auto id=static_cast<std::size_t>(a1_world_id(*query.witness));
            CHECK(std::find(audit.feasible_family.begin(),audit.feasible_family.end(),id)!=audit.feasible_family.end());
        } else CHECK(!query.witness);
        ++counts.query_checks;
    }
    ++counts.projections; counts.witnesses+=result.witnesses.size();
    for(auto d:result.supported) counts.literals+=d.size();
    counts.kernel_prunes+=audit.kernel.steps; counts.kernel_deletions+=audit.kernel.deletions;
    counts.projection_prunes+=audit.projection_prunes; counts.projection_deletions+=audit.projection_deletions;
    counts.queries+=stats.literal_queries; counts.negative_queries+=stats.infeasible_queries;
    counts.envelope_calls+=stats.envelope_calls;
    return result;
}
A1SupportResult verify(const test::A1PropagationOracle& oracle,const A1Domains& initial,Counts& counts) {
    return verify(oracle.problem,oracle.worlds,oracle.family,initial,counts);
}
void abstract_corpus(const std::vector<test::Fixture>& scenes,Counts& counts) {
    for(const auto& scene:scenes) {
        const test::A1PropagationOracle oracle(scene.problem);
        for(unsigned encoded=0;encoded<4096;++encoded) verify(oracle,test::domains_for(encoded),counts);
        std::cout << scene.name << ": exact projection and witness coverage for all 4096 domain triples\n";
    }
}
void named_cases(Counts& counts) {
    const auto check=[&](A1Problem p,A1Domains d) {
        return verify(test::A1PropagationOracle(std::move(p)),d,counts);
    };
    const auto empty=check(A1Problem(0,{}),{});
    CHECK(empty.feasible && empty.supported.empty() && empty.witnesses==std::vector<A1World>({{}}));
    CHECK(!check(A1Problem(0,{{{},PixelLabel::oak}}),{}).feasible);
    CHECK(check(A1Problem(2,{}),A1Domains(2)).supported==A1Domains(2));
    CHECK(check(A1Problem(2,{}),{A1Domain(3),A1Domain(12)}).supported==A1Domains({A1Domain(3),A1Domain(12)}));
    CHECK(check(A1Problem(2,{}),{A1Domain(15),A1Domain(0)}).supported==A1Domains(2,A1Domain(0)));
    check(A1Problem(2,{{{{0,true}},PixelLabel::oak}}),{A1Domain(15),A1Domain(0)});
    const A1Problem lower(1,{{{{0,true}},PixelLabel::oak}});
    InferenceStats root_stats; const auto root=envelope_feasible(lower,A1Domains(1),root_stats);
    CHECK(root.feasible && root.domains[0].contains(A1State::air));
    CHECK(check(lower,root.domains).supported==A1Domains({A1Domain(12)}));
    check(lower,{A1Domain(1)});
    check(lower,{A1Domain(2)});
    const A1Problem correlated(2,{{{{0,true},{1,true}},PixelLabel::oak}});
    const auto marginals=check(correlated,{A1Domain(9),A1Domain(9)});
    CHECK(marginals.feasible && marginals.supported==A1Domains(2,A1Domain(9)));
    CHECK(contains(marginals.supported,A1World{A1State::air,A1State::air}));
    CHECK(!correlated.accepts(A1World{A1State::air,A1State::air})); // marginals are not independent choices
    InferenceStats again_stats;
    CHECK(exact_supports(correlated,marginals.supported,again_stats).supported==marginals.supported);
    const A1Problem mixed(2,{{{{0,true},{1,true}},PixelLabel::oak},{{{0,false},{1,true}},PixelLabel::stone}});
    const auto determined=check(mixed,{A1Domain(12),A1Domain(2)});
    CHECK(determined.supported==A1Domains({A1Domain(8),A1Domain(2)}) && determined.witnesses.size()==1);
    std::vector<A1Constraint> reversed(mixed.constraints().begin(),mixed.constraints().end());
    std::reverse(reversed.begin(),reversed.end());
    CHECK(check(A1Problem(2,reversed),{A1Domain(12),A1Domain(2)}).supported==determined.supported);
    check(A1Problem(2,{{{{0,true},{1,false}},PixelLabel::stone}}),{A1Domain(2),A1Domain(15)});
    check(A1Problem(1,{{{{0,true}},PixelLabel::oak},{{{0,true}},PixelLabel::stone}}),A1Domains(1));
    // Audit controls reject root-context leakage and deletion of genuine ambiguity.
    const test::A1PropagationOracle oracle(lower);
    test::A1ProjectionAudit audit(lower,oracle.worlds,oracle.family,A1Domains(1));
    audit.begin(QueryKind::nested_envelope,A1Domains(1));
    check_throws<std::runtime_error>([&]{ audit.begin(QueryKind::support_projection,A1Domains({A1Domain(4)})); });
    audit.begin(QueryKind::support_projection,A1Domains(1));
    check_throws<std::runtime_error>([&]{ audit.prune({0,A1Domain(15),A1Domain(4),PruneReason::unsupported_state,{}}); });
    InferenceStats bad_stats;
    test::A1ProjectionAudit invalid(lower,oracle.worlds,oracle.family,A1Domains(1));
    check_throws<std::invalid_argument>([&]{ (void)exact_supports(lower,{},bad_stats,&invalid); });
    CHECK(bad_stats.envelope_calls==0 && invalid.kernel.contexts==0);
}
void physical_corpus(Counts& counts) {
    const Grid grid;
    const std::vector<Ray> rays{
        {{-1,Rational(1,4),Rational(1,2)},{1,0,0}},{{3,Rational(3,4),Rational(1,2)},{-1,0,0}},
        {{-1,Rational(5,4),Rational(1,2)},{1,0,0}},{{3,Rational(1,4),Rational(3,2)},{-1,0,0}},
        {{-1,Rational(7,4),Rational(3,2)},{1,0,0}},{{Rational(1,2),-1,Rational(3,2)},{0,1,0}}};
    std::vector<A1AabbReferenceRay> reference; std::vector<std::vector<A1RayCell>> programs;
    for(const auto& ray:rays) {
        reference.emplace_back(grid,ray); const A1TraversalRay traversal(grid,ray);
        programs.emplace_back(traversal.steps().begin(),traversal.steps().end());
    }
    std::vector<A1World> worlds; std::array<std::vector<std::size_t>,729> families;
    for(std::size_t id=0;id<65536;++id) {
        worlds.push_back(a1_world(8,id)); unsigned signature=0,power=1;
        for(const auto& ray:reference) { signature+=code(ray.sample(worlds.back()))*power; power*=3; }
        families[signature].push_back(id);
    }
    for(unsigned signature=0;signature<729;++signature) {
        std::vector<A1Constraint> factors; std::vector<PixelLabel> labels; unsigned digits=signature;
        for(const auto& program:programs) {
            labels.push_back(static_cast<PixelLabel>(digits%3)); digits/=3;
            factors.emplace_back(program,labels.back());
        }
        const A1Problem problem(8,std::move(factors));
        for(unsigned variant=0;variant<2;++variant) {
            A1Domains domains(8);
            if(variant) for(CellId v=0;v<8;++v) domains[v]=A1Domain(1+(signature*5+v*7)%15);
            const auto result=verify(problem,worlds,families[signature],domains,counts);
            for(const auto& witness:result.witnesses) for(std::size_t r=0;r<reference.size();++r)
                CHECK(reference[r].sample(witness)==labels[r]);
        }
    }
    std::cout << "physical_worlds=65536 observation_vectors=729 domain_variants=2 physical_projections=1458\n";
}
void python_agreement(const std::vector<test::Fixture>& scenes,const std::filesystem::path& directory) {
    std::ifstream in(directory/"projection.bin",std::ios::binary); CHECK(in.good());
    const auto byte=[&]() { const auto b=in.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); };
    std::string magic; for(unsigned i=0;i<8;++i) magic+=static_cast<char>(byte()); CHECK(magic=="MCRA1P2\n");
    unsigned count=0; for(unsigned i=0;i<4;++i) count|=byte()<<(8*i); CHECK(count==10125);
    unsigned seen=0,feasible=0,literals=0;
    for(const auto& scene:scenes) for(unsigned encoded=0;encoded<3375;++encoded) {
        A1Domains domains; unsigned digits=encoded;
        for(unsigned v=0;v<3;++v) { domains.emplace_back(1+digits%15); digits/=15; }
        const bool expected=byte()!=0; A1Domains support;
        for(unsigned v=0;v<3;++v) support.emplace_back(byte());
        InferenceStats stats; const auto result=exact_supports(scene.problem,domains,stats);
        CHECK(result.feasible==expected && result.supported==support);
        CHECK(stats.branches==0 && stats.search_nodes==0 && stats.envelope_calls==1+stats.literal_queries);
        ++seen; feasible+=result.feasible;
        for(auto d:support) literals+=d.size();
    }
    CHECK(seen==count && in.get()==std::char_traits<char>::eof());
    std::cout << "python_projections=" << seen << " feasible=" << feasible << " infeasible=" << seen-feasible
              << " supported_literals=" << literals << "; exact masks agree; zero search\n";
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==1 || argc==2); const auto scenes=test::fixtures(MCR_A1_PROPAGATION_FIXTURE);
    if(argc==2) { python_agreement(scenes,argv[1]); return; }
    Counts counts; abstract_corpus(scenes,counts); named_cases(counts); physical_corpus(counts);
    CHECK(counts.projections==counts.feasible+counts.infeasible && counts.projection_deletions>0);
    std::cout << "audited_projections=" << counts.projections << " feasible=" << counts.feasible << " infeasible=" << counts.infeasible
              << " verified_witnesses=" << counts.witnesses << " witnessed_literals=" << counts.literals
              << " single_query_checks=" << counts.query_checks << " kernel_prunes=" << counts.kernel_prunes
              << " kernel_deletions=" << counts.kernel_deletions << " projection_prunes=" << counts.projection_prunes
              << " projection_deletions=" << counts.projection_deletions << " literal_queries=" << counts.queries
              << " infeasible_queries=" << counts.negative_queries << " envelope_calls=" << counts.envelope_calls
              << " branches=0 search_nodes=0\n";
}); }
