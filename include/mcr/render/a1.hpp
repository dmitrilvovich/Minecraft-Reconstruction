#pragma once
#include "mcr/grid/traversal.hpp"
#include "mcr/model/a1.hpp"

namespace mcr {
// Full cubes always intersect a visited cell interval; only the slab can miss.
struct A1RayCell {
    CellId cell;
    bool slab_hit;
    [[nodiscard]] PixelLabel emission(A1State s) const {
        if(s==A1State::oak_slab && !slab_hit) return PixelLabel::background;
        return material(s);
    }
    friend auto operator<=>(const A1RayCell&,const A1RayCell&)=default;
};

class A1AabbReferenceRay {
public:
    A1AabbReferenceRay(const Grid&,const Ray&);
    [[nodiscard]] PixelLabel sample(std::span<const A1State>) const;
private:
    struct Hit { Rational depth; CellId cell; A1State state; };
    std::size_t cell_count_;
    std::vector<Hit> hits_;
};
class A1TraversalRay {
public:
    A1TraversalRay(const Grid&,const Ray&);
    [[nodiscard]] PixelLabel sample(std::span<const A1State>) const;
    [[nodiscard]] std::span<const A1RayCell> steps() const { return steps_; }
private:
    std::size_t cell_count_;
    std::vector<A1RayCell> steps_;
};
} // namespace mcr
