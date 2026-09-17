#include "mcr/model/a0.hpp"
#include <limits>

namespace mcr {
std::uint64_t a0_world_count(std::size_t cells) {
    std::uint64_t result=1;
    for(std::size_t j=0;j<cells;++j) {
        if(result>std::numeric_limits<std::uint64_t>::max()/3) throw std::overflow_error("world count overflow");
        result*=3;
    }
    return result;
}
World a0_world(std::size_t cells,std::uint64_t id) {
    if(id>=a0_world_count(cells)) throw std::out_of_range("world ID out of range");
    World w(cells);
    for(auto& state:w) { state=static_cast<A0State>(id%3); id/=3; }
    return w;
}
void validate_world(std::span<const A0State> w,std::size_t cells) {
    if(w.size()!=cells) throw std::invalid_argument("wrong world size");
    for(auto s:w) if(code(s)>2) throw std::invalid_argument("invalid A0 state");
}
std::uint64_t a0_world_id(std::span<const A0State> w) {
    validate_world(w,w.size());
    (void)a0_world_count(w.size());
    std::uint64_t id=0;
    for(auto it=w.rbegin();it!=w.rend();++it) id=3*id+code(*it);
    return id;
}
bool contains(std::span<const Domain> domains,std::span<const A0State> w) {
    validate_world(w,domains.size());
    for(std::size_t v=0;v<w.size();++v) if(!domains[v].contains(w[v])) return false;
    return true;
}
} // namespace mcr

