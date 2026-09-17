#include "mcr/camera/pinhole.hpp"
#include <stdexcept>

namespace mcr {
void PinholeCamera::validate() const {
    if (width <= 0 || height <= 0 || fx <= 0 || fy <= 0)
        throw std::invalid_argument("invalid camera intrinsics");
    if (dot(forward,forward)!=1 || dot(right,right)!=1 || dot(up,up)!=1 ||
        dot(forward,right)!=0 || dot(forward,up)!=0 || dot(right,up)!=0 || cross(right,up)!=forward)
        throw std::invalid_argument("camera basis must be right-handed and orthonormal");
}
Ray PinholeCamera::pixel(int column, int row) const {
    validate();
    if (column < 0 || column >= width || row < 0 || row >= height)
        throw std::out_of_range("invalid pixel");
    const auto u = (Rational(column)+Rational(1,2)-cx)/fx;
    const auto v = (Rational(row)+Rational(1,2)-cy)/fy;
    return {center, forward+u*right+v*up};
}
} // namespace mcr

