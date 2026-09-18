#pragma once
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace mcr {
// Distinct instantiations keep A0's three-state domains separate from A1.
template<class State,unsigned StateCount> class StateDomain {
    static_assert(StateCount>0 && StateCount<=8);
public:
    using state_type=State;
    static constexpr unsigned state_count=StateCount;
    static constexpr unsigned full_mask=(1U<<StateCount)-1U;
    explicit StateDomain(unsigned bits=full_mask) : bits_(static_cast<std::uint8_t>(bits)) {
        if(bits>full_mask) throw std::invalid_argument("invalid state domain");
    }
    static StateDomain singleton(State state) {
        const auto s=static_cast<unsigned>(state);
        if(s>=StateCount) throw std::invalid_argument("invalid state");
        return StateDomain(1U<<s);
    }
    [[nodiscard]] unsigned bits() const { return bits_; }
    [[nodiscard]] unsigned size() const { return static_cast<unsigned>(std::popcount(bits_)); }
    [[nodiscard]] bool empty() const { return bits_==0; }
    [[nodiscard]] bool contains(State s) const { return (bits_ & singleton(s).bits_)!=0; }
    [[nodiscard]] StateDomain occupied() const { return StateDomain(bits_ & (full_mask ^ 1U)); }
    friend StateDomain operator&(StateDomain a,StateDomain b) { return StateDomain(a.bits_ & b.bits_); }
    friend bool operator==(StateDomain,StateDomain)=default;
private:
    std::uint8_t bits_;
};
} // namespace mcr
