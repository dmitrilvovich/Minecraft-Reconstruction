#pragma once
#include "mcr/inference/constraint.hpp"

namespace mcr {
enum class QueryKind { gac, fixed_geometry, support_projection };
enum class PruneReason { ray_arc_support, first_active_material, unsupported_air };
enum class ContradictionReason { empty_domain, no_ray_support, foreground_escapes, required_cell_empty };
struct PruneEvent {
    CellId cell;
    Domain before,after;
    PruneReason reason;
    std::optional<std::size_t> factor;
};
// Read-only observer: it supplies no support masks or inference decisions.
class InferenceObserver {
public:
    virtual ~InferenceObserver() = default;
    virtual void begin(QueryKind,std::span<const Domain> assumptions) = 0;
    virtual void prune(const PruneEvent&) = 0;
    virtual void contradiction(ContradictionReason,std::optional<std::size_t> factor) = 0;
};
struct InferenceStats {
    std::uint64_t gac_calls=0,fixed_calls=0,factor_updates=0,deletions=0;
    std::uint64_t literal_queries=0,infeasible_queries=0;
};
struct PropagationResult { bool feasible; Domains domains; };
} // namespace mcr

