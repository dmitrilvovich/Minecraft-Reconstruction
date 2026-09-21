#include "test.hpp"
#include "fixtures/a1_corpus.hpp"
#include "fixtures/a1_scenes.hpp"
#include "oracle/a1_acceptance_audit.hpp"
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>

using namespace mcr;
namespace {
struct Reader {
    std::ifstream stream;
    explicit Reader(const std::filesystem::path& p) : stream(p,std::ios::binary) { CHECK(stream.good()); }
    unsigned byte() { const int b=stream.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); }
    std::uint32_t u32() {
        std::uint32_t v=0; for(unsigned k=0;k<4;++k) v |= static_cast<std::uint32_t>(byte())<<(8*k); return v;
    }
    std::string text(std::size_t n) { CHECK(n<1000); std::string out; for(std::size_t k=0;k<n;++k) out+=static_cast<char>(byte()); return out; }
    A1Domains masks() { A1Domains out; for(unsigned v=0;v<8;++v) out.emplace_back(byte()); return out; }
    experiments::WorldIds ids() {
        const auto n=u32(); CHECK(n<=65536);
        experiments::WorldIds out; out.reserve(n);
        for(unsigned k=0;k<n;++k) { out.push_back(u32()); CHECK(out.back()<65536); }
        CHECK(std::is_sorted(out.begin(),out.end()));
        CHECK(std::adjacent_find(out.begin(),out.end())==out.end()); return out;
    }
};
struct Totals {
    std::uint64_t cases=0,restricted=0,negative=0,named=0,validations=0,feasible=0;
    std::uint64_t contexts=0,prunes=0,deleted=0,contradictions=0;
    std::uint64_t projection_witnesses=0,feasibility_witnesses=0,query_witnesses=0,supported_literals=0;
    std::uint64_t direct_queries=0,query_supported=0,query_unsupported=0,gac_survivors=0;
    std::uint64_t envelopes=0,literals=0,failed_literals=0,branches=0,nodes=0;
};
void verify_witness(const A1World& witness,const A1Problem& problem,const A1Domains& initial,
                    const experiments::WorldIds& conditioned) {
    CHECK(witness.size()==8 && contains(initial,witness));
    // Family membership is based solely on independent ray/AABB rendering.
    CHECK(std::binary_search(conditioned.begin(),conditioned.end(),static_cast<std::uint32_t>(a1_world_id(witness))));
    CHECK(test::a1_direct_accepts(problem,witness)); // separate scalar constraint evaluator
}
void validate(const A1Problem& problem,const A1Domains& initial,
              const test::A1Corpus& corpus,const experiments::WorldIds& family,
              const experiments::WorldIds& conditioned,const A1Domains& expected,Totals& totals,
              const A1Domains* python_gac=nullptr,bool all_queries=false) {
    CHECK(expected==test::a1_supports(corpus.worlds(),conditioned,8));
    test::A1ExhaustiveAudit audit(corpus.worlds(),family);
    InferenceStats stats;
    const auto local=gac(problem,initial,stats,&audit);
    if(python_gac) CHECK(local.consistent && local.domains==*python_gac);
    const auto decision=envelope_feasible(problem,initial,stats,&audit);
    const auto result=exact_supports(problem,initial,stats,&audit);
    InferenceStats plain_stats;
    const auto plain=exact_supports(problem,initial,plain_stats);
    CHECK(result.feasible==!conditioned.empty() && decision.feasible==result.feasible);
    CHECK(result.supported==expected);
    CHECK(plain.feasible==result.feasible && plain.supported==result.supported && plain.witnesses==result.witnesses);
    if(result.feasible) {
        CHECK(local.consistent && decision.witness && !result.witnesses.empty());
        verify_witness(*decision.witness,problem,initial,conditioned); ++totals.feasibility_witnesses;
        CHECK(contains(decision.domains,*decision.witness));
        A1Domains certificates(8,A1Domain(0));
        for(const auto& witness:result.witnesses) {
            verify_witness(witness,problem,initial,conditioned);
            for(CellId v=0;v<8;++v) certificates[v]=A1Domain(certificates[v].bits() | (1U<<code(witness[v])));
            ++totals.projection_witnesses;
        }
        CHECK(certificates==result.supported); // every marginal literal has an actual simultaneous witness
        for(CellId v=0;v<8;++v) {
            CHECK((result.supported[v] & initial[v])==result.supported[v]);
            CHECK((result.supported[v] & decision.domains[v])==result.supported[v]);
            CHECK((result.supported[v] & local.domains[v])==result.supported[v]);
            totals.gac_survivors+=local.domains[v].size()-result.supported[v].size();
            totals.supported_literals+=result.supported[v].size();
        }
        ++totals.feasible;
    } else {
        CHECK(result.supported==A1Domains(8,A1Domain(0)) && result.witnesses.empty() && !decision.witness);
    }
    // One rotating literal for every complete camera family; all 32, including
    // input-excluded states, for every restricted/altered observation fixture.
    const unsigned first=all_queries ? 0U : static_cast<unsigned>(totals.validations%32);
    const unsigned end=all_queries ? 32U : first+1;
    for(unsigned literal=first;literal<end;++literal) {
        const CellId v=literal/4; const auto state=static_cast<A1State>(literal%4);
        const auto query=query_support(problem,initial,v,state,stats,&audit);
        CHECK(query.supported==expected[v].contains(state));
        CHECK(query.supported==query.witness.has_value());
        if(query.supported) {
            verify_witness(*query.witness,problem,initial,conditioned);
            CHECK((*query.witness)[v]==state);
            ++totals.query_supported; ++totals.query_witnesses;
        } else ++totals.query_unsupported;
        ++totals.direct_queries;
    }
    CHECK(stats.branches==0 && stats.search_nodes==0 && plain_stats.branches==0 && plain_stats.search_nodes==0);
    ++totals.validations;
    totals.contexts+=audit.contexts; totals.prunes+=audit.steps; totals.deleted+=audit.literal_deletions;
    totals.contradictions+=audit.contradictions;
    totals.envelopes+=stats.envelope_calls; totals.literals+=stats.literal_queries;
    totals.failed_literals+=stats.infeasible_queries; totals.branches+=stats.branches; totals.nodes+=stats.search_nodes;
}
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==3);
    Reader input(std::filesystem::path(argv[1])/"acceptance_cases.bin");
    CHECK(input.text(8)=="MCRA1C1\n");
    const test::A1Rig rig;
    const test::A1Corpus corpus(rig);
    CHECK(rig.factor_count()==73 && input.u32()==rig.factor_count());
    for(std::size_t r=0;r<rig.factor_count();++r) {
        CHECK(input.u32()==rig.steps(r).size());
        for(const auto& step:rig.steps(r)) { CHECK(input.u32()==step.cell); CHECK(input.byte()==step.slab_hit); }
    }
    CHECK(input.u32()==rig.suites().size());
    Totals totals;
    std::vector<std::array<unsigned,8>> previous;
    const auto scenes=test::a1_adversarial_scenes(); CHECK(scenes.size()==39);
    for(const auto& suite:rig.suites()) {
        CHECK(input.text(input.u32())==suite.name && input.u32()==suite.views);
        CHECK(input.u32()==suite.factors.size());
        for(auto r:suite.factors) CHECK(input.u32()==r);
        const auto classes=corpus.families(suite);
        CHECK(input.u32()==classes.size());
        std::vector<bool> visited(65536,false);
        std::vector<std::array<unsigned,8>> per_world(65536);
        for(std::size_t c=0;c<classes.size();++c) {
            const auto ids=input.ids(); CHECK(!ids.empty());
            try {
                const auto labels=corpus.observation(ids.front(),suite.factors);
                const auto found=classes.find(labels);
                CHECK(found!=classes.end() && found->second==ids);
                const auto python_gac=input.masks(),python_support=input.masks(),expected=input.masks();
                CHECK(python_support==expected);
                CHECK(input.byte()==static_cast<unsigned>(std::popcount(test::a1_freedom_mask(ids,8))));
                std::array<std::array<std::uint32_t,4>,8> counts{};
                for(auto id:ids) {
                    CHECK(!visited[id]); visited[id]=true;
                    for(CellId v=0;v<8;++v) {
                        ++counts[v][code(corpus.worlds()[id][v])];
                        per_world[id][v]=expected[v].bits();
                    }
                }
                for(CellId v=0;v<8;++v) for(auto s:a1_states) {
                    const auto count=counts[v][code(s)];
                    CHECK(input.u32()==count);
                    CHECK((count>0)==expected[v].contains(s));
                    CHECK((count==ids.size())==(expected[v]==A1Domain::singleton(s)));
                }
                validate(rig.problem(suite.factors,labels),A1Domains(8),corpus,ids,ids,expected,totals,&python_gac);
                ++totals.cases;
            } catch(const std::exception& e) {
                throw std::runtime_error(suite.name+" representative="+std::to_string(ids.front())+": "+e.what());
            }
        }
        CHECK(std::all_of(visited.begin(),visited.end(),[](bool value){return value;}));
        if(suite.name.starts_with("axis_")) {
            if(!previous.empty()) for(std::size_t id=0;id<65536;++id) for(CellId v=0;v<8;++v)
                CHECK((per_world[id][v] & previous[id][v])==per_world[id][v]);
            previous=std::move(per_world);
        }
        for(const auto& scene:scenes) try {
            const auto labels=corpus.observation(static_cast<std::uint32_t>(a1_world_id(scene.world)),suite.factors);
            const auto& ids=classes.at(labels);
            const auto expected=test::a1_supports(corpus.worlds(),ids,8);
            const auto problem=rig.problem(suite.factors,labels);
            validate(problem,A1Domains(8),corpus,ids,ids,expected,totals);
            CHECK(contains(expected,scene.world));
            std::vector<A1Constraint> repeated(problem.constraints().begin(),problem.constraints().end());
            repeated.insert(repeated.end(),problem.constraints().begin(),problem.constraints().end());
            std::reverse(repeated.begin(),repeated.end());
            const A1Problem reordered(8,std::move(repeated));
            validate(reordered,A1Domains(8),corpus,ids,ids,expected,totals);
            InferenceStats stats;
            const auto forward=gac(problem,A1Domains(8),stats),reverse=gac(reordered,A1Domains(8),stats);
            CHECK(forward.consistent && reverse.consistent && forward.domains==reverse.domains);
            CHECK(gac(problem,forward.domains,stats).domains==forward.domains);
            CHECK(stats.branches==0 && stats.search_nodes==0);
            ++totals.named;
        } catch(const std::exception& e) { throw std::runtime_error(scene.name+" / "+suite.name+": "+e.what()); }
        std::cout << suite.name << ": " << classes.size() << " complete A1 families checked\n" << std::flush;
    }
    const auto queries=input.u32(); CHECK(queries==512);
    for(unsigned q=0;q<queries;++q) {
        const auto n=input.u32(); CHECK(n<=rig.factor_count()+1);
        std::vector<std::size_t> indices; experiments::Observation labels;
        for(unsigned j=0;j<n;++j) { indices.push_back(input.u32()); labels.push_back(static_cast<PixelLabel>(input.byte())); }
        const auto initial=input.masks();
        const bool feasible=input.byte()!=0;
        const auto expected=input.masks();
        const auto ids=input.ids(); CHECK(feasible==!ids.empty());
        experiments::WorldIds family,conditioned;
        for(std::uint32_t id=0;id<65536;++id) if(corpus.matches(id,indices,labels)) {
            family.push_back(id);
            if(contains(initial,corpus.worlds()[id])) conditioned.push_back(id);
        }
        CHECK(ids==conditioned);
        try { validate(rig.problem(indices,labels),initial,corpus,family,conditioned,expected,totals,nullptr,true); }
        catch(const std::exception& e) { throw std::runtime_error("conditioned query "+std::to_string(q)+": "+e.what()); }
        ++totals.restricted; if(!feasible) ++totals.negative;
    }
    CHECK(input.stream.get()==std::char_traits<char>::eof());
    CHECK(totals.cases==195620 && totals.negative==421 && totals.named==312);
    CHECK(totals.query_supported>0 && totals.query_unsupported>0 && totals.branches==0 && totals.nodes==0);
    std::ofstream report(argv[2]); CHECK(report.good());
    report << "{\n  \"status\": \"pass\",\n  \"worlds\": 65536,\n  \"view_suites\": 8,\n  \"distinct_factors\": 73";
    const auto field=[&](const char* key,std::uint64_t value){report << ",\n  \"" << key << "\": " << value;};
    field("observation_classes",totals.cases); field("family_membership_comparisons",8*65536);
    field("occurrence_count_comparisons",totals.cases*32); field("freedom_count_comparisons",totals.cases);
    field("axis_support_monotonicity_checks",5*65536*8);
    field("restricted_cases",totals.restricted); field("infeasible_restricted_cases",totals.negative);
    field("named_scene_view_cases",totals.named); field("audited_pipeline_calls",totals.validations);
    field("feasible_pipeline_calls",totals.feasible); field("audited_contexts",totals.contexts);
    field("audited_domain_reductions",totals.prunes); field("audited_literal_deletions",totals.deleted);
    field("audited_contradictions",totals.contradictions);
    field("verified_projection_witnesses",totals.projection_witnesses);
    field("verified_feasibility_witnesses",totals.feasibility_witnesses);
    field("verified_query_witnesses",totals.query_witnesses);
    field("supported_literal_certificates",totals.supported_literals);
    field("direct_query_comparisons",totals.direct_queries); field("direct_queries_supported",totals.query_supported);
    field("direct_queries_unsupported",totals.query_unsupported); field("gac_unsupported_survivors",totals.gac_survivors);
    field("audited_envelope_calls",totals.envelopes); field("audited_literal_queries",totals.literals);
    field("audited_infeasible_literal_queries",totals.failed_literals);
    field("search_branches",totals.branches); field("search_nodes",totals.nodes);
    report << "\n}\n"; CHECK(report.good());
    std::cout << totals.cases << " A1 families; " << totals.prunes << " audited reductions; zero search and zero mismatches\n";
}); }
