#pragma once
#include "mcr/grid/traversal.hpp"
#include "mcr/model/a2.hpp"

namespace mcr {
// Both halves may hit a ray, but they are alternative states, never a full cube.
struct A2RayCell {
    CellId cell;
    bool bottom_hit,top_hit;
    [[nodiscard]] PixelLabel emission(A2State s,A2Palette palette) const {
        const auto label=material(s,palette);
        if((s==A2State::bottom_slab && !bottom_hit) ||
           (s==A2State::top_slab && !top_hit)) return PixelLabel::background;
        return label;
    }
    friend auto operator<=>(const A2RayCell&,const A2RayCell&)=default;
};
class A2AabbReferenceRay {
public:
    A2AabbReferenceRay(const Grid&,const Ray&,A2Palette = A2Palette::split_material);
    [[nodiscard]] PixelLabel sample(std::span<const A2State>) const;
private:
    struct Hit { Rational depth; CellId cell; A2State state; };
    std::size_t cell_count_;
    A2Palette palette_;
    std::vector<Hit> hits_;
};
class A2TraversalRay {
public:
    A2TraversalRay(const Grid&,const Ray&,A2Palette = A2Palette::split_material);
    [[nodiscard]] PixelLabel sample(std::span<const A2State>) const;
    [[nodiscard]] std::span<const A2RayCell> steps() const { return steps_; }
private:
    std::size_t cell_count_;
    A2Palette palette_;
    std::vector<A2RayCell> steps_;
};
} // namespace mcr
