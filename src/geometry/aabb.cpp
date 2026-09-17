#include "mcr/geometry/aabb.hpp"
#include <algorithm>
#include <stdexcept>

namespace mcr {
Aabb::Aabb(Vec3 lo, Vec3 hi) : lower(lo), upper(hi) {
    for (std::size_t j=0; j<3; ++j)
        if (lower[j] >= upper[j]) throw std::invalid_argument("empty AABB");
}
std::optional<HitInterval> intersect(const Ray& ray, const Aabb& box) {
    Rational enter{0};
    std::optional<Rational> leave;
    for (std::size_t j=0; j<3; ++j) {
        if (ray.direction[j] == 0) {
            if (ray.origin[j] < box.lower[j] || ray.origin[j] >= box.upper[j]) return {};
        } else {
            auto a = (box.lower[j]-ray.origin[j])/ray.direction[j];
            auto b = (box.upper[j]-ray.origin[j])/ray.direction[j];
            if (b < a) std::swap(a,b);
            enter = std::max(enter,a);
            leave = leave ? std::min(*leave,b) : b;
        }
    }
    if (!leave || enter >= *leave) return {};
    return HitInterval{enter,*leave};
}
} // namespace mcr

