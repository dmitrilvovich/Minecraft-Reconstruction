#pragma once
#include "mcr/geometry/ray.hpp"
#include <string>

namespace mcr {
struct PinholeCamera {
    std::string name;
    Vec3 center, forward, right, up;
    int width, height;
    Rational fx, fy, cx, cy;
    void validate() const;
    // Row increases along up. Rays are not normalized.
    [[nodiscard]] Ray pixel(int column, int row) const;
};
} // namespace mcr

