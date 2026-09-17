#pragma once
#include "mcr/experiments/cameras.hpp"
#include "mcr/render/a0.hpp"
#include "mcr/inference/constraint.hpp"
#include <map>

namespace mcr::experiments {
using Observation = std::vector<PixelLabel>;
using WorldIds = std::vector<std::uint32_t>;
using Families = std::map<Observation,WorldIds>;
struct ViewSuite { std::string name; unsigned views; std::vector<std::size_t> factors; };

// The frozen 2x2x2, 8x8-camera A0 setup. Geometry is compiled once per ray.
class A0Rig {
public:
    A0Rig();
    [[nodiscard]] std::size_t factor_count() const { return rays_.size(); }
    [[nodiscard]] std::span<const CellId> chain(std::size_t r) const { return rays_.at(r).cells(); }
    [[nodiscard]] std::span<const ViewSuite> suites() const { return suites_; }
    [[nodiscard]] Observation observe_aabb(std::span<const A0State>) const;
    [[nodiscard]] Problem problem(std::span<const std::size_t> factors,std::span<const PixelLabel> labels) const;
private:
    std::vector<TraversalRay> rays_;
    std::vector<AabbReferenceRay> reference_;
    std::vector<ViewSuite> suites_;
};

// Exhaustive data is confined to experiments/tests; the inference library never uses it.
class A0Corpus {
public:
    explicit A0Corpus(const A0Rig&);
    [[nodiscard]] std::span<const World> worlds() const { return worlds_; }
    [[nodiscard]] Families families(const ViewSuite&) const;
    [[nodiscard]] Observation observation(std::uint32_t world,std::span<const std::size_t> factors) const;
private:
    std::vector<World> worlds_;
    std::vector<Observation> images_;
};
} // namespace mcr::experiments

