#include "mcr/inference/a2_solver.hpp"
#include "gac_impl.hpp"
#include <algorithm>
#include <bit>
#include <numeric>
#include <utility>

namespace mcr {
A2PropagationResult gac(const A2Problem& problem,A2Domains domains,InferenceStats& stats,A2InferenceObserver* observer) {
    return detail::propagate_gac<A2PropagationResult>(problem,std::move(domains),stats,observer);
}
namespace {
struct ResidualFactor {
    std::size_t factor;
    std::vector<CellId> cells;
};
struct Component {
    std::vector<CellId> cells;
    std::vector<std::size_t> factors;
};
struct Residual {
    std::vector<ResidualFactor> factors;
    std::vector<std::size_t> degree;
    std::vector<bool> shape_choice;
    std::vector<Component> components;
};
Residual residual(const A2Problem& problem,const A2Domains& domains) {
    Residual result{{},std::vector<std::size_t>(domains.size(),0),std::vector<bool>(domains.size(),false),{}};
    for(std::size_t r=0;r<problem.constraints().size();++r) {
        const auto& factor=problem.constraints()[r];
        unsigned outputs=0;
        bool live=true;
        ResidualFactor scope{r,{}};
        std::vector<CellId> shape_choices;
        for(const auto& step:factor.steps()) {
            unsigned emissions=0,hits=0;
            for(auto s:a2_states) if(domains[step.cell].contains(s)) {
                const auto emission=step.emission(s,problem.palette());
                emissions |= 1U<<code(emission);
                if(s!=A2State::air) hits |= 1U<<(emission!=PixelLabel::background);
            }
            outputs |= emissions & ~1U;
            if(std::popcount(emissions)>1) scope.cells.push_back(step.cell);
            if(hits==3U) shape_choices.push_back(step.cell);
            // Once every remaining state hits, the tail is irrelevant under all
            // subsequent domain restrictions. Keep this guaranteed-hit cell.
            if(!(emissions & 1U)) { live=false; break; }
        }
        if(live) outputs |= 1U;
        // This is entailment over the Cartesian domains, not over sampled worlds.
        if(outputs==(1U<<code(factor.target()))) continue;
        if(scope.cells.empty()) throw std::logic_error("non-entailed A2 factor has no residual scope");
        for(auto v:scope.cells) ++result.degree[v];
        for(auto v:shape_choices) result.shape_choice[v]=true;
        result.factors.push_back(std::move(scope));
    }
    // Join only varying emissions in non-entailed, potentially visible prefixes.
    std::vector<CellId> parent(domains.size());
    std::iota(parent.begin(),parent.end(),CellId{0});
    const auto find=[&](CellId v) {
        while(parent[v]!=v) { parent[v]=parent[parent[v]]; v=parent[v]; }
        return v;
    };
    for(const auto& scope:result.factors) for(auto v:scope.cells) {
        const auto a=find(scope.cells.front()),b=find(v);
        parent[std::max(a,b)]=std::min(a,b);
    }
    std::vector<std::size_t> component(domains.size(),domains.size());
    for(CellId v=0;v<domains.size();++v) if(result.degree[v]) {
        const auto root=find(v);
        if(component[root]==domains.size()) {
            component[root]=result.components.size(); result.components.push_back({{}, {}});
        }
        result.components[component[root]].cells.push_back(v);
    }
    for(const auto& scope:result.factors)
        result.components[component[find(scope.cells.front())]].factors.push_back(scope.factor);
    return result;
}
std::optional<CellId> residual_choice(const Residual& active,const A2Domains& domains,bool only_shapes) {
    std::optional<CellId> choice;
    for(CellId v=0;v<domains.size();++v) {
        if(!active.degree[v] || domains[v].size()<=1 || (only_shapes && !active.shape_choice[v])) continue;
        if(!choice || domains[v].size()<domains[*choice].size() ||
           (domains[v].size()==domains[*choice].size() && active.degree[v]>active.degree[*choice])) choice=v;
    }
    return choice;
}
// GAC is already at a fixed point. Under the checked hit-equivalence guard,
// every occupied state at the first possible hit must emit the target (otherwise
// it has no local ray support). Choosing occupied states everywhere therefore
// satisfies all active rays. Entailed rays stay true under every such choice.
A2World fixed_hit(const A2Problem& problem,const A2Domains& domains,
                 InferenceStats& stats,A2InferenceObserver* observer) {
    ++stats.fixed_calls;
    if(observer) observer->begin(QueryKind::fixed_geometry,domains);
    A2World witness;
    for(auto d:domains) {
        const auto candidates=d.occupied().empty() ? d : d.occupied();
        for(auto s:a2_states) if(candidates.contains(s)) { witness.push_back(s); break; }
    }
    if(!contains(domains,witness) || !problem.accepts(witness))
        throw std::logic_error("A2 fixed-hit witness invariant failed");
    return witness;
}
// Component subproblems preserve cell IDs but number a subset of ray factors.
// Translate event IDs through every nesting level to the caller's original rays.
class ComponentObserver final : public A2InferenceObserver {
public:
    ComponentObserver(A2InferenceObserver& parent,std::span<const std::size_t> factors) : parent_(parent),factors_(factors) {}
    void begin(QueryKind kind,std::span<const A2Domain> assumptions) override { parent_.begin(kind,assumptions); }
    void prune(const BasicPruneEvent<A2Domain>& event) override {
        auto mapped=event;
        if(mapped.factor) mapped.factor=factors_[*mapped.factor];
        parent_.prune(mapped);
    }
    void contradiction(ContradictionReason reason,std::optional<std::size_t> factor) override {
        if(factor) factor=factors_[*factor];
        parent_.contradiction(reason,factor);
    }
private:
    A2InferenceObserver& parent_;
    std::span<const std::size_t> factors_;
};
std::optional<A2World> search(const A2Problem& problem,A2Domains domains,InferenceStats& stats,
                             A2InferenceObserver* observer,A2SearchOptions options) {
    ++stats.search_nodes;
    auto root=gac(problem,std::move(domains),stats,observer);
    if(!root.consistent) return {};
    A2World candidate;
    for(auto d:root.domains) for(auto s:a2_states) if(d.contains(s)) { candidate.push_back(s); break; }
    // Accept a simultaneous assignment, never a collection of local supports.
    if(problem.accepts(candidate)) return candidate;
    const auto active=residual(problem,root.domains);
    if(options.fixed_hit) {
        if(options.diagnostics) ++options.diagnostics->fixed_hit_checks;
        if(std::none_of(active.shape_choice.begin(),active.shape_choice.end(),[](bool b){return b;}))
            return fixed_hit(problem,root.domains,stats,observer);
        if(options.diagnostics) ++options.diagnostics->fixed_hit_rejections;
    }
    if(options.decompose && active.components.size()>1) {
        if(options.diagnostics) ++options.diagnostics->decompositions;
        for(const auto& component:active.components) {
            std::vector<A2Constraint> factors;
            for(auto r:component.factors) factors.push_back(problem.constraints()[r]);
            const A2Problem subproblem(problem.cell_count(),std::move(factors),problem.palette());
            if(options.diagnostics) ++options.diagnostics->component_solves;
            std::optional<A2World> local;
            if(observer) {
                ComponentObserver mapped(*observer,component.factors);
                local=search(subproblem,root.domains,stats,&mapped,options);
            } else local=search(subproblem,root.domains,stats,nullptr,options);
            if(!local) return {};
            // Outside-scope values in a local witness are only context. Copy
            // exactly this component's variables so siblings cannot overwrite it.
            for(auto v:component.cells) candidate[v]=(*local)[v];
        }
        if(!contains(root.domains,candidate) || !problem.accepts(candidate))
            throw std::logic_error("A2 component witness composition failed");
        return candidate;
    }
    const auto choice=residual_choice(active,root.domains,options.fixed_hit);
    if(!choice) throw std::logic_error("unsatisfied A2 factor has no residual variable");
    for(auto s:a2_states) if(root.domains[*choice].contains(s)) {
        ++stats.branches;
        auto child=root.domains;
        child[*choice]=A2Domain::singleton(s);
        if(auto witness=search(problem,std::move(child),stats,observer,options)) return witness;
    }
    return {}; // Every disjoint branch failed, so this conditioned scene is infeasible.
}
} // namespace
A2FeasibilityResult exact_feasible(const A2Problem& problem,A2Domains domains,InferenceStats& stats,A2InferenceObserver* observer,A2SearchOptions options) {
    problem.validate_domains(domains);
    auto witness=search(problem,std::move(domains),stats,observer,options);
    const bool feasible=witness.has_value();
    return {feasible,std::move(witness)};
}
A2SupportQueryResult query_support(const A2Problem& problem,A2Domains domains,CellId cell,A2State state,
                                  InferenceStats& stats,A2InferenceObserver* observer,A2SearchOptions options) {
    problem.validate_domains(domains);
    if(cell>=problem.cell_count()) throw std::out_of_range("invalid A2 support cell");
    domains[cell]=domains[cell] & A2Domain::singleton(state);
    ++stats.literal_queries;
    auto result=exact_feasible(problem,std::move(domains),stats,observer,options);
    if(!result.feasible) ++stats.infeasible_queries;
    if(result.feasible && result.witness->at(cell)!=state) throw std::logic_error("A2 query lost its condition");
    return {result.feasible,std::move(result.witness)};
}
A2SupportResult exact_supports(const A2Problem& problem,A2Domains initial,InferenceStats& stats,A2InferenceObserver* observer,A2SearchOptions options) {
    auto root=gac(problem,std::move(initial),stats,observer);
    A2SupportResult result{false,std::move(root.domains),A2Domains(problem.cell_count(),A2Domain(0)),{}};
    if(!root.consistent) return result;
    auto decision=exact_feasible(problem,result.root_domains,stats,observer,options);
    if(!decision.feasible) return result;
    result.feasible=true;
    const auto certify=[&](A2World witness) {
        for(CellId v=0;v<witness.size();++v)
            result.supported[v]=A2Domain(result.supported[v].bits() | (1U<<code(witness[v])));
        result.witnesses.push_back(std::move(witness));
    };
    certify(std::move(*decision.witness));
    for(CellId v=0;v<result.root_domains.size();++v) for(auto s:a2_states) {
        if(!result.root_domains[v].contains(s) || result.supported[v].contains(s)) continue;
        auto query=query_support(problem,result.root_domains,v,s,stats,observer,options);
        if(query.supported) certify(std::move(*query.witness));
    }
    if(observer) observer->begin(QueryKind::support_projection,result.root_domains);
    for(CellId v=0;v<result.root_domains.size();++v) if(result.root_domains[v]!=result.supported[v]) {
        if(observer) observer->prune({v,result.root_domains[v],result.supported[v],PruneReason::unsupported_state,{}});
        stats.deletions+=result.root_domains[v].size()-result.supported[v].size();
    }
    return result;
}
} // namespace mcr
