#include "mcr/experiments/a1.hpp"
#include <set>
#include <utility>

namespace mcr::experiments {
A1Rig::A1Rig() {
    const Grid grid;
    std::map<std::vector<A1RayCell>,std::size_t> interned;
    std::vector<std::vector<std::size_t>> camera_factors;
    for(const auto& camera:phase_a_cameras(grid)) {
        std::vector<std::size_t> indices;
        for(int row=0;row<8;++row) for(int col=0;col<8;++col) {
            const auto ray=camera.pixel(col,row);
            A1TraversalRay program(grid,ray);
            std::vector<A1RayCell> steps_(program.steps().begin(),program.steps().end());
            const auto [it,inserted]=interned.emplace(steps_,rays_.size());
            if(inserted) { rays_.push_back(std::move(program)); reference_.emplace_back(grid,ray); }
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
Observation A1Rig::observe_aabb(std::span<const A1State> world) const {
    Observation out; out.reserve(reference_.size());
    for(const auto& ray:reference_) out.push_back(ray.sample(world));
    return out;
}
A1Problem A1Rig::problem(std::span<const std::size_t> indices,std::span<const PixelLabel> labels) const {
    if(indices.size()!=labels.size()) throw std::invalid_argument("wrong observation count");
    std::vector<A1Constraint> constraints;
    for(std::size_t j=0;j<indices.size();++j) {
        const auto program=steps(indices[j]);
        constraints.emplace_back(std::vector<A1RayCell>(program.begin(),program.end()),labels[j]);
    }
    return A1Problem(8,std::move(constraints));
}
A1Corpus::A1Corpus(const A1Rig& rig) {
    worlds_.reserve(65536); images_.reserve(65536);
    for(std::uint64_t id=0;id<65536;++id) {
        worlds_.push_back(a1_world(8,id));
        images_.push_back(rig.observe_aabb(worlds_.back()));
    }
}
Observation A1Corpus::observation(std::uint32_t id,std::span<const std::size_t> indices) const {
    Observation out; out.reserve(indices.size());
    for(auto r:indices) out.push_back(images_.at(id).at(r));
    return out;
}
bool A1Corpus::matches(std::uint32_t id,std::span<const std::size_t> indices,std::span<const PixelLabel> labels) const {
    if(indices.size()!=labels.size()) throw std::invalid_argument("wrong observation count");
    const auto& image=images_.at(id);
    for(std::size_t j=0;j<indices.size();++j) if(image.at(indices[j])!=labels[j]) return false;
    return true;
}
Families A1Corpus::families(const ViewSuite& suite) const {
    Families result;
    for(std::uint32_t id=0;id<worlds_.size();++id) result[observation(id,suite.factors)].push_back(id);
    return result;
}
} // namespace mcr::experiments
