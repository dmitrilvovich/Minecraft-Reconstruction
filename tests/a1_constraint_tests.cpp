#include "test.hpp"
#include "mcr/inference/a1_constraint.hpp"
#include <filesystem>
#include <fstream>

using namespace mcr;
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==1 || argc==2);
    std::ifstream oracle;
    const auto byte=[&]() { const auto b=oracle.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); };
    if(argc==2) {
        oracle.open(std::filesystem::path(argv[1])/"local_supports.bin",std::ios::binary);
        CHECK(oracle.good());
        std::string magic; for(unsigned k=0;k<8;++k) magic+=static_cast<char>(byte());
        CHECK(magic=="MCRA1F1\n");
        unsigned count=0; for(unsigned k=0;k<4;++k) count |= byte()<<(8*k);
        CHECK(count==98304);
    }
    std::vector<A1World> worlds;
    for(unsigned id=0;id<64;++id) worlds.push_back(a1_world(3,id));
    std::uint64_t cases=0;
    // Exhaust every slab-hit pattern, every domain mask (including empty), and every label.
    for(unsigned flags=0;flags<8;++flags) for(unsigned encoded=0;encoded<4096;++encoded) {
        A1Domains domains;
        for(unsigned v=0;v<3;++v) domains.emplace_back((encoded>>(4*v)) & 15);
        const std::vector<A1RayCell> steps{{2,(flags&1)!=0},{0,(flags&2)!=0},{1,(flags&4)!=0}};
        for(unsigned target=0;target<3;++target) {
            const A1Constraint factor(steps,static_cast<PixelLabel>(target));
            A1Domains expected(3,A1Domain(0)); bool feasible=false;
            for(const auto& world:worlds) {
                if(!contains(domains,world)) continue;
                // Independent scalar first-hit oracle; no automaton or production emission call.
                unsigned label=0;
                for(const auto& step:steps) {
                    const auto state=code(world[step.cell]);
                    if(state==0 || (state==3 && !step.slab_hit)) continue;
                    label=state==1 ? 1U : 2U; break;
                }
                CHECK(factor.accepts(world)==(label==target));
                if(label!=target) continue;
                feasible=true;
                for(std::size_t j=0;j<steps.size();++j)
                    expected[j]=A1Domain(expected[j].bits() | (1U<<code(world[steps[j].cell])));
            }
            const auto before=domains;
            const auto actual=factor_supports(factor,domains);
            CHECK(domains==before && actual.feasible==feasible);
            if(feasible) {
                CHECK(actual.masks==expected);
                A1Domains reduced=domains;
                for(std::size_t j=0;j<steps.size();++j) {
                    CHECK((actual.masks[j] & domains[steps[j].cell])==actual.masks[j]);
                    reduced[steps[j].cell]=actual.masks[j];
                }
                CHECK(factor_supports(factor,reduced).masks==actual.masks);
            } else CHECK(actual.masks.empty());
            if(argc==2) {
                CHECK(byte()==static_cast<unsigned>(feasible));
                for(const auto mask:expected) CHECK(byte()==mask.bits());
            }
            ++cases;
        }
    }
    if(argc==2) CHECK(oracle.get()==std::char_traits<char>::eof());
    const A1Constraint lower({{0,true}},PixelLabel::oak),upper({{0,false}},PixelLabel::oak);
    CHECK(factor_supports(lower,A1Domains(1)).masks==A1Domains({A1Domain(12)})); // preserve cube/slab ambiguity
    CHECK(factor_supports(upper,A1Domains(1)).masks==A1Domains({A1Domain(4)}));
    CHECK(factor_supports(A1Constraint({{0,false}},PixelLabel::background),A1Domains(1)).masks==A1Domains({A1Domain(9)}));
    const A1Constraint pair({{0,false},{1,false}},PixelLabel::stone);
    CHECK(factor_supports(pair,A1Domains({A1Domain(12),A1Domain(2)})).masks==A1Domains({A1Domain(8),A1Domain(2)}));
    const A1Constraint occluded({{0,true},{1,true}},PixelLabel::stone);
    CHECK(factor_supports(occluded,A1Domains({A1Domain(2),A1Domain(15)})).masks==A1Domains({A1Domain(2),A1Domain(15)}));
    CHECK(!factor_supports(occluded,A1Domains({A1Domain(2),A1Domain(0)})).feasible);
    // The factor considers its own cells only; this is not a global satisfiability decision.
    CHECK(factor_supports(lower,A1Domains({A1Domain(4),A1Domain(0)})).feasible);
    CHECK(factor_supports(A1Constraint({},PixelLabel::background),A1Domains{}).feasible);
    CHECK(!factor_supports(A1Constraint({},PixelLabel::stone),A1Domains{}).feasible);
    CHECK(A1Constraint({},PixelLabel::background).accepts(A1World{}));
    CHECK(!A1Constraint({},PixelLabel::oak).accepts(A1World{}));
    check_throws<std::invalid_argument>([]{ (void)A1Constraint({{0,true},{0,false}},PixelLabel::oak); });
    check_throws<std::invalid_argument>([]{ (void)A1Constraint({},static_cast<PixelLabel>(3)); });
    check_throws<std::invalid_argument>([&]{ (void)factor_supports(pair,A1Domains(1)); });
    check_throws<std::invalid_argument>([&]{ (void)pair.accepts(A1World{A1State::oak}); });
    check_throws<std::invalid_argument>([&]{ (void)pair.accepts(A1World{A1State::oak,static_cast<A1State>(4)}); });
    std::cout << cases << " exhaustive A1 local-support cases; restricted/empty domains, contradictions and ambiguity checked\n";
}); }
