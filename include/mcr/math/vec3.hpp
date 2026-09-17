#pragma once
#include "mcr/math/rational.hpp"
#include <array>

namespace mcr {
using Vec3 = std::array<Rational, 3>;
inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}
inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a[0]-b[0], a[1]-b[1], a[2]-b[2]};
}
inline Vec3 operator*(const Rational& k, const Vec3& a) {
    return {k*a[0], k*a[1], k*a[2]};
}
inline Rational dot(const Vec3& a, const Vec3& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
} // namespace mcr

