#include "mcr/model/a1.hpp"
#include <stdexcept>

namespace mcr {
PixelLabel material(A1State s) {
    switch(s) {
    case A1State::air: return PixelLabel::background;
    case A1State::stone: return PixelLabel::stone;
    case A1State::oak:
    case A1State::oak_slab: return PixelLabel::oak;
    }
    throw std::invalid_argument("invalid A1 state");
}
unsigned geometry_rank(A1State s) {
    if(code(s)>3) throw std::invalid_argument("invalid A1 state");
    constexpr std::array<unsigned,4> ranks{0,2,2,1};
    return ranks[code(s)];
}
A1State maximal_geometry(A1Domain domain) {
    // Descending containment rank, then ascending state code for equal geometry.
    for(auto s:{A1State::stone,A1State::oak,A1State::oak_slab,A1State::air})
        if(domain.contains(s)) return s;
    throw std::invalid_argument("empty A1 domain has no geometry representative");
}
std::uint64_t a1_world_count(std::size_t cells) {
    if(cells>=32) throw std::overflow_error("A1 world count exceeds uint64 range");
    return std::uint64_t{1}<<(2*cells);
}
A1World a1_world(std::size_t cells,std::uint64_t id) {
    if(id>=a1_world_count(cells)) throw std::out_of_range("A1 world ID out of range");
    A1World world(cells);
    for(auto& s:world) { s=static_cast<A1State>(id & 3U); id>>=2; }
    return world;
}
void validate_world(std::span<const A1State> world,std::size_t cells) {
    if(world.size()!=cells) throw std::invalid_argument("wrong A1 world size");
    for(auto s:world) if(code(s)>3) throw std::invalid_argument("invalid A1 state");
}
std::uint64_t a1_world_id(std::span<const A1State> world) {
    validate_world(world,world.size());
    (void)a1_world_count(world.size());
    std::uint64_t id=0;
    for(auto it=world.rbegin();it!=world.rend();++it) id=(id<<2) | code(*it);
    return id;
}
bool contains(std::span<const A1Domain> domains,std::span<const A1State> world) {
    validate_world(world,domains.size());
    for(std::size_t v=0;v<world.size();++v) if(!domains[v].contains(world[v])) return false;
    return true;
}
} // namespace mcr
