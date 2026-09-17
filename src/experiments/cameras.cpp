#include "mcr/experiments/cameras.hpp"
#include <algorithm>

namespace mcr::experiments {
std::vector<PinholeCamera> phase_a_cameras(const Grid& grid, int resolution) {
    struct Basis { const char* name; Vec3 f,r,u; };
    const Vec3 f{Rational(2,3),Rational(-2,3),Rational(-1,3)};
    const Vec3 r{Rational(1,3),Rational(2,3),Rational(-2,3)};
    const Vec3 u{Rational(2,3),Rational(1,3),Rational(2,3)};
    const std::vector<Basis> bases{
        {"minus_x",{1,0,0},{0,0,-1},{0,1,0}},
        {"minus_y",{0,1,0},{1,0,0},{0,0,-1}},
        {"plus_z",{0,0,-1},{-1,0,0},{0,1,0}},
        {"plus_x",{-1,0,0},{0,0,1},{0,1,0}},
        {"plus_y",{0,-1,0},{1,0,0},{0,0,1}},
        {"minus_z",{0,0,1},{1,0,0},{0,1,0}},
        {"oblique",f,r,u}, {"opposite_oblique",Rational(-1)*f,Rational(-1)*r,u}
    };
    const auto shape = grid.shape();
    const Vec3 target{Rational(shape[0],2),Rational(shape[1],2),Rational(shape[2],2)};
    const Rational distance = Rational(3)*Rational(*std::max_element(shape.begin(),shape.end()));
    std::vector<PinholeCamera> result;
    for (const auto& b : bases) {
        PinholeCamera camera{b.name,target-distance*b.f,b.f,b.r,b.u,resolution,resolution,
                            Rational(3)*Rational(resolution,2),Rational(3)*Rational(resolution,2),
                            Rational(resolution,2),Rational(resolution,2)};
        camera.validate();
        result.push_back(camera);
    }
    return result;
}
} // namespace mcr::experiments

