#pragma once
#include "mcr/model/domain.hpp"
#include "mcr/model/observation.hpp"
#include <array>
#include <span>
#include <vector>

namespace mcr {
// Geometry codes are shared by two experimental palettes. A state plus its
// palette specifies a block; changing palette changes only the top material.
enum class A2State : std::uint8_t { air=0, bottom_slab=1, top_slab=2 };
enum class A2Palette { split_material, same_material };
inline constexpr std::array a2_states{A2State::air,A2State::bottom_slab,A2State::top_slab};
inline constexpr unsigned code(A2State s) { return static_cast<unsigned>(s); }
using A2Domain=StateDomain<A2State,3>;
using A2Domains=std::vector<A2Domain>;
using A2World=std::vector<A2State>;

void validate_palette(A2Palette);
[[nodiscard]] PixelLabel material(A2State,A2Palette);
[[nodiscard]] std::uint64_t a2_world_count(std::size_t cells);
[[nodiscard]] A2World a2_world(std::size_t cells,std::uint64_t id);
[[nodiscard]] std::uint64_t a2_world_id(std::span<const A2State>);
void validate_world(std::span<const A2State>,std::size_t cells);
[[nodiscard]] bool contains(std::span<const A2Domain>,std::span<const A2State>);
} // namespace mcr
