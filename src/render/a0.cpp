#include "mcr/render/a0.hpp"
#include <algorithm>

namespace mcr {
AabbReferenceRay::AabbReferenceRay(const Grid& grid,const Ray& ray) : cell_count_(grid.size()) {
    for(CellId v=0;v<grid.size();++v)
        if(const auto hit=intersect(ray,grid.bounds(v))) hits_.push_back({v,hit->enter});
    std::sort(hits_.begin(),hits_.end(),[](const Hit& a,const Hit& b) {
        return a.depth==b.depth ? a.cell<b.cell : a.depth<b.depth;
    });
}
PixelLabel AabbReferenceRay::sample(std::span<const A0State> w) const {
    validate_world(w,cell_count_);
    for(const auto& hit:hits_) if(w[hit.cell]!=A0State::air) return emission(w[hit.cell]);
    return PixelLabel::background;
}
TraversalRay::TraversalRay(const Grid& grid,const Ray& ray) : cell_count_(grid.size()) {
    for(const auto& visit:traverse(grid,ray)) cells_.push_back(visit.cell);
}
PixelLabel TraversalRay::sample(std::span<const A0State> w) const {
    validate_world(w,cell_count_);
    for(auto v:cells_) {
        const auto label=emission(w[v]);
        if(label!=PixelLabel::background) return label;
    }
    return PixelLabel::background;
}
} // namespace mcr

