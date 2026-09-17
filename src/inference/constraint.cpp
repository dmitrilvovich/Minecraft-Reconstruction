#include "mcr/inference/constraint.hpp"
#include <algorithm>
#include <utility>

namespace mcr {
std::optional<RayMode> transition(RayMode mode,A0State state,PixelLabel target) {
    const auto e=emission(state);
    if(code(target)>2) throw std::invalid_argument("invalid pixel label");
    if(mode==RayMode::done) return RayMode::done;
    if(mode!=RayMode::alive) throw std::invalid_argument("invalid automaton mode");
    if(e==PixelLabel::background) return RayMode::alive;
    if(e==target && target!=PixelLabel::background) return RayMode::done;
    return {};
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
    const auto cells=factor.cells();
    const auto m=cells.size();
    std::vector<unsigned> forward(m+1,0),backward(m+1,0);
    forward[0]=1U; // alive
    for(std::size_t j=0;j<m;++j) {
        if(cells[j]>=domains.size()) throw std::invalid_argument("constraint exceeds domain count");
        for(unsigned q=0;q<2;++q) if(forward[j] & (1U<<q))
            for(auto s:a0_states) if(domains[cells[j]].contains(s))
                if(auto next=transition(static_cast<RayMode>(q),s,factor.target()))
                    forward[j+1] |= 1U<<static_cast<unsigned>(*next);
    }
    const unsigned terminal=factor.target()==PixelLabel::background ? 1U : 2U;
    if(!(forward[m] & terminal)) return {false,{}};
    backward[m]=terminal;
    std::vector<Domain> support(m,Domain(0));
    for(std::size_t j=m;j-- >0;) {
        unsigned supported=0;
        for(unsigned q=0;q<2;++q)
            for(auto s:a0_states) if(domains[cells[j]].contains(s)) {
                const auto next=transition(static_cast<RayMode>(q),s,factor.target());
                if(next && (backward[j+1] & (1U<<static_cast<unsigned>(*next)))) {
                    backward[j] |= 1U<<q;
                    if(forward[j] & (1U<<q)) supported |= 1U<<code(s);
                }
            }
        support[j]=Domain(supported);
    }
    return {true,std::move(support)};
}
} // namespace mcr

