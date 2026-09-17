#pragma once
#include "mcr/geometry/aabb.hpp"
#include <array>
#include <cstddef>

namespace mcr {
using CellId = std::size_t;
using CellPosition = std::array<int, 3>;
class Grid {
public:
    explicit Grid(CellPosition shape = {2,2,2});
    [[nodiscard]] CellPosition shape() const { return shape_; }
    [[nodiscard]] std::size_t size() const { return size_; }
    [[nodiscard]] bool contains(const CellPosition&) const;
    [[nodiscard]] CellId id(const CellPosition&) const;
    [[nodiscard]] CellPosition position(CellId) const;
    [[nodiscard]] Aabb bounds(CellId) const;
private:
    CellPosition shape_;
    std::size_t size_ = 1;
};
} // namespace mcr

