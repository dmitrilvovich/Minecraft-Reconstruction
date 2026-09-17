#include "test.hpp"
#include "fixtures/a0_scenes.hpp"
#include "oracle/audit.hpp"
#include "mcr/experiments/a0.hpp"
#include "mcr/inference/a0_solver.hpp"
#include <algorithm>

using namespace mcr;
int main() { return run_tests([] {
    const experiments::A0Rig rig;
    const experiments::A0Corpus corpus(rig);
    const auto scenes=test::adversarial_scenes();
    CHECK(scenes.size()==24);
    for(const auto& suite:rig.suites()) {
        const auto classes=corpus.families(suite);
        for(const auto& scene:scenes) {
            const auto labels=corpus.observation(static_cast<std::uint32_t>(a0_world_id(scene.world)),suite.factors);
            const auto& ids=classes.at(labels);
            std::vector<World> family;
            for(auto id:ids) family.push_back(corpus.worlds()[id]);
            const auto problem=rig.problem(suite.factors,labels);
            test::ExhaustiveAudit audit(family);
            InferenceStats stats;
            const auto result=exact_supports(problem,Domains(8),stats,&audit);
            CHECK(result.feasible && result.supported==test::supports(family,8));
            CHECK(contains(result.supported,scene.world));
            // Repeating observations and changing processing order preserve meaning.
            std::vector<FirstHitConstraint> repeated(problem.constraints().begin(),problem.constraints().end());
            repeated.insert(repeated.end(),problem.constraints().begin(),problem.constraints().end());
            std::reverse(repeated.begin(),repeated.end());
            CHECK(exact_supports(Problem(8,repeated),Domains(8),stats,&audit).supported==result.supported);
        }
    }
    std::cout << "24 named adversarial scenes across eight view suites, duplicates and order reversal checked\n";
}); }

