#pragma once
#include <cstdint>

namespace mcr {
enum class PixelLabel : std::uint8_t { background=0, stone=1, oak=2 };
inline constexpr unsigned code(PixelLabel label) { return static_cast<unsigned>(label); }
} // namespace mcr
