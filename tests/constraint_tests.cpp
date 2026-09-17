#include "test.hpp"
#include "mcr/inference/gac.hpp"
#include <numeric>

using namespace mcr;
int main() { return run_tests([] {
    check_throws<std::invalid_argument>([]{ (void)FirstHitConstraint({0,0},PixelLabel::stone); });
    check_throws<std::invalid_argument>([]{ (void)Problem(1,{{{1},PixelLabel::stone}}); });
    for(unsigned encoded=0;encoded<2401;++encoded) {
        unsigned digits=encoded;
        Domains domains(4);
        for(auto& d:domains) { d=Domain(1+digits%7); digits/=7; }
        for(unsigned target=0;target<3;++target) {
            const FirstHitConstraint factor({2,0,3,1},static_cast<PixelLabel>(target));
            Domains expected(4,Domain(0)); bool feasible=false;
            for(std::uint64_t id=0;id<81;++id) {
                const auto world=a0_world(4,id);
                if(!contains(domains,world)) continue;
                unsigned label=0;
                for(auto v:factor.cells()) if(world[v]!=A0State::air) { label=code(world[v]); break; }
                CHECK(factor.accepts(world)==(label==target));
                if(label!=target) continue;
                feasible=true;
                for(std::size_t j=0;j<4;++j)
                    expected[j]=Domain(expected[j].bits() | (1U<<code(world[factor.cells()[j]])));
            }
            const auto actual=factor_supports(factor,domains);
            CHECK(actual.feasible==feasible);
            if(feasible) CHECK(actual.masks==expected);
        }
    }
    InferenceStats stats;
    const Problem pair(2,{{{0,1},PixelLabel::stone}});
    auto result=gac(pair,Domains(2),stats);
    CHECK(result.feasible);
    CHECK(result.domains==Domains({Domain(3),Domain(7)}));
    CHECK(gac(pair,result.domains,stats).domains==result.domains);
    CHECK(!gac(Problem(2,{{{0,1},PixelLabel::stone},{{0,1},PixelLabel::oak}}),Domains(2),stats).feasible);
    CHECK(gac(Problem(1,{{{},PixelLabel::background}}),Domains(1),stats).feasible);
    CHECK(!gac(Problem(1,{{{},PixelLabel::stone}}),Domains(1),stats).feasible);
    CHECK(!gac(Problem(1,{}),Domains(1,Domain(0)),stats).feasible);
    std::cout << "7,203 exhaustive four-cell local support cases checked\n";
}); }
