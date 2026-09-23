#include "mcr/model/a2.hpp"
#include <limits>
#include <stdexcept>

namespace mcr {
void validate_palette(A2Palette palette) {
    if(palette!=A2Palette::split_material && palette!=A2Palette::same_material)
        throw std::invalid_argument("invalid A2 palette");
}
PixelLabel material(A2State s,A2Palette palette) {
    validate_palette(palette);
    switch(s) {
    case A2State::air: return PixelLabel::background;
    case A2State::bottom_slab: return PixelLabel::oak;
    case A2State::top_slab:
        return palette==A2Palette::split_material ? PixelLabel::stone : PixelLabel::oak;
    }
    throw std::invalid_argument("invalid A2 state");
}
std::uint64_t a2_world_count(std::size_t cells) {
    std::uint64_t count=1;
    for(std::size_t v=0;v<cells;++v) {
        if(count>std::numeric_limits<std::uint64_t>::max()/3)
            throw std::overflow_error("A2 world count exceeds uint64 range");
        count*=3;
    }
    return count;
}
A2World a2_world(std::size_t cells,std::uint64_t id) {
    if(id>=a2_world_count(cells)) throw std::out_of_range("A2 world ID out of range");
    A2World world(cells);
    for(auto& s:world) { s=static_cast<A2State>(id%3); id/=3; }
    return world;
}
void validate_world(std::span<const A2State> world,std::size_t cells) {
    if(world.size()!=cells) throw std::invalid_argument("wrong A2 world size");
    for(auto s:world) if(code(s)>=a2_states.size()) throw std::invalid_argument("invalid A2 state");
}
std::uint64_t a2_world_id(std::span<const A2State> world) {
    validate_world(world,world.size());
    (void)a2_world_count(world.size());
    std::uint64_t id=0;
    for(auto it=world.rbegin();it!=world.rend();++it) id=3*id+code(*it);
    return id;
}
bool contains(std::span<const A2Domain> domains,std::span<const A2State> world) {
    validate_world(world,domains.size());
    for(std::size_t v=0;v<world.size();++v) if(!domains[v].contains(world[v])) return false;
    return true;
}
} // namespace mcr
