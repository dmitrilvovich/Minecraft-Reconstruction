#pragma once
#include "mcr/inference/a1_constraint.hpp"

namespace mcr {
class A1Problem {
public:
    A1Problem(std::size_t cell_count,std::vector<A1Constraint> constraints);
    [[nodiscard]] std::size_t cell_count() const { return cell_count_; }
    [[nodiscard]] std::span<const A1Constraint> constraints() const { return constraints_; }
    [[nodiscard]] std::span<const std::size_t> incident(CellId v) const { return incident_.at(v); }
    [[nodiscard]] bool accepts(std::span<const A1State>) const;
    void validate_domains(std::span<const A1Domain>) const;
private:
    std::size_t cell_count_;
    std::vector<A1Constraint> constraints_;
    std::vector<std::vector<std::size_t>> incident_;
};
} // namespace mcr
