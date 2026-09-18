#include "mcr/inference/a1_problem.hpp"
#include <algorithm>
#include <utility>

namespace mcr {
A1Problem::A1Problem(std::size_t n,std::vector<A1Constraint> constraints)
    : cell_count_(n),constraints_(std::move(constraints)),incident_(n) {
    for(std::size_t r=0;r<constraints_.size();++r) for(auto v:constraints_[r].cells()) {
        if(v>=n) throw std::invalid_argument("constraint references invalid cell");
        incident_[v].push_back(r);
    }
}
bool A1Problem::accepts(std::span<const A1State> world) const {
    validate_world(world,cell_count_);
    return std::all_of(constraints_.begin(),constraints_.end(),[&](const auto& f){return f.accepts(world);});
}
void A1Problem::validate_domains(std::span<const A1Domain> domains) const {
    if(domains.size()!=cell_count_) throw std::invalid_argument("wrong A1 domain count");
}
} // namespace mcr
