#pragma once
#include "mcr/model/domain.hpp"
#include "mcr/model/observation.hpp"
#include <array>
#include <span>
#include <vector>

namespace mcr {
enum class A1State : std::uint8_t { air=0, stone=1, oak=2, oak_slab=3 };
inline constexpr std::array a1_states{A1State::air,A1State::stone,A1State::oak,A1State::oak_slab};
inline constexpr unsigned code(A1State state) { return static_cast<unsigned>(state); }
using A1Domain=StateDomain<A1State,4>;
using A1Domains=std::vector<A1Domain>;
using A1World=std::vector<A1State>;

[[nodiscard]] PixelLabel material(A1State);
[[nodiscard]] unsigned geometry_rank(A1State);
[[nodiscard]] A1State maximal_geometry(A1Domain);
[[nodiscard]] std::uint64_t a1_world_count(std::size_t cells);
[[nodiscard]] A1World a1_world(std::size_t cells,std::uint64_t id);
[[nodiscard]] std::uint64_t a1_world_id(std::span<const A1State>);
void validate_world(std::span<const A1State>,std::size_t cells);
[[nodiscard]] bool contains(std::span<const A1Domain>,std::span<const A1State>);
} // namespace mcr
