#include "input.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

namespace mcr::app {
namespace {
Json text(const char* s) { return Json(std::string(s)); }
Json number(std::size_t n) { return Json(static_cast<double>(n)); }
constexpr std::array names{"air","bottom_slab","top_slab"};
Json state(A2State s) { return text(names.at(code(s))); }
Json catalog(A2Palette palette) {
    auto slab = [](const char* block, const char* half) {
        return Json(Object{{"block",text(block)},
            {"properties",Json(Object{{"type",text(half)},{"waterlogged",text("false")}})}});
    };
    return Json(Object{{"air",Json(Object{{"block",text("minecraft:air")},{"properties",Json(Object{})}})},
        {"bottom_slab",slab("minecraft:oak_slab","bottom")},
        {"top_slab",slab(palette == A2Palette::split_material ? "minecraft:stone_slab" : "minecraft:oak_slab","top")}});
}
} // namespace

Json reconstruct(const Input& input, const Pixels& pixels) {
    std::vector<Ray> rays;
    std::vector<A2Constraint> factors;
    std::array<std::size_t,3> counts{};
    for (unsigned row = 0; row < pixels.height; ++row) for (unsigned col = 0; col < pixels.width; ++col) {
        const auto target = pixels.labels.at(static_cast<std::size_t>(row)*pixels.width+col);
        ++counts.at(code(target));
        rays.push_back(pixel_ray(input,col,row));
        const A2TraversalRay traversal(input.grid,rays.back(),input.palette);
        factors.emplace_back(std::vector<A2RayCell>(traversal.steps().begin(),traversal.steps().end()),target,input.palette);
    }
    const A2Problem problem(input.grid.size(),std::move(factors),input.palette);
    const A2Domains domains(input.grid.size()); // all three legal A2 states in every cell
    InferenceStats stats;
    const auto result = exact_supports(problem,domains,stats,nullptr,{true,true,nullptr});
    Json witness;
    if (result.feasible) {
        if (result.witnesses.empty()) throw std::runtime_error("internal error: feasible result has no witness");
        const auto& chosen = result.witnesses.front();
        if (!contains(domains,chosen) || !problem.accepts(chosen))
            throw std::runtime_error("internal error: invalid simultaneous witness");
        // Validate against original pixels using primitive AABB intersections,
        // independently of the traversal constraints supplied to inference.
        for (std::size_t i = 0; i < rays.size(); ++i)
            if (A2AabbReferenceRay(input.grid,rays[i],input.palette).sample(chosen) != pixels.labels[i])
                throw std::runtime_error("internal error: witness does not reproduce the PNG observations");
        Array states;
        for (const auto s : chosen) states.push_back(state(s));
        witness = Json(Object{{"states",Json(states)},{"validated_against_input_pixels",Json(true)}});
    }
    Array cells;
    bool unique = result.feasible;
    for (CellId v = 0; v < input.grid.size(); ++v) {
        const auto p = input.grid.position(v);
        Array supported;
        for (const auto s : a2_states) if (result.supported.at(v).contains(s)) supported.push_back(state(s));
        unique = unique && supported.size() == 1;
        const auto only = supported.size() == 1 ? supported.front() : Json();
        cells.emplace_back(Object{{"id",number(v)},
            {"position",Json(Array{number(static_cast<std::size_t>(p[0])),number(static_cast<std::size_t>(p[1])),number(static_cast<std::size_t>(p[2]))})},
            {"supported_states",Json(supported)},{"unique_state",only}});
    }
    return Json(Object{
        {"schema",text("mcr-reconstruction-v1")},
        {"feasibility",text(result.feasible ? "SAT" : "UNSAT")},
        {"projection_complete",Json(true)},
        {"joint_solution_unique",result.feasible ? Json(unique) : Json()},
        {"metadata",input.metadata},
        {"state_catalog",catalog(input.palette)},
        {"cell_order",text("id = (x * 2 + y) * 2 + z; y is vertical")},
        {"observations",Json(Object{{"pixels",number(pixels.labels.size())},
            {"background",number(counts[0])},{"stone",number(counts[1])},{"oak",number(counts[2])}})},
        {"witness",witness},
        {"witness_semantics",text("one simultaneous solution; not a claim of recovered ground truth")},
        {"support_semantics",text("exact per-cell marginals; arbitrary combinations need not form a feasible scene")},
        {"cells",Json(cells)}});
}

void write_result(const std::filesystem::path& path, const Json& result) {
    const auto staging = std::filesystem::path(path.string()+".mcr-tmp");
    // Atomic directory creation reserves the staging name without truncating an
    // existing file or another writer's temporary result.
    if (!std::filesystem::create_directory(staging)) throw std::runtime_error("output staging path already exists: " + staging.string());
    try {
        const auto temporary = staging/"result.json";
        std::ofstream out(temporary,std::ios::binary);
        out << result.serialize(true); out.flush(); // pretty serialization includes its final newline
        if (!out) throw std::runtime_error("cannot write reconstruction output");
        out.close();
        if (!out) throw std::runtime_error("cannot close reconstruction output");
        std::filesystem::rename(temporary,path);
        std::filesystem::remove(staging);
    } catch (...) {
        std::filesystem::remove_all(staging);
        throw;
    }
}
} // namespace mcr::app
