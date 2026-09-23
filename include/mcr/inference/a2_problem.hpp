#pragma once
#include "mcr/inference/automaton.hpp"
#include "mcr/render/a2.hpp"

namespace mcr {
class A2Constraint {
public:
    A2Constraint(std::vector<A2RayCell>,PixelLabel,A2Palette = A2Palette::split_material);
    [[nodiscard]] std::span<const CellId> cells() const { return cells_; }
    [[nodiscard]] std::span<const A2RayCell> steps() const { return steps_; }
    [[nodiscard]] PixelLabel target() const { return target_; }
    [[nodiscard]] A2Palette palette() const { return palette_; }
    [[nodiscard]] bool accepts(std::span<const A2State>) const;
private:
    std::vector<A2RayCell> steps_;
    std::vector<CellId> cells_;
    PixelLabel target_;
    A2Palette palette_;
};
struct A2LocalSupport { bool feasible; A2Domains masks; };
[[nodiscard]] A2LocalSupport factor_supports(const A2Constraint&,std::span<const A2Domain>);

class A2Problem {
public:
    A2Problem(std::size_t cell_count,std::vector<A2Constraint>,A2Palette = A2Palette::split_material);
    [[nodiscard]] std::size_t cell_count() const { return cell_count_; }
    [[nodiscard]] A2Palette palette() const { return palette_; }
    [[nodiscard]] std::span<const A2Constraint> constraints() const { return constraints_; }
    [[nodiscard]] std::span<const std::size_t> incident(CellId v) const { return incident_.at(v); }
    [[nodiscard]] bool accepts(std::span<const A2State>) const;
    void validate_domains(std::span<const A2Domain>) const;
private:
    std::size_t cell_count_;
    A2Palette palette_;
    std::vector<A2Constraint> constraints_;
    std::vector<std::vector<std::size_t>> incident_;
};
} // namespace mcr
