#include "mcr/grid/grid.hpp"
#include <limits>
#include <stdexcept>

namespace mcr {
Grid::Grid(CellPosition shape) : shape_(shape) {
    for (int n : shape) {
        if (n <= 0) throw std::invalid_argument("grid dimensions must be positive");
        if (size_ > std::numeric_limits<std::size_t>::max()/static_cast<std::size_t>(n))
            throw std::overflow_error("grid cell count overflow");
        size_ *= static_cast<std::size_t>(n);
    }
}
bool Grid::contains(const CellPosition& p) const {
    for (std::size_t j=0; j<3; ++j) if (p[j] < 0 || p[j] >= shape_[j]) return false;
    return true;
}
CellId Grid::id(const CellPosition& p) const {
    if (!contains(p)) throw std::out_of_range("cell outside grid");
    return (static_cast<std::size_t>(p[0])*static_cast<std::size_t>(shape_[1])+
            static_cast<std::size_t>(p[1]))*static_cast<std::size_t>(shape_[2])+static_cast<std::size_t>(p[2]);
}
CellPosition Grid::position(CellId id_) const {
    if (id_ >= size_) throw std::out_of_range("invalid cell ID");
    CellPosition p{};
    for (int j=2; j>=0; --j) {
        const auto k = static_cast<std::size_t>(j);
        p[k] = static_cast<int>(id_%static_cast<std::size_t>(shape_[k]));
        id_ /= static_cast<std::size_t>(shape_[k]);
    }
    return p;
}
Aabb Grid::bounds(CellId v) const {
    const auto p = position(v);
    return {{p[0],p[1],p[2]}, {Rational(p[0])+1,Rational(p[1])+1,Rational(p[2])+1}};
}
} // namespace mcr

