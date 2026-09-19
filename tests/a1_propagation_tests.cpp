#include "oracle/a1_propagation_audit.hpp"
#include "fixtures/a1_problems.hpp"
#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>

using namespace mcr;
using test::A1PropagationOracle;
using test::Fixture;
using test::domains_for;
struct Counts { std::uint64_t cases=0,runs=0,steps=0,deletions=0,contradictions=0; };

void check_run(const A1PropagationOracle& oracle,const A1Domains& initial,
               const A1PropagationResult& expected,Counts& counts) {
    const auto before=initial;
    test::A1PropagationAudit audit(oracle);
    InferenceStats stats,plain_stats;
    const auto result=gac(oracle.problem,initial,stats,&audit);
    const auto plain=gac(oracle.problem,initial,plain_stats);
    CHECK(initial==before && result.consistent==expected.consistent);
    CHECK(result.consistent==plain.consistent && result.domains==plain.domains);
    CHECK(result.domains==audit.current && stats.gac_calls==1 && audit.contexts==1);
    CHECK(stats.deletions==audit.deletions && stats.deletions==plain_stats.deletions);
    CHECK(stats.factor_updates==plain_stats.factor_updates);
    CHECK(audit.contradictions==static_cast<unsigned>(!result.consistent));
    for(CellId v=0;v<initial.size();++v) CHECK((result.domains[v] & initial[v])==result.domains[v]);
    if(result.consistent) {
        CHECK(result.domains==expected.domains);
        for(std::size_t r=0;r<oracle.problem.constraints().size();++r) {
            const auto local=oracle.project(r,result.domains);
            CHECK(local.consistent);
            for(auto v:oracle.problem.constraints()[r].cells()) CHECK(local.domains[v]==result.domains[v]);
        }
        // A new observed invocation must establish a fresh assumption context.
        InferenceStats again_stats;
        const auto again=gac(oracle.problem,result.domains,again_stats,&audit);
        CHECK(again.consistent && again.domains==result.domains && again_stats.deletions==0);
        CHECK(again_stats.factor_updates==oracle.problem.constraints().size() && audit.contexts==2);
    }
    counts.runs+=audit.contexts; counts.steps+=audit.steps;
    counts.deletions+=audit.deletions; counts.contradictions+=audit.contradictions;
}

void python_agreement(const std::vector<Fixture>& scenes,const std::filesystem::path& directory) {
    std::ifstream in(directory/"propagation.bin",std::ios::binary); CHECK(in.good());
    const auto byte=[&]() { const auto b=in.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); };
    std::string magic; for(unsigned i=0;i<8;++i) magic+=static_cast<char>(byte());
    CHECK(magic=="MCRA1G1\n");
    unsigned count=0; for(unsigned i=0;i<4;++i) count|=byte()<<(8*i); CHECK(count==12288);
    for(const auto& scene:scenes) for(unsigned encoded=0;encoded<4096;++encoded) {
        InferenceStats stats; const auto result=gac(scene.problem,domains_for(encoded),stats);
        const bool consistent=byte()!=0; A1Domains expected;
        for(unsigned v=0;v<3;++v) expected.emplace_back(byte());
        CHECK(result.consistent==consistent);
        if(consistent) CHECK(result.domains==expected);
    }
    CHECK(in.get()==std::char_traits<char>::eof());
    std::cout << count << " multi-ray cases agree with frozen Python GAC\n";
}
void corpus(const std::vector<Fixture>& scenes,Counts& counts) {
    for(const auto& scene:scenes) {
        if(scene.name=="cascade") {
            InferenceStats stats; const auto result=gac(scene.problem,A1Domains(3),stats);
            CHECK(result.consistent && result.domains==A1Domains({A1Domain(2),A1Domain(4),A1Domain(1)}));
            CHECK(stats.factor_updates>scene.problem.constraints().size());
        }
        std::vector<A1PropagationOracle> schedules;
        std::vector<std::size_t> order{0,1,2};
        do {
            std::vector<A1Constraint> factors;
            for(auto r:order) factors.push_back(scene.problem.constraints()[r]);
            schedules.emplace_back(A1Problem(3,std::move(factors)));
        } while(std::next_permutation(order.begin(),order.end()));
        CHECK(schedules.size()==6);
        for(unsigned encoded=0;encoded<4096;++encoded) {
            const auto initial=domains_for(encoded),before=initial;
            const auto expected=schedules.front().closure(initial);
            for(const auto& oracle:schedules) check_run(oracle,initial,expected,counts);
            CHECK(initial==before); ++counts.cases;
        }
        std::cout << scene.name << ": all 4096 domain triples and all 6 factor orders audited\n";
    }
}
void named_cases(Counts& counts) {
    const auto verify=[&](A1Problem p,A1Domains d) {
        const A1PropagationOracle oracle(std::move(p));
        check_run(oracle,d,oracle.closure(d),counts); ++counts.cases;
    };
    verify(A1Problem(0,{}),{});
    verify(A1Problem(2,{}),A1Domains(2));
    verify(A1Problem(2,{}),{A1Domain(15),A1Domain(0)});
    verify(A1Problem(2,{{{{0,true}},PixelLabel::oak}}),{A1Domain(15),A1Domain(0)});
    verify(A1Problem(2,{{{},PixelLabel::background}}),A1Domains(2));
    verify(A1Problem(2,{{{},PixelLabel::stone}}),A1Domains(2));
    const A1Constraint stone({{0,true},{1,false}},PixelLabel::stone);
    const A1Constraint oak({{0,true},{1,false}},PixelLabel::oak);
    verify(A1Problem(2,{stone,oak}),A1Domains(2));
    verify(A1Problem(2,{stone,stone}),A1Domains(2));
    const A1Problem duplicates(2,{stone,oak});
    CHECK(duplicates.constraints().size()==2 && duplicates.incident(0).size()==2);
    CHECK(duplicates.incident(0)[0]==0 && duplicates.incident(0)[1]==1);
    InferenceStats stats;
    CHECK(!gac(duplicates,A1Domains(2),stats).consistent);
    const A1Problem lower(1,{{{{0,true}},PixelLabel::oak}});
    CHECK(gac(lower,A1Domains(1),stats).domains==A1Domains({A1Domain(12)}));
    const A1Problem mixed(2,{{{{0,true},{1,true}},PixelLabel::oak},
                              {{{0,false},{1,true}},PixelLabel::stone}});
    CHECK(gac(mixed,A1Domains(2),stats).domains==A1Domains({A1Domain(8),A1Domain(2)}));
    const A1Problem hidden(2,{{{{0,true},{1,true}},PixelLabel::stone}});
    CHECK(gac(hidden,{A1Domain(2),A1Domain(15)},stats).domains==A1Domains({A1Domain(2),A1Domain(15)}));
    // Marginal masks must not be interpreted as independent choices of a world.
    const A1World invalid_combination{A1State::air,A1State::oak};
    CHECK(contains(gac(hidden,A1Domains(2),stats).domains,invalid_combination));
    CHECK(!hidden.accepts(invalid_combination));
    check_throws<std::invalid_argument>([]{ (void)A1Problem(1,{{{{1,true}},PixelLabel::oak}}); });
    check_throws<std::invalid_argument>([&]{ (void)gac(lower,A1Domains(2),stats); });
    check_throws<std::invalid_argument>([&]{ (void)lower.accepts(A1World{}); });
    check_throws<std::invalid_argument>([&]{ (void)hidden.accepts(A1World{A1State::oak,static_cast<A1State>(4)}); });
    check_throws<std::out_of_range>([&]{ (void)lower.incident(1); });
    const A1PropagationOracle oracle(lower);
    test::A1PropagationAudit audit(oracle);
    audit.begin(QueryKind::gac,A1Domains(1));
    check_throws<std::runtime_error>([&]{ audit.prune({0,A1Domain(15),A1Domain(7),PruneReason::ray_arc_support,0}); });
    check_throws<std::runtime_error>([&]{ audit.contradiction(ContradictionReason::no_ray_support,0); });
    audit.begin(QueryKind::gac,A1Domains({A1Domain(1)}));
    audit.contradiction(ContradictionReason::no_ray_support,0);
    audit.begin(QueryKind::gac,A1Domains(1));
    check_throws<std::runtime_error>([&]{ audit.prune({0,A1Domain(15),A1Domain(7),PruneReason::ray_arc_support,0}); });
}
void random_cases(Counts& counts) {
    std::mt19937 random(20260918);
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
                    const auto s=code(truth[step.cell]);
                    if(s==0 || (s==3 && !step.slab_hit)) continue;
                    label=s==1 ? 1U : 2U; break;
                }
            }
            factors.emplace_back(steps,static_cast<PixelLabel>(label));
        }
        A1Domains initial;
        for(CellId v=0;v<4;++v) initial.emplace_back((random()%16) | (i%2==0 ? 1U<<code(truth[v]) : 0U));
        const A1PropagationOracle forward(A1Problem(4,factors));
        const auto expected=forward.closure(initial);
        check_run(forward,initial,expected,counts);
        std::reverse(factors.begin(),factors.end());
        check_run(A1PropagationOracle(A1Problem(4,factors)),initial,expected,counts);
        ++counts.cases;
    }
}
void physical_scene(Counts& counts) {
    const Grid grid;
    const std::vector<Ray> rays{
        {{-1,Rational(1,4),Rational(1,2)},{1,0,0}},
        {{3,Rational(1,4),Rational(1,2)},{-1,0,0}},
        {{-1,Rational(3,4),Rational(1,2)},{1,0,0}},
        {{3,Rational(3,4),Rational(1,2)},{-1,0,0}},
        {{Rational(1,2),Rational(1,4),-1},{0,0,1}},
        {{Rational(1,2),-1,Rational(1,2)},{0,1,0}}};
    const auto truth=a1_world(8,12345);
    std::vector<A1AabbReferenceRay> reference;
    std::vector<A1Constraint> factors;
    for(const auto& ray:rays) {
        reference.emplace_back(grid,ray); const A1TraversalRay traversal(grid,ray);
        factors.emplace_back(std::vector<A1RayCell>(traversal.steps().begin(),traversal.steps().end()),reference.back().sample(truth));
    }
    const A1PropagationOracle oracle(A1Problem(8,factors),reference);
    A1Domains initial(8);
    check_run(oracle,initial,oracle.closure(initial),counts); ++counts.cases;
    initial[0]=A1Domain::singleton(truth[0]); initial[3]=A1Domain::singleton(truth[3]);
    const auto expected=oracle.closure(initial);
    check_run(oracle,initial,expected,counts); ++counts.cases;
    std::reverse(factors.begin(),factors.end()); std::reverse(reference.begin(),reference.end());
    check_run(A1PropagationOracle(A1Problem(8,factors),reference),initial,expected,counts);
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==1 || argc==2); const auto scenes=test::fixtures(MCR_A1_PROPAGATION_FIXTURE);
    if(argc==2) { python_agreement(scenes,argv[1]); return; }
    Counts counts; corpus(scenes,counts); named_cases(counts); random_cases(counts); physical_scene(counts);
    CHECK(counts.steps>0 && counts.deletions>=counts.steps && counts.contradictions>0);
    std::cout << "scene_cases=" << counts.cases << " audited_runs=" << counts.runs
              << " reductions=" << counts.steps << " deleted_states=" << counts.deletions
              << " detected_contradictions=" << counts.contradictions << '\n';
}); }
