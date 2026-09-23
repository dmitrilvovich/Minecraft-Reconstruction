#include "mcr/render/a2.hpp"
#include <algorithm>

namespace mcr {
A2AabbReferenceRay::A2AabbReferenceRay(const Grid& grid,const Ray& ray,A2Palette palette)
    : cell_count_(grid.size()),palette_(palette) {
    validate_palette(palette);
    for(CellId v=0;v<grid.size();++v) for(auto s:{A2State::bottom_slab,A2State::top_slab}) {
        auto box=grid.bounds(v);
        const auto half=box.lower[1]+Rational(1,2);
        if(s==A2State::bottom_slab) box.upper[1]=half;
        else box.lower[1]=half;
        if(const auto hit=intersect(ray,box)) hits_.push_back({hit->enter,v,s});
    }
    std::sort(hits_.begin(),hits_.end(),[](const Hit& a,const Hit& b) {
        if(a.depth!=b.depth) return a.depth<b.depth;
        if(a.cell!=b.cell) return a.cell<b.cell;
        return code(a.state)<code(b.state);
    });
}
PixelLabel A2AabbReferenceRay::sample(std::span<const A2State> world) const {
    validate_world(world,cell_count_);
    for(const auto& hit:hits_) if(world[hit.cell]==hit.state) return material(hit.state,palette_);
    return PixelLabel::background;
}
A2TraversalRay::A2TraversalRay(const Grid& grid,const Ray& ray,A2Palette palette)
    : cell_count_(grid.size()),palette_(palette) {
    validate_palette(palette);
    // Independent of ray/AABB intersection: split each positive cell interval
    // at its half-height event and classify interior midpoints.
    for(const auto& visit:traverse(grid,ray)) {
        const auto half=Rational(grid.position(visit.cell)[1])+Rational(1,2);
        std::vector<Rational> events{visit.enter,visit.leave};
        if(ray.direction[1]!=0) {
            const auto crossing=(half-ray.origin[1])/ray.direction[1];
            if(visit.enter<crossing && crossing<visit.leave) events.insert(events.begin()+1,crossing);
        }
        bool bottom=false,top=false;
        for(std::size_t j=1;j<events.size();++j) {
            const auto y=ray.origin[1]+((events[j-1]+events[j])/2)*ray.direction[1];
            bottom=bottom || y<half;
            top=top || y>=half;
        }
        steps_.push_back({visit.cell,bottom,top});
    }
}
PixelLabel A2TraversalRay::sample(std::span<const A2State> world) const {
    validate_world(world,cell_count_);
    for(const auto& step:steps_) {
        const auto label=step.emission(world[step.cell],palette_);
        if(label!=PixelLabel::background) return label;
    }
    return PixelLabel::background;
}
} // namespace mcr
