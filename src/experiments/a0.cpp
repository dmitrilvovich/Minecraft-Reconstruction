#include "mcr/experiments/a0.hpp"
#include <set>

namespace mcr::experiments {
A0Rig::A0Rig() {
    const Grid grid;
    std::map<std::vector<CellId>,std::size_t> interned;
    std::vector<std::vector<std::size_t>> camera_factors;
    for(const auto& camera:phase_a_cameras(grid)) {
        std::vector<std::size_t> indices;
        for(int row=0;row<8;++row) for(int col=0;col<8;++col) {
            const auto ray=camera.pixel(col,row);
            TraversalRay chain_(grid,ray);
            const std::vector<CellId> cells(chain_.cells().begin(),chain_.cells().end());
            const auto [it,inserted]=interned.emplace(cells,rays_.size());
            if(inserted) { rays_.push_back(std::move(chain_)); reference_.emplace_back(grid,ray); }
            indices.push_back(it->second);
        }
        camera_factors.push_back(std::move(indices));
    }
    const auto append_suite=[&](std::string name,unsigned views,std::size_t first,std::size_t end) {
        std::set<std::size_t> indices;
        for(auto j=first;j<end;++j) indices.insert(camera_factors[j].begin(),camera_factors[j].end());
        suites_.push_back({std::move(name),views,{indices.begin(),indices.end()}});
    };
    for(unsigned views=1;views<=6;++views) append_suite("axis_"+std::to_string(views),views,0,views);
    append_suite("oblique_1",1,6,7);
    append_suite("oblique_2",2,6,8);
}
Observation A0Rig::observe_aabb(std::span<const A0State> world) const {
    Observation labels;
    for(const auto& ray:reference_) labels.push_back(ray.sample(world));
    return labels;
}
Problem A0Rig::problem(std::span<const std::size_t> indices,std::span<const PixelLabel> labels) const {
    if(indices.size()!=labels.size()) throw std::invalid_argument("wrong observation count");
    std::vector<FirstHitConstraint> constraints;
    for(std::size_t j=0;j<indices.size();++j) {
        const auto cells=chain(indices[j]);
        constraints.emplace_back(std::vector<CellId>(cells.begin(),cells.end()),labels[j]);
    }
    // Conflicting duplicate ray observations remain separate constraints.
    return Problem(8,std::move(constraints));
}
A0Corpus::A0Corpus(const A0Rig& rig) {
    for(std::uint64_t id=0;id<6561;++id) {
        worlds_.push_back(a0_world(8,id));
        images_.push_back(rig.observe_aabb(worlds_.back()));
    }
}
Observation A0Corpus::observation(std::uint32_t world,std::span<const std::size_t> factors) const {
    Observation out;
    for(auto r:factors) out.push_back(images_.at(world).at(r));
    return out;
}
Families A0Corpus::families(const ViewSuite& suite) const {
    Families result;
    for(std::uint32_t id=0;id<worlds_.size();++id) result[observation(id,suite.factors)].push_back(id);
    return result;
}
} // namespace mcr::experiments

