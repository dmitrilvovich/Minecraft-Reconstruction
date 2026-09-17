#pragma once
#include "mcr/grid/grid.hpp"
#include <vector>

namespace mcr {
struct CellVisit { CellId cell; Rational enter, leave; };
// Grid-plane events plus interval midpoints; never calls ray/AABB intersection.
[[nodiscard]] std::vector<CellVisit> traverse(const Grid&, const Ray&);
} // namespace mcr

