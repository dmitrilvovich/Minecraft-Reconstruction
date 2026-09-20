#include "fixtures/a1_problems.hpp"
#include "mcr/inference/a1_support_query.hpp"
#include "oracle/a1_feasibility_audit.hpp"
#include "oracle/a1_propagation_audit.hpp"
#include <array>

using namespace mcr;
struct Counts {
    std::uint64_t queries=0,supported=0,unsupported=0,witnesses=0,excluded=0,reductions=0,deletions=0;
};
// Check the query boundary separately from the existing feasibility audit. The
// condition is an assumption, never a deletion justified under the root family.
struct ConditionedAudit final : A1InferenceObserver {
    test::A1FeasibilityAudit kernel;
    A1Domains expected;
    ConditionedAudit(const A1Problem& p,std::span<const A1World> worlds,std::span<const std::size_t> family)
        : kernel(p,worlds,family) {}
    void expect(A1Domains domains,CellId v,A1State s) {
        domains[v]=A1Domain(domains[v].bits() & (1U<<code(s))); expected=std::move(domains);
    }
    void begin(QueryKind kind,std::span<const A1Domain> assumptions) override {
        CHECK(std::equal(assumptions.begin(),assumptions.end(),expected.begin(),expected.end()));
        kernel.begin(kind,assumptions);
    }
    void prune(const A1PruneEvent& event) override { kernel.prune(event); }
    void contradiction(ContradictionReason reason,std::optional<std::size_t> factor) override {
        kernel.contradiction(reason,factor);
    }
};
A1SupportQueryResult verify(const A1Problem& problem,std::span<const A1World> worlds,
                            std::span<const std::size_t> family,const A1Domains& initial,
                            CellId v,A1State s,Counts& counts) {
    // Direct filtering of original feasible worlds, independent of conditioning
    // code and of the production envelope/first-hit routines.
    std::vector<std::size_t> expected;
    for(auto id:family) {
        if(worlds[id][v]!=s) continue;
        bool allowed=true;
        for(CellId j=0;j<initial.size();++j)
            allowed=allowed && (initial[j].bits() & (1U<<code(worlds[id][j])))!=0;
        if(allowed) expected.push_back(id);
    }
    ConditionedAudit audit(problem,worlds,family); audit.expect(initial,v,s);
    InferenceStats stats,plain_stats; const auto before=initial;
    const auto result=query_support(problem,initial,v,s,stats,&audit);
    const auto plain=query_support(problem,initial,v,s,plain_stats);
    CHECK(initial==before && result.supported==!expected.empty());
    CHECK(result.supported==plain.supported && result.witness==plain.witness);
    CHECK(audit.kernel.conditioned==expected && audit.kernel.contexts==1);
    CHECK(audit.kernel.contradictions==static_cast<unsigned>(!result.supported));
    CHECK(stats.deletions==audit.kernel.deletions && stats.deletions==plain_stats.deletions);
    CHECK(stats.factor_updates==plain_stats.factor_updates && stats.envelope_advances==plain_stats.envelope_advances);
    for(const auto& measured:{stats,plain_stats}) {
        CHECK(measured.literal_queries==1 && measured.envelope_calls==1);
        CHECK(measured.infeasible_queries==static_cast<unsigned>(!result.supported));
        CHECK(measured.branches==0 && measured.search_nodes==0 && measured.gac_calls==0 && measured.fixed_calls==0);
    }
    std::size_t steps=0,bound=problem.constraints().size(),removed=0;
    for(const auto& factor:problem.constraints()) steps+=factor.steps().size();
    for(CellId j=0;j<initial.size();++j) {
        CHECK((audit.kernel.current[j] & audit.expected[j])==audit.kernel.current[j]);
        const auto delta=audit.expected[j].size()-audit.kernel.current[j].size();
        removed+=delta; bound+=problem.incident(j).size()*delta;
    }
    CHECK(stats.deletions==removed && stats.factor_updates<=bound && stats.envelope_advances<=steps);
    if(result.supported) {
        CHECK(result.witness && result.witness->at(v)==s && contains(initial,*result.witness));
        const auto id=static_cast<std::size_t>(a1_world_id(*result.witness));
        CHECK(std::find(expected.begin(),expected.end(),id)!=expected.end());
        ++counts.supported; ++counts.witnesses;
    } else { CHECK(!result.witness); ++counts.unsupported; }
    ++counts.queries; counts.excluded+=!initial[v].contains(s);
    counts.reductions+=audit.kernel.steps; counts.deletions+=stats.deletions;
    return result;
}
A1SupportQueryResult verify(const test::A1PropagationOracle& oracle,const A1Domains& domains,
                            CellId v,A1State s,Counts& counts) {
    return verify(oracle.problem,oracle.worlds,oracle.family,domains,v,s,counts);
}
void abstract_corpus(const std::vector<test::Fixture>& scenes,Counts& counts) {
    for(const auto& scene:scenes) {
        const test::A1PropagationOracle oracle(scene.problem);
        for(unsigned encoded=0;encoded<4096;++encoded) {
            const auto domains=test::domains_for(encoded);
            for(CellId v=0;v<3;++v) for(auto s:a1_states) verify(oracle,domains,v,s,counts);
        }
        std::cout << scene.name << ": all 4096 domain triples, 3 cells and 4 requested states\n";
    }
}
void named_cases(Counts& counts) {
    const test::A1PropagationOracle lower(A1Problem(1,{{{{0,true}},PixelLabel::oak}}));
    InferenceStats root_stats;
    const auto root=envelope_feasible(lower.problem,A1Domains(1),root_stats);
    CHECK(root.feasible && root.domains[0].contains(A1State::air));
    CHECK(!verify(lower,root.domains,0,A1State::air,counts).supported); // surviving air is not support
    for(auto s:a1_states) verify(lower,A1Domains(1),0,s,counts);
    CHECK(!verify(lower,{A1Domain(8)},0,A1State::oak,counts).supported); // cannot reintroduce cube
    const test::A1PropagationOracle mixed(A1Problem(2,{
        {{{0,true},{1,true}},PixelLabel::oak},{{{0,false},{1,true}},PixelLabel::stone}}));
    CHECK(verify(mixed,{A1Domain(12),A1Domain(2)},0,A1State::oak_slab,counts).supported);
    CHECK(!verify(mixed,{A1Domain(12),A1Domain(2)},0,A1State::oak,counts).supported);
    const test::A1PropagationOracle hidden(A1Problem(2,{{{{0,true},{1,false}},PixelLabel::stone}}));
    for(auto s:a1_states) CHECK(verify(hidden,{A1Domain(2),A1Domain(15)},1,s,counts).supported);
    const test::A1PropagationOracle blank(A1Problem(2,{}));
    for(auto s:a1_states) CHECK(verify(blank,A1Domains(2),1,s,counts).supported);
    CHECK(!verify(blank,{A1Domain(15),A1Domain(0)},0,A1State::air,counts).supported);
    CHECK(!verify(blank,{A1Domain(0),A1Domain(15)},0,A1State::air,counts).supported);
    const test::A1PropagationOracle impossible(A1Problem(1,{{{},PixelLabel::oak}}));
    for(auto s:a1_states) CHECK(!verify(impossible,A1Domains(1),0,s,counts).supported);

    // The same observer and accumulating stats must not leak one probe's family
    // into the next. Include a failing probe between successful ones.
    ConditionedAudit shared(blank.problem,blank.worlds,blank.family); InferenceStats accumulated;
    for(unsigned k=0;k<4;++k) {
        A1Domains d(2); if(k==2) d[1]=A1Domain(0);
        const auto s=a1_states[k]; shared.expect(d,0,s);
        const auto r=query_support(blank.problem,d,0,s,accumulated,&shared);
        CHECK(r.supported==(k!=2) && (r.supported ? r.witness->at(0)==s : !r.witness));
    }
    CHECK(shared.kernel.contexts==4 && shared.kernel.contradictions==1);
    CHECK(accumulated.literal_queries==4 && accumulated.envelope_calls==4 && accumulated.infeasible_queries==1);
    CHECK(accumulated.deletions==0 && accumulated.branches==0 && accumulated.search_nodes==0);
    ConditionedAudit invalid(lower.problem,lower.worlds,lower.family); InferenceStats invalid_stats;
    check_throws<std::invalid_argument>([&]{ (void)query_support(lower.problem,{},0,A1State::air,invalid_stats,&invalid); });
    check_throws<std::out_of_range>([&]{ (void)query_support(lower.problem,A1Domains(1),1,A1State::air,invalid_stats,&invalid); });
    for(unsigned code_value:{4U,255U}) check_throws<std::invalid_argument>([&]{
        (void)query_support(lower.problem,A1Domains(1),0,static_cast<A1State>(code_value),invalid_stats,&invalid);
    });
    check_throws<std::out_of_range>([&]{ (void)query_support(A1Problem(0,{}),{},0,A1State::air,invalid_stats,&invalid); });
    CHECK(invalid.kernel.contexts==0 && invalid_stats.literal_queries==0 && invalid_stats.envelope_calls==0);
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
            for(CellId v=0;v<8;++v) for(auto s:a1_states) {
                const auto result=verify(problem,worlds,families[signature],domains,v,s,counts);
                if(result.witness) for(std::size_t r=0;r<reference.size();++r)
                    CHECK(reference[r].sample(*result.witness)==labels[r]);
            }
        }
    }
    std::cout << "physical_worlds=65536 observation_vectors=729 domain_variants=2 physical_queries=46656\n";
}
void python_agreement(const std::vector<test::Fixture>& scenes,const std::filesystem::path& directory) {
    std::ifstream in(directory/"support_query.bin",std::ios::binary); CHECK(in.good());
    const auto byte=[&]() { const auto b=in.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); };
    std::string magic; for(unsigned i=0;i<8;++i) magic+=static_cast<char>(byte()); CHECK(magic=="MCRA1Q1\n");
    unsigned count=0; for(unsigned i=0;i<4;++i) count|=byte()<<(8*i); CHECK(count==64800);
    unsigned supported=0,seen=0;
    for(const auto& scene:scenes) for(unsigned encoded=0;encoded<3375;++encoded) {
        A1Domains domains; unsigned digits=encoded;
        for(unsigned v=0;v<3;++v) { domains.emplace_back(1+digits%15); digits/=15; }
        for(CellId v=0;v<3;++v) for(auto s:a1_states) if(domains[v].contains(s)) {
            InferenceStats stats; const auto result=query_support(scene.problem,domains,v,s,stats);
            const bool expected=byte()!=0; A1World witness;
            for(unsigned j=0;j<3;++j) witness.push_back(static_cast<A1State>(byte()));
            CHECK(result.supported==expected && stats.literal_queries==1 && stats.envelope_calls==1);
            CHECK(stats.infeasible_queries==static_cast<unsigned>(!expected) && stats.branches==0 && stats.search_nodes==0);
            if(expected) { CHECK(result.witness && *result.witness==witness && witness[v]==s && contains(domains,witness)); ++supported; }
            else CHECK(!result.witness);
            ++seen;
        }
    }
    CHECK(seen==count && in.get()==std::char_traits<char>::eof());
    std::cout << "python_queries=" << seen << " supported=" << supported << " unsupported=" << seen-supported
              << "; exact decisions and witnesses agree; zero search\n";
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==1 || argc==2); const auto scenes=test::fixtures(MCR_A1_PROPAGATION_FIXTURE);
    if(argc==2) { python_agreement(scenes,argv[1]); return; }
    Counts counts; abstract_corpus(scenes,counts); named_cases(counts); physical_corpus(counts);
    CHECK(counts.queries==counts.supported+counts.unsupported && counts.supported==counts.witnesses);
    CHECK(counts.reductions>0 && counts.supported>0 && counts.unsupported>counts.excluded);
    std::cout << "audited_queries=" << counts.queries << " supported=" << counts.supported
              << " unsupported=" << counts.unsupported << " verified_witnesses=" << counts.witnesses
              << " excluded_by_input=" << counts.excluded << " audited_reductions=" << counts.reductions
              << " deleted_states=" << counts.deletions << " branches=0 search_nodes=0\n";
}); }
