#pragma once
#include "mcr/inference/automaton.hpp"
#include "mcr/grid/grid.hpp"
#include "mcr/model/a0.hpp"
#include <optional>

namespace mcr {
[[nodiscard]] std::optional<RayMode> transition(RayMode,A0State,PixelLabel);

class FirstHitConstraint {
public:
    FirstHitConstraint(std::vector<CellId> cells,PixelLabel target);
    [[nodiscard]] std::span<const CellId> cells() const { return cells_; }
    [[nodiscard]] PixelLabel target() const { return target_; }
    [[nodiscard]] bool accepts(std::span<const A0State>) const;
private:
    std::vector<CellId> cells_;
    PixelLabel target_;
};
class Problem {
public:
    Problem(std::size_t cell_count,std::vector<FirstHitConstraint> constraints);
    [[nodiscard]] std::size_t cell_count() const { return cell_count_; }
    [[nodiscard]] std::span<const FirstHitConstraint> constraints() const { return constraints_; }
    [[nodiscard]] std::span<const std::size_t> incident(CellId v) const { return incident_.at(v); }
    [[nodiscard]] bool accepts(std::span<const A0State>) const;
    void validate_domains(std::span<const Domain>) const;
private:
    std::size_t cell_count_;
    std::vector<FirstHitConstraint> constraints_;
    std::vector<std::vector<std::size_t>> incident_;
};
struct LocalSupport {
    bool feasible;
    // One mask per position in the ray, not per scene cell.
    std::vector<Domain> masks;
};
[[nodiscard]] LocalSupport factor_supports(const FirstHitConstraint&,std::span<const Domain>);
} // namespace mcr
