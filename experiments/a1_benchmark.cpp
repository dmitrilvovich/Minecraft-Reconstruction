#include "mcr/experiments/a1.hpp"
#include "mcr/inference/a1_supports.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

using namespace mcr;
namespace {
using Clock=std::chrono::steady_clock;
void require(bool ok,const char* message) { if(!ok) throw std::logic_error(message); }
double elapsed(Clock::time_point start) { return std::chrono::duration<double>(Clock::now()-start).count(); }
long peak_rss_kib() {
#if defined(__unix__) || defined(__APPLE__)
    rusage usage{};
    if(getrusage(RUSAGE_SELF,&usage)!=0) return -1;
#if defined(__APPLE__)
    return usage.ru_maxrss/1024;
#else
    return usage.ru_maxrss;
#endif
#else
    return -1;
#endif
}
constexpr std::array work_names{"gac_calls","fixed_calls","factor_updates","deletions","literal_queries",
    "infeasible_queries","envelope_calls","envelope_advances","branches","search_nodes"};
auto work(const InferenceStats& s) {
    return std::array<std::uint64_t,10>{s.gac_calls,s.fixed_calls,s.factor_updates,s.deletions,s.literal_queries,
        s.infeasible_queries,s.envelope_calls,s.envelope_advances,s.branches,s.search_nodes};
}
unsigned candidates(const A1Domains& domains) { unsigned n=0; for(auto d:domains) n+=d.size(); return n; }
std::uint32_t packed(const A1Domains& domains) {
    require(domains.size()==8,"wrong domain count"); std::uint32_t result=0;
    for(unsigned v=0;v<8;++v) result |= domains[v].bits()<<(4*v);
    return result;
}
struct Metrics {
    A1Domains support=A1Domains(8,A1Domain(0));
    unsigned identified=0,free=0,geometry=0,cube_slab=0;
    std::array<std::uint64_t,9> slabs{};
};
// All exhaustive/reference work is outside the solver timing interval.
Metrics summarize(const experiments::A1Corpus& corpus,const experiments::WorldIds& ids,const A1Domains& initial) {
    Metrics out;
    for(auto id:ids) {
        const auto& world=corpus.worlds()[id];
        require(contains(initial,world),"oracle family violates input domains");
        unsigned slabs=0;
        for(CellId v=0;v<8;++v) {
            out.support[v]=A1Domain(out.support[v].bits() | (1U<<code(world[v])));
            slabs+=world[v]==A1State::oak_slab;
        }
        ++out.slabs[slabs];
    }
    if(ids.empty()) return out; // identification/freedom are undefined for infeasible inputs
    for(CellId v=0;v<8;++v) {
        out.identified+=out.support[v].size()==1;
        unsigned ranks=0;
        for(auto s:a1_states) if(out.support[v].contains(s)) ranks |= 1U<<geometry_rank(s);
        out.geometry+=std::popcount(ranks)==1;
        out.cube_slab+=out.support[v].contains(A1State::oak_slab) && (ranks & (1U<<2));
        bool free=out.support[v]==initial[v];
        for(auto id:ids) {
            if(!free) break;
            const unsigned shift=2*static_cast<unsigned>(v);
            const auto context=id & ~(3U<<shift);
            for(auto s:a1_states) if(initial[v].contains(s) &&
                !std::binary_search(ids.begin(),ids.end(),context | (code(s)<<shift))) free=false;
        }
        out.free+=free;
    }
    return out;
}
void witness(const A1World& w,const A1Domains& initial,const experiments::WorldIds& ids) {
    require(w.size()==8 && contains(initial,w),"witness violates original domains");
    require(std::binary_search(ids.begin(),ids.end(),static_cast<std::uint32_t>(a1_world_id(w))),
            "witness missing from independent AABB family");
}
struct Reader {
    std::ifstream in;
    explicit Reader(const std::filesystem::path& path) : in(path,std::ios::binary) { require(in.good(),"cannot open restricted fixture"); }
    unsigned byte() { const auto b=in.get(); require(b!=std::char_traits<char>::eof(),"truncated fixture"); return static_cast<unsigned>(b); }
    std::uint32_t u32() { std::uint32_t n=0; for(unsigned i=0;i<4;++i) n |= byte()<<(8*i); return n; }
    std::string text(unsigned n) { require(n<1000,"bad fixture text length"); std::string s; while(n--) s+=static_cast<char>(byte()); return s; }
    A1Domains masks() { A1Domains d; for(unsigned v=0;v<8;++v) d.emplace_back(byte()); return d; }
    void skip(std::uint64_t n) { in.seekg(static_cast<std::streamoff>(n),std::ios::cur); require(in.good(),"fixture seek failed"); }
};
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path output="out/a1",restricted;
        unsigned repetitions=5,limit=0,csv_rows=10000;
        for(int arg=1;arg<argc;++arg) {
            const std::string_view option(argv[arg]);
            if(option=="--help") { std::cout << "mcr_a1_benchmark [--output DIR] [--repetitions 1..100] [--restricted-fixture FILE] [--limit-per-suite N] [--csv-rows-per-file 1..10000]\n"; return 0; }
            if(arg+1>=argc) throw std::invalid_argument("missing option argument");
            const std::string value(argv[++arg]);
            if(option=="--output") output=value;
            else if(option=="--restricted-fixture") restricted=value;
            else if(option=="--repetitions" || option=="--limit-per-suite" || option=="--csv-rows-per-file") {
                std::size_t consumed=0; const auto n=std::stoul(value,&consumed);
                if(consumed!=value.size() || n>65536 || (option=="--repetitions" && (n<1 || n>100)) || (option=="--csv-rows-per-file" && (n<1 || n>10000)))
                    throw std::invalid_argument("invalid benchmark count");
                if(option=="--repetitions") repetitions=static_cast<unsigned>(n);
                else if(option=="--limit-per-suite") limit=static_cast<unsigned>(n);
                else csv_rows=static_cast<unsigned>(n);
            } else throw std::invalid_argument("unknown option");
        }
        std::filesystem::create_directories(output);
        const auto start=Clock::now(); const auto rss_start=peak_rss_kib();
        std::vector<std::int64_t> clock_samples;
        for(unsigned i=0;i<10000;++i) { const auto a=Clock::now(),b=Clock::now(); clock_samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count()); }
        std::sort(clock_samples.begin(),clock_samples.end());
        const auto setup=Clock::now();
        const experiments::A1Rig rig; const experiments::A1Corpus corpus(rig);
        const double corpus_seconds=elapsed(setup); const auto rss_corpus=peak_rss_kib();
        double families_seconds=0,metrics_seconds=0,problem_seconds=0,validation_seconds=0,solver_seconds=0;
        std::uint64_t rendered_cases=0,restricted_cases=0,feasible_cases=0,witnesses=0;
        std::filesystem::path pending_csv,complete_csv;
        const auto open_csv=[&](const std::string& name) {
            complete_csv=output/(name+".csv"); pending_csv=output/(name+".csv.tmp");
            std::ofstream csv(pending_csv); require(csv.good(),"cannot create benchmark CSV");
            csv << "case,feasible_worlds,feasible,initial_candidates,gac_consistent,gac_candidates,envelope_candidates,exact_candidates,identified_cells,free_cells,geometry_cells,cube_slab_cells,witnesses,input_masks,gac_masks,envelope_masks,supported_masks";
            for(unsigned k=0;k<9;++k) csv << ",truth_slabs_" << k;
            for(const auto stage:{"gac","envelope","projection"}) for(const auto field:work_names) csv << ',' << stage << '_' << field;
            for(unsigned r=0;r<repetitions;++r) csv << ",solver_ns_" << r;
            csv << '\n'; return csv;
        };
        const auto close_csv=[&](std::ofstream& csv) {
            csv.close(); require(csv.good(),"CSV part write failed");
            std::filesystem::rename(pending_csv,complete_csv);
        };
        const auto run_case=[&](std::ofstream& csv,std::uint32_t id,const A1Problem& problem,const A1Domains& initial,
                                const experiments::WorldIds& family,const Metrics& expected) {
            auto stage_start=Clock::now();
            InferenceStats gs,es,stats;
            // Separate diagnostics from identical input domains. GAC is NOT a
            // prepass in the accepted exact_supports implementation.
            const auto g=gac(problem,initial,gs);
            const auto e=envelope_feasible(problem,initial,es);
            const auto result=exact_supports(problem,initial,stats); // one warmup
            require(result.feasible==!family.empty() && e.feasible==result.feasible,"feasibility mismatch");
            require(result.supported==expected.support,"exhaustive support mismatch");
            require(!gs.branches && !gs.search_nodes && !es.branches && !es.search_nodes && !stats.branches && !stats.search_nodes,"unexpected search");
            require(stats.gac_calls==0 && stats.envelope_calls==1+stats.literal_queries,"projection work accounting mismatch");
            if(result.feasible) {
                require(g.consistent && e.witness.has_value(),"lost feasible case"); witness(*e.witness,initial,family);
                A1Domains covered(8,A1Domain(0));
                for(const auto& w:result.witnesses) { witness(w,initial,family); for(CellId v=0;v<8;++v) covered[v]=A1Domain(covered[v].bits() | (1U<<code(w[v]))); }
                require(covered==expected.support,"missing witness coverage");
                require(result.witnesses.size()==1+stats.literal_queries-stats.infeasible_queries,"query/witness accounting mismatch");
                for(CellId v=0;v<8;++v) {
                    require((expected.support[v] & g.domains[v])==expected.support[v],"GAC lost support");
                    require((expected.support[v] & e.domains[v])==expected.support[v],"envelope lost support");
                }
            } else require(result.witnesses.empty() && !e.witness && !stats.literal_queries,"infeasible output mismatch");
            validation_seconds+=elapsed(stage_start);
            std::vector<std::int64_t> times; times.reserve(repetitions);
            for(unsigned r=0;r<repetitions;++r) {
                InferenceStats measured;
                const auto before=Clock::now();
                const auto actual=exact_supports(problem,initial,measured);
                const auto after=Clock::now();
                const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(after-before).count();
                times.push_back(ns); solver_seconds+=static_cast<double>(ns)*1e-9;
                stage_start=Clock::now();
                require(actual.feasible==result.feasible && actual.supported==result.supported && actual.witnesses==result.witnesses && work(measured)==work(stats),"timed call changed result or work");
                validation_seconds+=elapsed(stage_start);
            }
            feasible_cases+=result.feasible; witnesses+=result.witnesses.size();
            csv << id << ',' << family.size() << ',' << result.feasible << ',' << candidates(initial) << ',' << g.consistent << ',' << candidates(g.domains) << ',' << candidates(e.domains) << ',' << candidates(result.supported)
                << ',' << expected.identified << ',' << expected.free << ',' << expected.geometry << ',' << expected.cube_slab << ',' << result.witnesses.size()
                << ',' << packed(initial) << ',' << packed(g.domains) << ',' << packed(e.domains) << ',' << packed(result.supported);
            for(auto n:expected.slabs) csv << ',' << n;
            for(const auto& s:{gs,es,stats}) for(auto n:work(s)) csv << ',' << n;
            for(auto ns:times) csv << ',' << ns;
            csv << '\n';
        };
        for(const auto& suite:rig.suites()) {
            auto stage_start=Clock::now(); const auto families=corpus.families(suite); families_seconds+=elapsed(stage_start);
            auto csv=open_csv(suite.name+"_part0"); unsigned count=0;
            for(const auto& [observation,ids]:families) {
                if(limit && count>=limit) break;
                if(count && count%csv_rows==0) {
                    close_csv(csv);
                    csv=open_csv(suite.name+"_part"+std::to_string(count/csv_rows));
                }
                const A1Domains initial(8);
                stage_start=Clock::now(); const auto expected=summarize(corpus,ids,initial); metrics_seconds+=elapsed(stage_start);
                stage_start=Clock::now(); const auto problem=rig.problem(suite.factors,observation); problem_seconds+=elapsed(stage_start);
                run_case(csv,ids.front(),problem,initial,ids,expected); ++count; ++rendered_cases;
            }
            close_csv(csv);
            std::cout << suite.name << ": " << count << " observation families measured\n" << std::flush;
        }
        if(!limit) require(rendered_cases==195620,"incomplete camera corpus");
        if(!restricted.empty()) {
            auto stage_start=Clock::now(); Reader reader(restricted);
            require(reader.text(8)=="MCRA1C1\n" && reader.u32()==rig.factor_count(),"wrong fixture model");
            for(std::size_t r=0;r<rig.factor_count();++r) {
                require(reader.u32()==rig.steps(r).size(),"wrong factor length");
                for(const auto& step:rig.steps(r)) require(reader.u32()==step.cell && reader.byte()==step.slab_hit,"wrong fixture geometry");
            }
            require(reader.u32()==8,"wrong suite count");
            for(const auto& suite:rig.suites()) {
                require(reader.text(reader.u32())==suite.name && reader.u32()==suite.views,"wrong suite");
                require(reader.u32()==suite.factors.size(),"wrong factor count");
                for(auto r:suite.factors) require(reader.u32()==r,"wrong factor ID");
                const auto n=reader.u32(); require(n<=65536,"bad family count");
                for(unsigned c=0;c<n;++c) { const auto size=reader.u32(); require(size<=65536,"bad family size"); reader.skip(4ULL*size+153); }
            }
            const auto queries=reader.u32(); require(queries==512,"wrong restricted count");
            families_seconds+=elapsed(stage_start); auto csv=open_csv("restricted_part0");
            for(unsigned q=0;q<queries;++q) {
                if(q && q%csv_rows==0) {
                    close_csv(csv);
                    csv=open_csv("restricted_part"+std::to_string(q/csv_rows));
                }
                stage_start=Clock::now();
                const auto n=reader.u32(); require(n<=74,"bad observation count");
                std::vector<std::size_t> indices; experiments::Observation labels;
                for(unsigned j=0;j<n;++j) { indices.push_back(reader.u32()); labels.push_back(static_cast<PixelLabel>(reader.byte())); }
                const auto initial=reader.masks(); const bool feasible=reader.byte()!=0; const auto support=reader.masks();
                const auto size=reader.u32(); require(size<=65536,"bad conditioned family size");
                experiments::WorldIds expected_ids; for(unsigned j=0;j<size;++j) expected_ids.push_back(reader.u32());
                experiments::WorldIds family;
                for(std::uint32_t id=0;id<65536;++id) if(contains(initial,corpus.worlds()[id]) && corpus.matches(id,indices,labels)) family.push_back(id);
                require(family==expected_ids && feasible==!family.empty(),"restricted exhaustive family mismatch");
                const auto expected=summarize(corpus,family,initial); require(expected.support==support,"restricted support mismatch");
                metrics_seconds+=elapsed(stage_start);
                stage_start=Clock::now(); const auto problem=rig.problem(indices,labels); problem_seconds+=elapsed(stage_start);
                run_case(csv,q,problem,initial,family,expected); ++restricted_cases;
            }
            require(reader.in.get()==std::char_traits<char>::eof(),"extra fixture bytes");
            close_csv(csv);
            std::cout << restricted_cases << " restricted/altered cases measured\n" << std::flush;
        }
        std::ofstream json(output/"run.json.tmp"); require(json.good(),"cannot create run metadata");
        json << std::setprecision(12) << "{\n  \"status\": \"pass\",\n  \"model\": \"A1\",\n  \"schema_version\": 1,\n"
             << "  \"compiler\": \"" << MCR_COMPILER << "\",\n  \"build_type\": \"" << MCR_BUILD_TYPE << "\",\n"
             << "  \"repetitions\": " << repetitions << ",\n  \"limit_per_suite\": " << limit
             << ",\n  \"csv_rows_per_file\": " << csv_rows
             << ",\n  \"rendered_cases\": " << rendered_cases << ",\n  \"restricted_cases\": " << restricted_cases
             << ",\n  \"feasible_cases\": " << feasible_cases << ",\n  \"verified_warmup_projection_witnesses\": " << witnesses
             << ",\n  \"steady_clock_is_steady\": " << (Clock::is_steady ? "true" : "false")
             << ",\n  \"median_back_to_back_clock_ns\": " << clock_samples[4999]
             << ",\n  \"corpus_setup_seconds\": " << corpus_seconds << ",\n  \"family_partition_and_fixture_read_seconds\": " << families_seconds
             << ",\n  \"oracle_metrics_seconds\": " << metrics_seconds << ",\n  \"problem_setup_seconds\": " << problem_seconds
             << ",\n  \"diagnostics_warmup_validation_seconds\": " << validation_seconds << ",\n  \"timed_projection_seconds\": " << solver_seconds
             << ",\n  \"whole_benchmark_seconds\": " << elapsed(start)
             << ",\n  \"peak_rss_kib_at_start\": " << rss_start << ",\n  \"peak_rss_kib_after_corpus\": " << rss_corpus
             << ",\n  \"peak_rss_kib_whole_process\": " << peak_rss_kib()
             << ",\n  \"memory_scope\": \"whole process including corpus, families, allocator and output buffers; not solver-only; -1 means unavailable\",\n"
             << "  \"timing_scope\": \"exact_supports from original domains, including input copy, allocations, witness construction and internal checks; excludes result destruction, compilation, oracle, external validation and CSV I/O\",\n"
             << "  \"root_stage\": \"envelope_from_input; standalone_gac_is_a_separate_diagnostic\",\n"
             << "  \"search_branches\": 0,\n  \"search_nodes\": 0\n}\n";
        json.close(); require(json.good(),"run metadata write failed");
        std::filesystem::rename(output/"run.json.tmp",output/"run.json"); return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
