#include "mcr/render/a1.hpp"
#include <algorithm>

namespace mcr {
A1AabbReferenceRay::A1AabbReferenceRay(const Grid& grid,const Ray& ray) : cell_count_(grid.size()) {
    for(CellId v=0;v<grid.size();++v) for(auto s:a1_states) {
        if(s==A1State::air) continue;
        auto box=grid.bounds(v);
        if(s==A1State::oak_slab) box.upper[1]=box.lower[1]+Rational(1,2);
        if(const auto hit=intersect(ray,box)) hits_.push_back({hit->enter,v,s});
    }
    std::sort(hits_.begin(),hits_.end(),[](const Hit& a,const Hit& b) {
        if(a.depth!=b.depth) return a.depth<b.depth;
        if(a.cell!=b.cell) return a.cell<b.cell;
        return code(a.state)<code(b.state);
    });
}
PixelLabel A1AabbReferenceRay::sample(std::span<const A1State> world) const {
    validate_world(world,cell_count_);
    for(const auto& hit:hits_) if(world[hit.cell]==hit.state) return material(hit.state);
    return PixelLabel::background;
}
A1TraversalRay::A1TraversalRay(const Grid& grid,const Ray& ray) : cell_count_(grid.size()) {
    for(const auto& visit:traverse(grid,ray)) {
        const Rational bottom(grid.position(visit.cell)[1]);
        const auto half=bottom+Rational(1,2);
        std::vector<Rational> events{visit.enter,visit.leave};
        if(ray.direction[1]!=0) {
            const auto crossing=(half-ray.origin[1])/ray.direction[1];
            if(visit.enter<crossing && crossing<visit.leave) events.insert(events.begin()+1,crossing);
        }
        bool slab_hit=false;
        for(std::size_t j=1;j<events.size();++j) {
            const auto y=ray.origin[1]+((events[j-1]+events[j])/2)*ray.direction[1];
            slab_hit=slab_hit || (bottom<=y && y<half);
        }
        steps_.push_back({visit.cell,slab_hit});
    }
}
PixelLabel A1TraversalRay::sample(std::span<const A1State> world) const {
    validate_world(world,cell_count_);
    for(const auto& step:steps_) {
        const auto label=step.emission(world[step.cell]);
        if(label!=PixelLabel::background) return label;
    }
    return PixelLabel::background;
}
} // namespace mcr
