#include "test.hpp"
#include "oracle/audit.hpp"
#include "mcr/experiments/a0.hpp"
#include "mcr/inference/a0_solver.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>

using namespace mcr;
namespace {
struct Reader {
    std::ifstream stream;
    explicit Reader(const std::filesystem::path& path) : stream(path,std::ios::binary) { CHECK(stream.good()); }
    unsigned byte() { const int v=stream.get(); CHECK(v!=std::char_traits<char>::eof()); return static_cast<unsigned>(v); }
    std::uint32_t u32() {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i) value |= static_cast<std::uint32_t>(byte())<<(8*i);
        return value;
    }
    std::string text(std::size_t n) { CHECK(n<1000); std::string s; for(std::size_t i=0;i<n;++i) s+=static_cast<char>(byte()); return s; }
    Domains masks() { Domains d; for(unsigned v=0;v<8;++v) d.emplace_back(byte()); return d; }
    experiments::WorldIds ids() {
        const auto n=u32(); CHECK(n<=6561);
        experiments::WorldIds out;
        for(unsigned j=0;j<n;++j) { out.push_back(u32()); CHECK(out.back()<6561); }
        CHECK(std::is_sorted(out.begin(),out.end()));
        CHECK(std::adjacent_find(out.begin(),out.end())==out.end());
        return out;
    }
};
struct Totals {
    std::uint64_t cases=0,queries=0,negative_queries=0,prunes=0,deleted=0,contradictions=0;
    std::uint64_t witnesses=0,supported_literals=0,root_unsupported=0;
    void audit(const test::ExhaustiveAudit& a) { prunes+=a.steps; deleted+=a.literal_deletions; contradictions+=a.contradictions; }
};
std::vector<World> family(const experiments::A0Corpus& corpus,const experiments::WorldIds& ids) {
    std::vector<World> out;
    for(auto id:ids) out.push_back(corpus.worlds()[id]);
    return out;
}
void check_result(const Problem& problem,const Domains& initial,const std::vector<World>& worlds,
                  const Domains& expected,Totals& total,const Domains* python_root=nullptr) {
    test::ExhaustiveAudit audit(worlds);
    InferenceStats stats;
    const auto result=exact_supports(problem,initial,stats,&audit);
    std::vector<World> conditioned;
    for(const auto& w:worlds) if(contains(initial,w)) conditioned.push_back(w);
    CHECK(result.feasible==!conditioned.empty());
    InferenceStats plain_stats;
    const auto plain=exact_supports(problem,initial,plain_stats);
    CHECK(plain.feasible==result.feasible && plain.supported==result.supported && plain.root_domains==result.root_domains);
    const auto decision=fixed_geometry(problem,initial,plain_stats,&audit);
    CHECK(decision.feasible==result.feasible);
    if(result.feasible) {
        CHECK(result.supported==expected);
        CHECK(result.supported==test::supports(conditioned,8));
        if(python_root) CHECK(result.root_domains==*python_root);
        CHECK(std::find(conditioned.begin(),conditioned.end(),*result.witness)!=conditioned.end());
        CHECK(std::find(conditioned.begin(),conditioned.end(),*decision.witness)!=conditioned.end());
        ++total.witnesses;
        for(CellId v=0;v<8;++v) {
            CHECK((result.supported[v] & result.root_domains[v])==result.supported[v]);
            total.root_unsupported+=result.root_domains[v].size()-result.supported[v].size();
            for(auto s:a0_states) if(result.supported[v].contains(s)) {
                ++total.supported_literals;
                if(s!=A0State::air) {
                    auto witness=*result.witness; witness[v]=s;
                    CHECK(std::find(conditioned.begin(),conditioned.end(),witness)!=conditioned.end());
                }
            }
        }
    } else { CHECK(result.supported.empty() && !result.witness); }
    total.audit(audit);
}
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==3);
    const auto started=std::chrono::steady_clock::now();
    Reader input(std::filesystem::path(argv[1])/"cases.bin");
    CHECK(input.text(8)=="MCRA0C1\n");
    const experiments::A0Rig rig;
    const experiments::A0Corpus corpus(rig);
    CHECK(rig.factor_count()==47);
    CHECK(input.u32()==rig.factor_count());
    for(std::size_t r=0;r<rig.factor_count();++r) {
        const auto n=input.u32(); CHECK(n==rig.chain(r).size());
        for(auto v:rig.chain(r)) CHECK(input.u32()==v);
    }
    Totals totals;
    CHECK(input.u32()==rig.suites().size());
    std::vector<Domains> previous_world_support;
    for(const auto& suite:rig.suites()) {
        CHECK(input.text(input.u32())==suite.name);
        CHECK(input.u32()==suite.views);
        CHECK(input.u32()==suite.factors.size());
        for(auto r:suite.factors) CHECK(input.u32()==r);
        const auto classes=corpus.families(suite);
        CHECK(input.u32()==classes.size());
        std::set<std::uint32_t> visited_worlds;
        std::vector<Domains> world_support(6561);
        for(std::size_t j=0;j<classes.size();++j) {
            const auto ids=input.ids(); CHECK(!ids.empty());
            const auto observation=corpus.observation(ids.front(),suite.factors);
            const auto found=classes.find(observation);
            CHECK(found!=classes.end() && found->second==ids); // entire feasible relation
            for(auto id:ids) CHECK(visited_worlds.insert(id).second);
            const auto python_root=input.masks(),python_support=input.masks(),expected=input.masks();
            CHECK(python_support==expected);
            const auto worlds=family(corpus,ids);
            CHECK(expected==test::supports(worlds,8));
            CHECK(input.byte()==static_cast<unsigned>(std::popcount(test::freedom_mask(worlds,8))));
            for(CellId v=0;v<8;++v) for(auto s:a0_states) {
                const auto count=static_cast<std::uint32_t>(std::count_if(worlds.begin(),worlds.end(),[&](const auto& w){return w[v]==s;}));
                CHECK(input.u32()==count);
                CHECK((count>0)==expected[v].contains(s));
                CHECK((count==ids.size())==(expected[v]==Domain::singleton(s)));
            }
            const auto problem=rig.problem(suite.factors,observation);
            check_result(problem,Domains(8),worlds,expected,totals,&python_root);
            for(auto id:ids) world_support[id]=expected;
            ++totals.cases;
        }
        CHECK(visited_worlds.size()==6561);
        if(suite.name.starts_with("axis_")) {
            if(!previous_world_support.empty())
                for(std::size_t id=0;id<6561;++id) for(CellId v=0;v<8;++v)
                    CHECK((world_support[id][v] & previous_world_support[id][v])==world_support[id][v]);
            previous_world_support=std::move(world_support);
        }
        std::cout << suite.name << ": " << classes.size() << " complete feasible families checked\n" << std::flush;
    }
    const auto query_count=input.u32(); CHECK(query_count==512);
    for(unsigned j=0;j<query_count;++j) {
        const auto count=input.u32(); CHECK(count<=rig.factor_count());
        std::vector<std::size_t> indices;
        experiments::Observation labels;
        for(unsigned k=0;k<count;++k) { indices.push_back(input.u32()); labels.push_back(static_cast<PixelLabel>(input.byte())); }
        const auto initial=input.masks();
        const bool feasible=input.byte()!=0;
        const auto expected=input.masks();
        const auto expected_ids=input.ids();
        CHECK(feasible==!expected_ids.empty());
        std::vector<World> worlds;
        experiments::WorldIds actual_ids;
        for(std::uint32_t id=0;id<6561;++id) if(corpus.observation(id,indices)==labels) {
            worlds.push_back(corpus.worlds()[id]);
            if(contains(initial,corpus.worlds()[id])) actual_ids.push_back(id);
        }
        CHECK(actual_ids==expected_ids);
        check_result(rig.problem(indices,labels),initial,worlds,expected,totals);
        ++totals.queries;
        if(!feasible) ++totals.negative_queries;
    }
    CHECK(input.stream.get()==std::char_traits<char>::eof());
    CHECK(totals.cases==29032 && totals.negative_queries>0);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::ofstream report(argv[2]); CHECK(report.good());
    report << "{\n  \"status\": \"pass\",\n  \"worlds\": 6561,\n  \"observation_classes\": " << totals.cases
           << ",\n  \"conditioned_queries\": " << totals.queries << ",\n  \"infeasible_conditioned_queries\": " << totals.negative_queries
           << ",\n  \"audited_domain_reductions\": " << totals.prunes << ",\n  \"audited_literal_deletions\": " << totals.deleted
           << ",\n  \"audited_contradictions\": " << totals.contradictions << ",\n  \"verified_witnesses\": " << totals.witnesses
           << ",\n  \"supported_literals\": " << totals.supported_literals << ",\n  \"root_unsupported_survivors_total\": " << totals.root_unsupported
           << ",\n  \"search_branches\": 0,\n  \"acceptance_seconds_including_oracle\": " << seconds << "\n}\n";
    CHECK(report.good());
    std::cout << totals.cases << " classes, " << totals.queries << " conditioned queries, " << totals.prunes
              << " audited reductions; zero mismatches\n";
}); }
