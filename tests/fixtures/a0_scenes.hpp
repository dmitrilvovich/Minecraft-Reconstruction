#pragma once
#include "mcr/grid/grid.hpp"
#include "mcr/model/a0.hpp"
#include <string>

namespace mcr::test {
struct Scene { std::string name; World world; };
inline std::vector<Scene> adversarial_scenes() {
    std::vector<Scene> out{
        {"empty",World(8,A0State::air)}, {"solid_stone",World(8,A0State::stone)},
        {"solid_oak",World(8,A0State::oak)}
    };
    const Grid grid;
    for(unsigned kind=0;kind<5;++kind) {
        const std::array names{"stone_front_oak_back","front_sheet","back_sheet","checkerboard","open_corridor"};
        World world(8,A0State::air);
        for(CellId v=0;v<8;++v) {
            const auto p=grid.position(v);
            switch(kind) {
            case 0: world[v]=p[0]==0 ? A0State::stone : A0State::oak; break;
            case 1: world[v]=p[0]==0 ? A0State::stone : A0State::air; break;
            case 2: world[v]=p[0]==1 ? A0State::stone : A0State::air; break;
            case 3: world[v]=(p[0]+p[1]+p[2])%2==0 ? A0State::stone : A0State::oak; break;
            default: world[v]=p[1]==0 && p[2]==0 ? A0State::air : A0State::stone; break;
            }
        }
        out.push_back({names[kind],std::move(world)});
    }
    for(CellId v=0;v<8;++v) for(auto s:{A0State::stone,A0State::oak}) {
        World world(8,A0State::air); world[v]=s;
        out.push_back({"single_"+std::to_string(v)+"_"+std::to_string(code(s)),std::move(world)});
    }
    return out;
}
} // namespace mcr::test

