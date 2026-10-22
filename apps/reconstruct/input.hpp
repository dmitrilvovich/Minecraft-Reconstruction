#pragma once
#include "mcr/camera/pinhole.hpp"
#include "mcr/inference/a2_solver.hpp"
#include "picojson.h"
#include <filesystem>
#include <vector>

namespace mcr::app {
using Json = picojson::value;
using Object = picojson::object;
using Array = picojson::array;
struct Input {
    Grid grid;
    PinholeCamera camera; // core convention: row increases along up
    A2Palette palette;
    Json metadata;
};
struct Pixels {
    unsigned width, height;
    std::vector<PixelLabel> labels; // PNG top-to-bottom, left-to-right
};
[[nodiscard]] Input read_metadata(const std::filesystem::path&);
[[nodiscard]] Pixels read_png(const std::filesystem::path&, const Input&);
[[nodiscard]] Ray pixel_ray(const Input&, unsigned column, unsigned png_row);
[[nodiscard]] Json reconstruct(const Input&, const Pixels&);
void write_result(const std::filesystem::path&, const Json&);
} // namespace mcr::app
