#pragma once
#include "mcr/model/domain.hpp"
#include "mcr/model/observation.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace mcr {
enum class A0State : std::uint8_t { air=0, stone=1, oak=2 };
inline constexpr std::array a0_states{A0State::air,A0State::stone,A0State::oak};
inline constexpr unsigned code(A0State state) { return static_cast<unsigned>(state); }
inline PixelLabel emission(A0State state) {
    if (code(state)>2) throw std::invalid_argument("invalid A0 state");
    return static_cast<PixelLabel>(state);
}

using Domain=StateDomain<A0State,3>;
using World = std::vector<A0State>;
using Domains = std::vector<Domain>;
[[nodiscard]] std::uint64_t a0_world_count(std::size_t cells);
[[nodiscard]] World a0_world(std::size_t cells,std::uint64_t id);
[[nodiscard]] std::uint64_t a0_world_id(std::span<const A0State>);
void validate_world(std::span<const A0State>,std::size_t cells);
[[nodiscard]] bool contains(std::span<const Domain>,std::span<const A0State>);
} // namespace mcr
