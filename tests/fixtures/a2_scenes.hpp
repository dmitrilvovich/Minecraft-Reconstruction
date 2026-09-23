#pragma once
#include "mcr/inference/a2_solver.hpp"
#include <array>

namespace mcr::test {
struct A2Fixture {
    Grid grid;
    std::array<CellId,3> active;
    std::vector<Ray> rays;
    std::vector<PixelLabel> targets;
    A2Problem problem;
    [[nodiscard]] A2Domains domains() const {
        A2Domains result(grid.size(),A2Domain::singleton(A2State::air));
        for(auto v:active) result[v]=A2Domain();
        return result;
    }
    [[nodiscard]] A2World world(unsigned id) const {
        A2World result(grid.size(),A2State::air);
        const auto small=a2_world(3,id);
        for(unsigned v=0;v<3;++v) result[active[v]]=small[v];
        return result;
    }
};
// Frozen Phase A adversary: AB and AC form a path; BC closes the odd cycle.
// Coordinates and targets are unchanged except for the control's top material.
inline A2Fixture a2_fixture(bool triangle,A2Palette palette) {
    const Grid grid;
    std::vector<Ray> rays;
    std::vector<PixelLabel> targets;
    std::vector<A2Constraint> factors;
    for(unsigned pair=0;pair<(triangle ? 3U : 2U);++pair) for(unsigned level=0;level<2;++level) {
        const Rational h(level ? 3 : 1,4),half(1,2);
        const Ray ray=pair==0 ? Ray({-1,h,half},{1,0,0}) :
                      pair==1 ? Ray({half,h,-1},{0,0,1}) : Ray({3,h,-1},{-1,0,1});
        const auto target=material(level ? A2State::top_slab : A2State::bottom_slab,palette);
        const A2TraversalRay traversal(grid,ray,palette);
        factors.emplace_back(std::vector<A2RayCell>(traversal.steps().begin(),traversal.steps().end()),target,palette);
        rays.push_back(ray); targets.push_back(target);
    }
    return {grid,{grid.id({0,0,0}),grid.id({1,0,0}),grid.id({0,0,1})},
            std::move(rays),std::move(targets),A2Problem(grid.size(),std::move(factors),palette)};
}
} // namespace mcr::test
