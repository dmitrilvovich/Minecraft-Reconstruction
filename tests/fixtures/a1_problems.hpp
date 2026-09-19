#pragma once
#include "test.hpp"
#include "mcr/inference/a1_problem.hpp"
#include <filesystem>
#include <fstream>

namespace mcr::test {
struct Fixture { std::string name; A1Problem problem; };
inline std::vector<Fixture> fixtures(const std::filesystem::path& path) {
    std::ifstream in(path); CHECK(in.good());
    std::string magic; unsigned count=0; in>>magic>>count; CHECK(magic=="MCRA1P1" && count==3);
    std::vector<Fixture> result;
    for(unsigned k=0;k<count;++k) {
        std::string name; std::size_t n=0,m=0; in>>name>>n>>m; CHECK(n==3 && m==3);
        std::vector<A1Constraint> factors;
        for(std::size_t r=0;r<m;++r) {
            unsigned target=0; std::size_t length=0; in>>target>>length;
            std::vector<A1RayCell> steps;
            for(std::size_t j=0;j<length;++j) {
                CellId cell=0; unsigned hit=0; in>>cell>>hit; CHECK(hit<=1); steps.push_back({cell,hit!=0});
            }
            factors.emplace_back(steps,static_cast<PixelLabel>(target));
        }
        CHECK(in.good()); result.push_back({name,A1Problem(n,std::move(factors))});
    }
    in>>std::ws; CHECK(in.eof()); return result;
}
inline A1Domains domains_for(unsigned encoded) {
    A1Domains result;
    for(unsigned v=0;v<3;++v) result.emplace_back((encoded>>(4*v)) & 15);
    return result;
}
} // namespace mcr::test
