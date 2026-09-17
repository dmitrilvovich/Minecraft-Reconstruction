#pragma once
#include "mcr/geometry/ray.hpp"
#include <optional>

namespace mcr {
struct Aabb {
    Vec3 lower, upper;
    Aabb(Vec3 lo, Vec3 hi);
};
struct HitInterval { Rational enter, leave; };
// Independent interval intersection; does not use grid traversal.
[[nodiscard]] std::optional<HitInterval> intersect(const Ray&, const Aabb&);
} // namespace mcr

