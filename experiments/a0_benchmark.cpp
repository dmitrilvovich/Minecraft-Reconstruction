#include "mcr/experiments/a0.hpp"
#include "mcr/inference/a0_solver.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <string_view>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

using namespace mcr;
namespace {
using Clock=std::chrono::steady_clock;
struct Summary {
    std::string name;
    unsigned views;
    std::size_t classes;
    std::uint64_t identifiable=0,free=0,unique_worlds=0;
    std::uint64_t removed=0,queries=0,updates=0;
    std::vector<double> times;
};
unsigned candidate_count(const Domains& domains) {
    unsigned count=0; for(auto d:domains) count+=d.size(); return count;
}
std::string mask_string(const Domains& domains) {
    std::ostringstream out;
    for(std::size_t v=0;v<domains.size();++v) { if(v) out << ' '; out << domains[v].bits(); }
    return out.str();
}
double percentile(std::vector<double> values,double fraction) {
    std::sort(values.begin(),values.end());
    return values.at(static_cast<std::size_t>(fraction*static_cast<double>(values.size()-1)));
}
struct FamilySummary { Domains support; unsigned identifiable=0,free=0; };
FamilySummary summarize(const experiments::A0Corpus& corpus,const experiments::WorldIds& ids) {
    FamilySummary out{Domains(8,Domain(0))};
    for(auto id:ids) for(CellId v=0;v<8;++v)
        out.support[v]=Domain(out.support[v].bits() | (1U<<code(corpus.worlds()[id][v])));
    for(CellId v=0;v<8;++v) {
        if(out.support[v].size()==1) ++out.identifiable;
        bool free=true;
        for(auto id:ids) {
            auto altered=corpus.worlds()[id];
            for(auto s:a0_states) {
                altered[v]=s;
                if(!std::binary_search(ids.begin(),ids.end(),static_cast<std::uint32_t>(a0_world_id(altered)))) free=false;
            }
            if(!free) break;
        }
        if(free) ++out.free;
    }
    return out;
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path output="out/a0";
        unsigned repetitions=5;
        for(int arg=1;arg<argc;++arg) {
            const std::string_view option(argv[arg]);
            if(option=="--help") {
                std::cout << "mcr_a0_benchmark [--output DIRECTORY] [--repetitions 1..100]\n"; return 0;
            }
            if(arg+1>=argc) throw std::invalid_argument("missing option argument");
            if(option=="--output") output=argv[++arg];
            else if(option=="--repetitions") {
                const std::string value(argv[++arg]); std::size_t consumed=0;
                const auto parsed=std::stoul(value,&consumed);
                if(consumed!=value.size() || parsed<1 || parsed>100) throw std::invalid_argument("repetitions must be 1..100");
                repetitions=static_cast<unsigned>(parsed);
            } else throw std::invalid_argument("unknown option");
        }
        std::filesystem::create_directories(output);
        std::ofstream csv(output/"cases.csv");
        if(!csv) throw std::runtime_error("cannot create benchmark CSV");
        csv << "suite,representative,feasible_worlds,initial_candidates,root_candidates,removed_by_root,exact_candidates,unsupported_survivors,identifiable_cells,freely_variable_cells,literal_queries,infeasible_queries,fixed_calls,gac_calls,factor_updates,deletions,branches,search_nodes,median_solver_us,root_domains,supported_domains\n";
        const auto start=Clock::now();
        const experiments::A0Rig rig;
        const experiments::A0Corpus corpus(rig);
        const double oracle_seconds=std::chrono::duration<double>(Clock::now()-start).count();
        std::vector<Summary> summaries;
        std::uint64_t case_count=0;
        for(const auto& suite:rig.suites()) {
            const auto families=corpus.families(suite);
            Summary summary{suite.name,suite.views,families.size()};
            for(const auto& [observation,ids]:families) {
                const auto expected=summarize(corpus,ids);
                const auto problem=rig.problem(suite.factors,observation);
                InferenceStats stats;
                // Warmup and validation are outside the timed repetitions.
                auto result=exact_supports(problem,Domains(8),stats);
                if(!result.feasible || result.supported!=expected.support)
                    throw std::logic_error("benchmark failed its exhaustive reference check");
                std::vector<double> times;
                for(unsigned repeat=0;repeat<repetitions;++repeat) {
                    InferenceStats measured;
                    const auto before=Clock::now();
                    auto actual=exact_supports(problem,Domains(8),measured);
                    const auto after=Clock::now();
                    if(actual.supported!=expected.support || !actual.feasible)
                        throw std::logic_error("timed inference changed its answer");
                    times.push_back(std::chrono::duration<double,std::micro>(after-before).count());
                }
                const auto median=percentile(times,0.5);
                summary.times.push_back(median);
                summary.identifiable+=ids.size()*expected.identifiable;
                summary.free+=ids.size()*expected.free;
                summary.unique_worlds+=ids.size()==1 ? 1 : 0;
                const auto root_candidates=candidate_count(result.root_domains),exact_candidates=candidate_count(result.supported);
                summary.removed+=24-root_candidates;
                summary.queries+=stats.literal_queries;
                summary.updates+=stats.factor_updates;
                csv << suite.name << ',' << ids.front() << ',' << ids.size() << ",24," << root_candidates << ','
                    << 24-root_candidates << ',' << exact_candidates << ',' << root_candidates-exact_candidates << ','
                    << expected.identifiable << ',' << expected.free << ',' << stats.literal_queries << ','
                    << stats.infeasible_queries << ',' << stats.fixed_calls << ',' << stats.gac_calls << ','
                    << stats.factor_updates << ',' << stats.deletions << ",0,0," << std::setprecision(9) << median << ','
                    << mask_string(result.root_domains) << ',' << mask_string(result.supported) << '\n';
                ++case_count;
            }
            std::cout << suite.name << ": " << families.size() << " classes, median "
                      << percentile(summary.times,0.5) << " us\n";
            summaries.push_back(std::move(summary));
        }
        csv.close();
        if(!csv) throw std::runtime_error("benchmark CSV write failed");
        std::ofstream json(output/"summary.json");
        if(!json) throw std::runtime_error("cannot create benchmark JSON");
        json << std::setprecision(10) << "{\n  \"schema_version\": 1,\n  \"model\": \"A0\",\n  \"status\": \"pass\",\n"
             << "  \"compiler\": \"" << MCR_COMPILER << "\",\n  \"build_type\": \"" << MCR_BUILD_TYPE << "\",\n"
             << "  \"worlds\": 6561,\n  \"classes\": " << case_count << ",\n  \"repetitions\": " << repetitions
             << ",\n  \"root_stage\": \"fixed_geometry_then_gac\",\n  \"timing_scope\": \"complete_support_query_without_oracle_or_audit\",\n"
             << "  \"sampling\": \"all_classes; identifiability weighted uniformly over all worlds\",\n"
             << "  \"oracle_setup_seconds\": " << oracle_seconds << ",\n  \"peak_rss_kib_whole_process\": ";
#if defined(__unix__) || defined(__APPLE__)
        rusage usage{};
        if(getrusage(RUSAGE_SELF,&usage)==0) {
#if defined(__APPLE__)
            json << usage.ru_maxrss/1024;
#else
            json << usage.ru_maxrss;
#endif
        } else json << "null";
#else
        json << "null";
#endif
        json << ",\n  \"memory_scope\": \"whole C++ process including exhaustive corpus; not solver-only\",\n"
             << "  \"search_branches\": 0,\n  \"search_nodes\": 0,\n  \"suites\": [\n";
        for(std::size_t j=0;j<summaries.size();++j) {
            const auto& s=summaries[j];
            json << "    {\"name\": \"" << s.name << "\", \"views\": " << s.views << ", \"classes\": " << s.classes
                 << ", \"identifiable_cell_fraction\": " << static_cast<double>(s.identifiable)/(6561*8)
                 << ", \"freely_variable_cell_fraction\": " << static_cast<double>(s.free)/(6561*8)
                 << ", \"unique_world_fraction\": " << static_cast<double>(s.unique_worlds)/6561
                 << ", \"mean_removed_by_root\": " << static_cast<double>(s.removed)/static_cast<double>(s.classes)
                 << ", \"literal_queries\": " << s.queries << ", \"factor_updates\": " << s.updates
                 << ", \"median_solver_us\": " << percentile(s.times,0.5)
                 << ", \"p95_solver_us\": " << percentile(s.times,0.95)
                 << ", \"max_solver_us\": " << *std::max_element(s.times.begin(),s.times.end()) << '}';
            if(j+1<summaries.size()) json << ',';
            json << '\n';
        }
        json << "  ]\n}\n";
        json.close();
        if(!json) throw std::runtime_error("benchmark JSON write failed");
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
