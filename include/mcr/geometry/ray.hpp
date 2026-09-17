#pragma once
#include "mcr/math/vec3.hpp"
#include <stdexcept>

namespace mcr {
struct Ray {
    Vec3 origin;
    Vec3 direction;
    Ray(Vec3 origin_, Vec3 direction_) : origin(origin_), direction(direction_) {
        if (direction == Vec3{}) throw std::invalid_argument("zero ray direction");
    }
    [[nodiscard]] Vec3 at(const Rational& t) const { return origin + t*direction; }
};
} // namespace mcr

