#pragma once
#include "mcr/camera/pinhole.hpp"
#include "mcr/grid/grid.hpp"
#include <vector>

namespace mcr::experiments {
[[nodiscard]] std::vector<PinholeCamera> phase_a_cameras(const Grid&, int resolution = 8);
} // namespace mcr::experiments

