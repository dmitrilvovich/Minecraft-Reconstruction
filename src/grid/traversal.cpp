#include "mcr/grid/traversal.hpp"
#include <algorithm>

namespace mcr {
std::vector<CellVisit> traverse(const Grid& grid, const Ray& ray) {
    std::vector<Rational> events{0};
    const auto shape = grid.shape();
    for (std::size_t j=0; j<3; ++j) {
        if (ray.direction[j] == 0) continue;
        for (std::int64_t plane=0; plane<=shape[j]; ++plane) {
            const auto t = (Rational(plane)-ray.origin[j])/ray.direction[j];
            if (t > 0) events.push_back(t);
        }
    }
    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end()), events.end());
    std::vector<CellVisit> visits;
    for (std::size_t k=1; k<events.size(); ++k) {
        const auto point = ray.at((events[k-1]+events[k])/2);
        CellPosition p{};
        bool inside = true;
        for (std::size_t j=0; j<3; ++j) {
            const auto coordinate = point[j].floor();
            if (coordinate < 0 || coordinate >= shape[j]) { inside=false; break; }
            p[j] = static_cast<int>(coordinate);
        }
        if (inside) visits.push_back({grid.id(p),events[k-1],events[k]});
    }
    return visits;
}
} // namespace mcr

