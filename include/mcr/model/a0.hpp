#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace mcr {
enum class A0State : std::uint8_t { air=0, stone=1, oak=2 };
enum class PixelLabel : std::uint8_t { background=0, stone=1, oak=2 };
inline constexpr std::array a0_states{A0State::air,A0State::stone,A0State::oak};
inline constexpr unsigned code(A0State state) { return static_cast<unsigned>(state); }
inline constexpr unsigned code(PixelLabel label) { return static_cast<unsigned>(label); }
inline PixelLabel emission(A0State state) {
    if (code(state)>2) throw std::invalid_argument("invalid A0 state");
    return static_cast<PixelLabel>(state);
}

class Domain {
public:
    explicit Domain(unsigned bits=7) : bits_(static_cast<std::uint8_t>(bits)) {
        if (bits>7) throw std::invalid_argument("invalid A0 domain");
    }
    static Domain singleton(A0State s) {
        if (code(s)>2) throw std::invalid_argument("invalid A0 state");
        return Domain(1U<<code(s));
    }
    [[nodiscard]] unsigned bits() const { return bits_; }
    [[nodiscard]] unsigned size() const { return static_cast<unsigned>(std::popcount(bits_)); }
    [[nodiscard]] bool empty() const { return bits_==0; }
    [[nodiscard]] bool contains(A0State s) const { return (bits_ & singleton(s).bits_)!=0; }
    [[nodiscard]] Domain occupied() const { return Domain(bits_ & 6U); }
    friend Domain operator&(Domain a,Domain b) { return Domain(a.bits_ & b.bits_); }
    friend bool operator==(Domain,Domain) = default;
private:
    std::uint8_t bits_;
};
using World = std::vector<A0State>;
using Domains = std::vector<Domain>;
[[nodiscard]] std::uint64_t a0_world_count(std::size_t cells);
[[nodiscard]] World a0_world(std::size_t cells,std::uint64_t id);
[[nodiscard]] std::uint64_t a0_world_id(std::span<const A0State>);
void validate_world(std::span<const A0State>,std::size_t cells);
[[nodiscard]] bool contains(std::span<const Domain>,std::span<const A0State>);
} // namespace mcr

