#include "mcr/inference/a1_constraint.hpp"
#include <algorithm>
#include <utility>

namespace mcr {
A1Constraint::A1Constraint(std::vector<A1RayCell> steps,PixelLabel target)
    : steps_(std::move(steps)),target_(target) {
    if(code(target)>2) throw std::invalid_argument("invalid pixel label");
    for(const auto& step:steps_) cells_.push_back(step.cell);
    auto sorted=cells_;
    std::sort(sorted.begin(),sorted.end());
    if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())
        throw std::invalid_argument("a first-hit ray cannot repeat a cell");
}
bool A1Constraint::accepts(std::span<const A1State> world) const {
    // Reject malformed inputs even when an earlier hit would already reject the ray.
    for(const auto& step:steps_) {
        if(step.cell>=world.size()) throw std::invalid_argument("constraint exceeds world size");
        if(code(world[step.cell])>=a1_states.size()) throw std::invalid_argument("invalid A1 state");
    }
    RayMode mode=RayMode::alive;
    for(const auto& step:steps_) {
        const auto next=transition(mode,step.emission(world[step.cell]),target_);
        if(!next) return false;
        mode=*next;
    }
    return mode==(target_==PixelLabel::background ? RayMode::alive : RayMode::done);
}
A1LocalSupport factor_supports(const A1Constraint& factor,std::span<const A1Domain> domains) {
    const auto steps=factor.steps();
    auto masks=detail::automaton_supports(factor.cells(),domains,factor.target(),
        [&](std::size_t j,unsigned s){return steps[j].emission(static_cast<A1State>(s));});
    if(!masks) return {false,{}};
    return {true,std::move(*masks)};
}
} // namespace mcr
