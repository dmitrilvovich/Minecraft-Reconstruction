#pragma once
#include "mcr/grid/grid.hpp"
#include "mcr/model/observation.hpp"
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace mcr {
enum class RayMode : unsigned { alive=0, done=1 };
// Input is an observed emission, not a state code: an A1 slab can miss.
[[nodiscard]] std::optional<RayMode> transition(RayMode,PixelLabel emission,PixelLabel target);

namespace detail {
template<class DomainType,class Emit>
std::optional<std::vector<DomainType>> automaton_supports(
        std::span<const CellId> cells,std::span<const DomainType> domains,
        PixelLabel target,Emit emit) {
    const auto m=cells.size();
    std::vector<unsigned> forward(m+1,0),backward(m+1,0);
    forward[0]=1U;
    for(std::size_t j=0;j<m;++j) {
        if(cells[j]>=domains.size()) throw std::invalid_argument("constraint exceeds domain count");
        for(unsigned q=0;q<2;++q) if(forward[j] & (1U<<q))
            for(unsigned s=0;s<DomainType::state_count;++s) if(domains[cells[j]].bits() & (1U<<s))
                if(auto next=transition(static_cast<RayMode>(q),emit(j,s),target))
                    forward[j+1] |= 1U<<static_cast<unsigned>(*next);
    }
    const unsigned terminal=target==PixelLabel::background ? 1U : 2U;
    if(!(forward[m] & terminal)) return {};
    backward[m]=terminal;
    std::vector<DomainType> support(m,DomainType(0));
    for(std::size_t j=m;j-- >0;) {
        unsigned supported=0;
        for(unsigned q=0;q<2;++q)
            for(unsigned s=0;s<DomainType::state_count;++s) if(domains[cells[j]].bits() & (1U<<s)) {
                const auto next=transition(static_cast<RayMode>(q),emit(j,s),target);
                if(next && (backward[j+1] & (1U<<static_cast<unsigned>(*next)))) {
                    backward[j] |= 1U<<q;
                    if(forward[j] & (1U<<q)) supported |= 1U<<s;
                }
            }
        support[j]=DomainType(supported);
    }
    return support;
}
} // namespace detail
} // namespace mcr
