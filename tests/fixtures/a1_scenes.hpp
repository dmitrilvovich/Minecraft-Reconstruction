#pragma once
#include "a0_scenes.hpp"
#include "mcr/model/a1.hpp"

namespace mcr::test {
struct A1Scene { std::string name; A1World world; };
inline std::vector<A1Scene> a1_adversarial_scenes() {
    std::vector<A1Scene> out;
    for(const auto& old:adversarial_scenes()) {
        A1World world;
        for(auto s:old.world) world.push_back(static_cast<A1State>(s));
        out.push_back({"a0_"+old.name,std::move(world)});
    }
    for(CellId v=0;v<8;++v) {
        A1World world(8,A1State::air); world[v]=A1State::oak_slab;
        out.push_back({"single_slab_"+std::to_string(v),std::move(world)});
    }
    const Grid grid;
    const std::array names{"all_slabs","slab_front_stone_back","slab_front_oak_back",
                           "slab_floor","full_roof_over_slabs","full_slab_checkerboard","horizontal_full_slab_checkerboard"};
    for(unsigned kind=0;kind<names.size();++kind) {
        A1World world(8,A1State::air);
        for(CellId v=0;v<8;++v) {
            const auto p=grid.position(v);
            switch(kind) {
            case 0: world[v]=A1State::oak_slab; break;
            case 1: world[v]=p[0]==0 ? A1State::oak_slab : A1State::stone; break;
            case 2: world[v]=p[0]==0 ? A1State::oak_slab : A1State::oak; break;
            case 3: world[v]=p[1]==0 ? A1State::oak_slab : A1State::air; break;
            case 4: world[v]=p[1]==0 ? A1State::oak_slab : A1State::oak; break;
            case 5: world[v]=(p[0]+p[1]+p[2])%2==0 ? A1State::oak_slab : A1State::oak; break;
            default: world[v]=(p[0]+p[2])%2!=0 ? A1State::oak_slab : A1State::oak; break;
            }
        }
        out.push_back({names[kind],std::move(world)});
    }
    return out;
}
} // namespace mcr::test
