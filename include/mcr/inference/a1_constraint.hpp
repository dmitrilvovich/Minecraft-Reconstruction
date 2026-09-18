#pragma once
#include "mcr/inference/automaton.hpp"
#include "mcr/render/a1.hpp"

namespace mcr {
class A1Constraint {
public:
    A1Constraint(std::vector<A1RayCell> steps,PixelLabel target);
    [[nodiscard]] std::span<const CellId> cells() const { return cells_; }
    [[nodiscard]] std::span<const A1RayCell> steps() const { return steps_; }
    [[nodiscard]] PixelLabel target() const { return target_; }
    [[nodiscard]] bool accepts(std::span<const A1State>) const;
private:
    std::vector<A1RayCell> steps_;
    std::vector<CellId> cells_;
    PixelLabel target_;
};
// Masks follow ray positions. Only cells in this factor are considered.
// Infeasible results have no masks; feasible empty rays have an empty vector.
struct A1LocalSupport { bool feasible; A1Domains masks; };
[[nodiscard]] A1LocalSupport factor_supports(const A1Constraint&,std::span<const A1Domain>);
} // namespace mcr
