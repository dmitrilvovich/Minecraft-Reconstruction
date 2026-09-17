#pragma once
#include "mcr/grid/traversal.hpp"
#include "mcr/model/a0.hpp"

namespace mcr {
// Reference path: intersects every cell independently and orders exact depths.
class AabbReferenceRay {
public:
    AabbReferenceRay(const Grid&,const Ray&);
    [[nodiscard]] PixelLabel sample(std::span<const A0State>) const;
private:
    struct Hit { CellId cell; Rational depth; };
    std::size_t cell_count_;
    std::vector<Hit> hits_;
};

// Production path: compiled from independent grid-plane traversal.
class TraversalRay {
public:
    TraversalRay(const Grid&,const Ray&);
    [[nodiscard]] PixelLabel sample(std::span<const A0State>) const;
    [[nodiscard]] std::span<const CellId> cells() const { return cells_; }
private:
    std::size_t cell_count_;
    std::vector<CellId> cells_;
};
} // namespace mcr

