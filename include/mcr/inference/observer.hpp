#pragma once
#include "mcr/inference/constraint.hpp"

namespace mcr {
enum class QueryKind { gac, fixed_geometry, support_projection, nested_envelope };
enum class PruneReason { ray_arc_support, first_active_material, unsupported_air, envelope_first_hit, unsupported_state };
enum class ContradictionReason { empty_domain, no_ray_support, foreground_escapes, required_cell_empty, envelope_empty };
template<class DomainType> struct BasicPruneEvent {
    CellId cell;
    DomainType before,after;
    PruneReason reason;
    std::optional<std::size_t> factor;
};
// Read-only observer: it supplies no support masks or inference decisions.
template<class DomainType> class BasicInferenceObserver {
public:
    virtual ~BasicInferenceObserver() = default;
    virtual void begin(QueryKind,std::span<const DomainType> assumptions) = 0;
    virtual void prune(const BasicPruneEvent<DomainType>&) = 0;
    virtual void contradiction(ContradictionReason,std::optional<std::size_t> factor) = 0;
};
using PruneEvent=BasicPruneEvent<Domain>;
using InferenceObserver=BasicInferenceObserver<Domain>;
struct InferenceStats {
    std::uint64_t gac_calls=0,fixed_calls=0,factor_updates=0,deletions=0;
    std::uint64_t literal_queries=0,infeasible_queries=0;
    std::uint64_t envelope_calls=0,envelope_advances=0;
    // Nested A1 feasibility performs no search and leaves these counters unchanged.
    std::uint64_t branches=0,search_nodes=0;
};
struct PropagationResult { bool feasible; Domains domains; };
} // namespace mcr
