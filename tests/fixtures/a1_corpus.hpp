#pragma once
#include "mcr/experiments/cameras.hpp"
#include "mcr/experiments/a0.hpp"
#include "mcr/inference/a1_problem.hpp"

namespace mcr::test {
using experiments::Observation;
using experiments::ViewSuite;
using experiments::Families;
// Test-only independent AABB corpus. Inference never receives these families.
class A1Rig {
public:
    A1Rig();
    [[nodiscard]] std::size_t factor_count() const { return rays_.size(); }
    [[nodiscard]] std::span<const A1RayCell> steps(std::size_t r) const { return rays_.at(r).steps(); }
    [[nodiscard]] std::span<const ViewSuite> suites() const { return suites_; }
    [[nodiscard]] Observation observe_aabb(std::span<const A1State>) const;
    [[nodiscard]] A1Problem problem(std::span<const std::size_t>,std::span<const PixelLabel>) const;
private:
    std::vector<A1TraversalRay> rays_;
    std::vector<A1AabbReferenceRay> reference_;
    std::vector<ViewSuite> suites_;
};
class A1Corpus {
public:
    explicit A1Corpus(const A1Rig&);
    [[nodiscard]] std::span<const A1World> worlds() const { return worlds_; }
    [[nodiscard]] Families families(const ViewSuite&) const;
    [[nodiscard]] Observation observation(std::uint32_t,std::span<const std::size_t>) const;
    [[nodiscard]] bool matches(std::uint32_t,std::span<const std::size_t>,std::span<const PixelLabel>) const;
private:
    std::vector<A1World> worlds_;
    std::vector<Observation> images_;
};
} // namespace mcr::test
