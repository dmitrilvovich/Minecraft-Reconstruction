#include "mcr/inference/constraint.hpp"
#include <algorithm>
#include <utility>

namespace mcr {
std::optional<RayMode> transition(RayMode mode,A0State state,PixelLabel target) {
    return transition(mode,emission(state),target);
}
FirstHitConstraint::FirstHitConstraint(std::vector<CellId> cells,PixelLabel target)
    : cells_(std::move(cells)),target_(target) {
    if(code(target)>2) throw std::invalid_argument("invalid pixel label");
    auto sorted=cells_;
    std::sort(sorted.begin(),sorted.end());
    if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())
        throw std::invalid_argument("a first-hit ray cannot repeat a cell");
}
bool FirstHitConstraint::accepts(std::span<const A0State> world) const {
    RayMode mode=RayMode::alive;
    for(auto v:cells_) {
        if(v>=world.size()) throw std::invalid_argument("constraint exceeds world size");
        const auto next=transition(mode,world[v],target_);
        if(!next) return false;
        mode=*next;
    }
    return mode==(target_==PixelLabel::background ? RayMode::alive : RayMode::done);
}
Problem::Problem(std::size_t n,std::vector<FirstHitConstraint> constraints)
    : cell_count_(n),constraints_(std::move(constraints)),incident_(n) {
    for(std::size_t r=0;r<constraints_.size();++r)
        for(auto v:constraints_[r].cells()) {
            if(v>=n) throw std::invalid_argument("constraint references invalid cell");
            incident_[v].push_back(r);
        }
}
bool Problem::accepts(std::span<const A0State> world) const {
    validate_world(world,cell_count_);
    return std::all_of(constraints_.begin(),constraints_.end(),[&](const auto& f){return f.accepts(world);});
}
void Problem::validate_domains(std::span<const Domain> domains) const {
    if(domains.size()!=cell_count_) throw std::invalid_argument("wrong domain count");
}
LocalSupport factor_supports(const FirstHitConstraint& factor,std::span<const Domain> domains) {
    auto masks=detail::automaton_supports(factor.cells(),domains,factor.target(),
        [](std::size_t,unsigned s){return emission(static_cast<A0State>(s));});
    if(!masks) return {false,{}};
    return {true,std::move(*masks)};
}
} // namespace mcr
