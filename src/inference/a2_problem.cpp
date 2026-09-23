#include "mcr/inference/a2_problem.hpp"
#include <algorithm>
#include <utility>

namespace mcr {
A2Constraint::A2Constraint(std::vector<A2RayCell> steps,PixelLabel target,A2Palette palette)
    : steps_(std::move(steps)),target_(target),palette_(palette) {
    validate_palette(palette);
    if(code(target)>2) throw std::invalid_argument("invalid pixel label");
    for(const auto& step:steps_) cells_.push_back(step.cell);
    auto sorted=cells_;
    std::sort(sorted.begin(),sorted.end());
    if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())
        throw std::invalid_argument("a first-hit ray cannot repeat a cell");
}
bool A2Constraint::accepts(std::span<const A2State> world) const {
    for(auto v:cells_) {
        if(v>=world.size()) throw std::invalid_argument("constraint exceeds world size");
        if(code(world[v])>=a2_states.size()) throw std::invalid_argument("invalid A2 state");
    }
    RayMode mode=RayMode::alive;
    for(const auto& step:steps_) {
        const auto next=transition(mode,step.emission(world[step.cell],palette_),target_);
        if(!next) return false;
        mode=*next;
    }
    return mode==(target_==PixelLabel::background ? RayMode::alive : RayMode::done);
}
A2LocalSupport factor_supports(const A2Constraint& factor,std::span<const A2Domain> domains) {
    const auto steps=factor.steps();
    auto masks=detail::automaton_supports(factor.cells(),domains,factor.target(),
        [&](std::size_t j,unsigned s){return steps[j].emission(static_cast<A2State>(s),factor.palette());});
    if(!masks) return {false,{}};
    return {true,std::move(*masks)};
}
A2Problem::A2Problem(std::size_t n,std::vector<A2Constraint> constraints,A2Palette palette)
    : cell_count_(n),palette_(palette),constraints_(std::move(constraints)),incident_(n) {
    validate_palette(palette);
    for(std::size_t r=0;r<constraints_.size();++r) {
        if(constraints_[r].palette()!=palette) throw std::invalid_argument("mixed A2 palettes in one problem");
        for(auto v:constraints_[r].cells()) {
            if(v>=n) throw std::invalid_argument("constraint references invalid cell");
            incident_[v].push_back(r);
        }
    }
}
bool A2Problem::accepts(std::span<const A2State> world) const {
    validate_world(world,cell_count_);
    return std::all_of(constraints_.begin(),constraints_.end(),[&](const auto& f){return f.accepts(world);});
}
void A2Problem::validate_domains(std::span<const A2Domain> domains) const {
    if(domains.size()!=cell_count_) throw std::invalid_argument("wrong A2 domain count");
}
} // namespace mcr
