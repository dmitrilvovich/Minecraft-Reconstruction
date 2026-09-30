#include "mcr/experiments/cameras.hpp"
#include "mcr/inference/a2_solver.hpp"
#include "../tests/fixtures/a2_scenes.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <sys/resource.h>

// Measurement and exhaustive data live here, never in the production solver.
using namespace mcr;
namespace {
using Clock=std::chrono::steady_clock;
using Labels=std::vector<PixelLabel>;
using Ids=std::vector<unsigned>;
constexpr std::array modes{"search","decomposition","fixed_hit","both"};
void require(bool b,const char* message) { if(!b) throw std::runtime_error(message); }
struct Exhausted {};
struct Budget final : A2InferenceObserver {
    InferenceStats& stats;
    std::uint64_t nodes;
    Clock::time_point deadline;
    Budget(InferenceStats& s,std::uint64_t n,double seconds):stats(s),nodes(n),
        deadline(Clock::now()+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds))) {}
    void check() const { if(stats.search_nodes>nodes || Clock::now()>=deadline) throw Exhausted{}; }
    void begin(QueryKind,std::span<const A2Domain>) override { check(); }
    void prune(const BasicPruneEvent<A2Domain>&) override { check(); }
    void contradiction(ContradictionReason,std::optional<std::size_t>) override { check(); }
};
struct Run {
    InferenceStats stats;
    A2SearchDiagnostics diag;
    std::uint64_t ns=0,witnesses=0;
    std::string status="UNRESOLVED";
};
template<class F> Run timed(unsigned mode,std::uint64_t nodes,double seconds,F&& fn) {
    Run r;
    Budget budget(r.stats,nodes,seconds);
    const auto start=Clock::now();
    try {
        budget.check();
        const bool sat=fn(r,budget,A2SearchOptions{bool(mode&1U),bool(mode&2U),&r.diag});
        budget.check();
        r.status=sat?"SAT":"UNSAT";
    } catch(const Exhausted&) {}
    r.ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());
    return r;
}
unsigned pack(const A2Domains& d) {
    unsigned x=0;
    for(unsigned v=0;v<d.size();++v) x|=d[v].bits()<<(3*v);
    return x;
}
A2Domains supports(const std::vector<A2World>& worlds,const Ids& ids) {
    A2Domains d(8,A2Domain(0));
    for(auto id:ids) for(unsigned v=0;v<8;++v) d[v]=A2Domain(d[v].bits()|(1U<<code(worlds[id][v])));
    return d;
}
// Scalar reference for abstract controls and witness checks. No automaton/search.
bool direct(const A2Problem& p,const A2World& w) {
    for(const auto& f:p.constraints()) {
        PixelLabel hit=PixelLabel::background;
        for(const auto& s:f.steps()) {
            if(w[s.cell]==A2State::bottom_slab && s.bottom_hit) { hit=PixelLabel::oak; break; }
            if(w[s.cell]==A2State::top_slab && s.top_hit) {
                hit=p.palette()==A2Palette::same_material?PixelLabel::oak:PixelLabel::stone; break;
            }
        }
        if(hit!=f.target()) return false;
    }
    return true;
}
struct RootMetrics { unsigned components=0,largest=0,variables=0,shapes=0,factors=0; };
// Read-only root instrumentation, matching the documented residual dependency
// criterion. Conditioning only removes these edges; this is not a runtime trace.
RootMetrics residual_metrics(const A2Problem& p,const A2Domains& d) {
    RootMetrics out;
    std::array<unsigned,8> parent{},degree{};
    std::array<bool,8> shapes{};
    std::iota(parent.begin(),parent.end(),0);
    const auto find=[&](unsigned v) { while(parent[v]!=v) v=parent[v]; return v; };
    for(const auto& f:p.constraints()) {
        unsigned outputs=0; bool live=true;
        std::vector<unsigned> scope,shape;
        for(const auto& step:f.steps()) {
            unsigned emissions=0,hits=0;
            for(auto s:a2_states) if(d[step.cell].contains(s)) {
                auto e=step.emission(s,p.palette()); emissions|=1U<<code(e);
                if(s!=A2State::air) hits|=1U<<(e!=PixelLabel::background);
            }
            outputs|=emissions&~1U;
            if(std::popcount(emissions)>1) scope.push_back(static_cast<unsigned>(step.cell));
            if(hits==3) shape.push_back(static_cast<unsigned>(step.cell));
            if(!(emissions&1U)) { live=false; break; }
        }
        if(live) outputs|=1;
        if(outputs==(1U<<code(f.target()))) continue;
        require(!scope.empty(),"invalid residual scope after GAC"); ++out.factors;
        for(auto v:scope) { ++degree[v]; parent[find(v)]=find(scope.front()); }
        for(auto v:shape) shapes[v]=true;
    }
    std::array<unsigned,8> sizes{};
    for(unsigned v=0;v<8;++v) { if(degree[v]) { ++sizes[find(v)]; ++out.variables; } out.shapes+=shapes[v]; }
    for(auto n:sizes) { out.components+=n>0; out.largest=std::max(out.largest,n); }
    return out;
}
struct Suite { std::string name; std::vector<unsigned> factors; };
struct Rig {
    std::vector<std::vector<A2RayCell>> steps;
    std::vector<A2AabbReferenceRay> reference;
    std::vector<unsigned> raw;
    std::vector<Suite> suites;
    A2Palette palette;
    explicit Rig(int resolution,A2Palette pal):palette(pal) {
        std::map<std::vector<A2RayCell>,unsigned> intern;
        std::vector<std::vector<unsigned>> cameras;
        const Grid grid;
        for(const auto& camera:experiments::phase_a_cameras(grid,resolution)) {
            std::vector<unsigned> ids;
            for(int y=0;y<resolution;++y) for(int x=0;x<resolution;++x) {
                auto ray=camera.pixel(x,y); A2TraversalRay t(grid,ray,pal);
                std::vector<A2RayCell> key(t.steps().begin(),t.steps().end());
                auto [it,added]=intern.emplace(key,static_cast<unsigned>(steps.size()));
                if(added) { steps.push_back(key); reference.emplace_back(grid,ray,pal); }
                ids.push_back(it->second); raw.push_back(it->second);
            }
            cameras.push_back(std::move(ids));
        }
        const auto add=[&](const std::string& name,unsigned a,unsigned b) {
            std::set<unsigned> rs;
            for(unsigned c=a;c<b;++c) rs.insert(cameras[c].begin(),cameras[c].end());
            suites.push_back({name,{rs.begin(),rs.end()}});
        };
        for(unsigned n:{1U,2U,3U,6U}) add("axis_"+std::to_string(n),0,n);
        add("oblique_1",6,7); add("oblique_2",6,8);
    }
    A2Problem problem(const std::vector<unsigned>& indices,const Labels& labels) const {
        require(indices.size()==labels.size(),"invalid observation");
        std::vector<A2Constraint> fs;
        for(unsigned j=0;j<indices.size();++j) fs.emplace_back(steps.at(indices[j]),labels[j],palette);
        return A2Problem(8,std::move(fs),palette);
    }
};
struct Collector {
    std::ofstream cases,runs,spots;
    std::vector<A2World> worlds;
    unsigned mask,reps,limit;
    std::uint64_t node_budget,cases_count=0,run_count=0,witnesses=0,unresolved=0,queries=0;
    double seconds;
    Collector(const std::string& prefix,unsigned m,unsigned r,unsigned l,std::uint64_t b,double sec):
        cases(prefix+".cases.csv"),runs(prefix+".runs.csv"),spots(prefix+".spots.jsonl"),mask(m),reps(r),limit(l),node_budget(b),seconds(sec) {
        require(cases.good() && runs.good() && spots.good(),"cannot open output");
        cases<<"case_id,kind,suite,truth_id,domains,supports,family_size,balanced_numerator,identified,free_cells,root_consistent,root_domains,gac_gap,residual_components,largest_component,residual_variables,shape_variables,residual_factors,queries_sampled\n";
        runs<<"case_id,mode,operation,repetition,cell,state,status,ns,nodes,branches,factor_updates,gac_calls,fixed_calls,literal_queries,infeasible_queries,decompositions,component_solves,fixed_checks,fixed_rejections,witnesses\n";
        for(unsigned i=0;i<6561;++i) worlds.push_back(a2_world(8,i));
    }
    void write_run(std::uint64_t id,unsigned mode,const char* operation,unsigned rep,int cell,int state,const Run& r) {
        runs<<id<<','<<modes[mode]<<','<<operation<<','<<rep<<','<<cell<<','<<state<<','<<r.status<<','<<r.ns<<','
            <<r.stats.search_nodes<<','<<r.stats.branches<<','<<r.stats.factor_updates<<','<<r.stats.gac_calls<<','<<r.stats.fixed_calls<<','
            <<r.stats.literal_queries<<','<<r.stats.infeasible_queries<<','<<r.diag.decompositions<<','<<r.diag.component_solves<<','
            <<r.diag.fixed_hit_checks<<','<<r.diag.fixed_hit_rejections<<','<<r.witnesses<<'\n';
        ++run_count; unresolved+=r.status=="UNRESOLVED"; witnesses+=r.witnesses;
    }
    void witness(const A2Problem& p,const A2Domains& d,const Ids& ids,const A2World& w,Run& r) {
        require(contains(d,w),"witness violates domains");
        require(std::binary_search(ids.begin(),ids.end(),static_cast<unsigned>(a2_world_id(w))),"witness outside exhaustive family");
        require(direct(p,w),"witness violates independent scalar reference"); ++r.witnesses;
    }
    void spot(const A2Problem& p,const A2Domains& d,const A2Domains& supported,std::uint64_t id,unsigned family_size) {
        spots<<"{\"case_id\":"<<id<<",\"domains\":[";
        for(unsigned v=0;v<8;++v) spots<<(v?",":"")<<d[v].bits();
        spots<<"],\"supported\":[";
        for(unsigned v=0;v<8;++v) spots<<(v?",":"")<<supported[v].bits();
        spots<<"],\"family_size\":"<<family_size<<",\"factors\":[";
        bool first=true;
        for(const auto& f:p.constraints()) {
            spots<<(first?"":",")<<"{\"target\":"<<code(f.target())<<",\"steps\":["; first=false;
            bool fs=true;
            for(const auto& s:f.steps()) {
                spots<<(fs?"":",")<<'['<<s.cell<<','<<s.bottom_hit<<','<<s.top_hit<<']'; fs=false;
            }
            spots<<"]}";
        }
        spots<<"]}\n";
    }
    void collect(const std::string& kind,const std::string& suite,const A2Problem& p,const A2Domains& d,
                 const Ids& ids,unsigned truth,bool sample_queries,bool python_spot) {
        const auto id=cases_count++;
        require(std::is_sorted(ids.begin(),ids.end()),"unsorted family");
        auto expected=supports(worlds,ids);
        InferenceStats gs; const auto root=gac(p,d,gs);
        if(!root.consistent) require(ids.empty(),"unsound root contradiction");
        RootMetrics rm;
        if(root.consistent) rm=residual_metrics(p,root.domains);
        unsigned identified=0,free=0,gap=0;
        std::uint64_t balanced=0;
        for(auto w:ids) {
            unsigned occupied_shapes=0;
            for(unsigned v=0;v<8;++v) occupied_shapes+=bool(mask&(1U<<v)) && worlds[w][v]!=A2State::air;
            balanced+=1ULL<<(std::popcount(mask)-occupied_shapes);
        }
        for(unsigned v=0;v<8;++v) {
            identified+=expected[v].size()==1;
            if(root.consistent) { require((root.domains[v]&expected[v])==expected[v],"unsound GAC"); gap+=root.domains[v].size()-expected[v].size(); }
            bool independent=!ids.empty() && expected[v]==d[v];
            for(auto wid:ids) {
                if(!independent) break;
                auto w=worlds[wid];
                for(auto s:a2_states) if(d[v].contains(s)) {
                    w[v]=s;
                    if(!std::binary_search(ids.begin(),ids.end(),static_cast<unsigned>(a2_world_id(w)))) independent=false;
                }
            }
            free+=independent;
        }
        cases<<id<<','<<kind<<','<<suite<<','<<truth<<','<<pack(d)<<','<<pack(expected)<<','<<ids.size()<<','
            <<(kind=="camera"?balanced:0)<<','<<identified<<','<<free<<','<<root.consistent<<','<<pack(root.domains)<<','<<gap<<','
            <<rm.components<<','<<rm.largest<<','<<rm.variables<<','<<rm.shapes<<','<<rm.factors<<','<<sample_queries<<'\n';
        if(python_spot) spot(p,d,expected,id,static_cast<unsigned>(ids.size()));
        // Rotate modes deterministically to reduce a fixed mode/order bias.
        for(unsigned rep=0;rep<reps;++rep) for(unsigned offset=0;offset<4;++offset) {
            const unsigned mode=static_cast<unsigned>((id+rep+offset)%4);
            A2FeasibilityResult decision{};
            auto dr=timed(mode,node_budget,seconds,[&](Run& r,Budget& b,A2SearchOptions o) {
                decision=exact_feasible(p,d,r.stats,&b,o); return decision.feasible;
            });
            if(dr.status!="UNRESOLVED") {
                require(decision.feasible==!ids.empty(),"wrong feasibility");
                if(decision.witness) witness(p,d,ids,*decision.witness,dr);
            }
            write_run(id,mode,"feasibility",rep,-1,-1,dr);
            A2SupportResult projection{};
            auto pr=timed(mode,node_budget,seconds,[&](Run& r,Budget& b,A2SearchOptions o) {
                projection=exact_supports(p,d,r.stats,&b,o); return projection.feasible;
            });
            if(pr.status!="UNRESOLVED") {
                require(projection.feasible==!ids.empty() && projection.supported==expected,"wrong support projection");
                A2Domains covered(8,A2Domain(0));
                for(const auto& w:projection.witnesses) {
                    witness(p,d,ids,w,pr);
                    for(unsigned v=0;v<8;++v) covered[v]=A2Domain(covered[v].bits()|(1U<<code(w[v])));
                }
                require(covered==expected,"projection lacks witness coverage");
            }
            write_run(id,mode,"projection",rep,-1,-1,pr);
            if(rep==0 && sample_queries) for(unsigned v=0;v<8;++v) for(auto s:a2_states) {
                A2SupportQueryResult answer{};
                auto qr=timed(mode,node_budget,seconds,[&](Run& r,Budget& b,A2SearchOptions o) {
                    answer=query_support(p,d,v,s,r.stats,&b,o); return answer.supported;
                });
                if(qr.status!="UNRESOLVED") {
                    require(answer.supported==expected[v].contains(s),"wrong literal query");
                    if(answer.witness) {
                        witness(p,d,ids,*answer.witness,qr);
                        require(answer.witness->at(v)==s,"lost query assumption");
                    }
                }
                ++queries; write_run(id,mode,"query",0,static_cast<int>(v),static_cast<int>(code(s)),qr);
            }
        }
    }
    void close(const std::string& prefix) {
        cases.close(); runs.close(); spots.close();
        require(!cases.fail() && !runs.fail() && !spots.fail(),"failed to close output");
        rusage usage{}; getrusage(RUSAGE_SELF,&usage);
        std::ofstream meta(prefix+".json");
        meta<<"{\"cases\":"<<cases_count<<",\"runs\":"<<run_count<<",\"direct_queries\":"<<queries
            <<",\"verified_witnesses\":"<<witnesses<<",\"unresolved\":"<<unresolved<<",\"peak_process_rss_kib\":"<<usage.ru_maxrss<<"}\n";
        meta.close(); require(!meta.fail(),"metadata close failed");
    }
};
void pair(std::vector<A2Constraint>& fs,unsigned a,unsigned b,A2Palette pal) {
    fs.emplace_back(std::vector<A2RayCell>{{a,true,false},{b,true,false}},PixelLabel::oak,pal);
    fs.emplace_back(std::vector<A2RayCell>{{a,false,true},{b,false,true}},material(A2State::top_slab,pal),pal);
}
void graph(std::vector<A2Constraint>& fs,unsigned base,bool triangle,A2Palette pal) {
    pair(fs,base,base+1,pal); pair(fs,base,base+2,pal); if(triangle) pair(fs,base+1,base+2,pal);
}
void controls(Collector& c,A2Palette pal) {
    const auto add=[&](const std::string& name,const A2Problem& p,const A2Domains& d) {
        Ids ids;
        for(unsigned i=0;i<c.worlds.size();++i) if(contains(d,c.worlds[i]) && direct(p,c.worlds[i])) ids.push_back(i);
        c.collect("structured",name,p,d,ids,0,true,true);
    };
    for(bool tri:{false,true}) { auto f=test::a2_fixture(tri,pal); add(tri?"physical_triangle":"physical_path",f.problem,f.domains()); }
    const auto domains=[](unsigned active) { A2Domains d(8,A2Domain(1)); for(unsigned v=0;v<active;++v) d[v]=A2Domain(); return d; };
    std::vector<A2Constraint> fs;
    graph(fs,0,false,pal); graph(fs,3,false,pal);
    add("two_paths",A2Problem(8,fs,pal),domains(6));
    std::rotate(fs.begin()+1,fs.begin()+4,fs.end()); pair(fs,4,5,pal);
    add("path_plus_triangle",A2Problem(8,fs,pal),domains(6));
    fs={A2Constraint({{0,true,true},{1,true,true}},PixelLabel::oak,pal)};
    add("fixed_hit",A2Problem(8,fs,pal),domains(2));
    graph(fs,2,false,pal); add("fixed_plus_path",A2Problem(8,fs,pal),domains(5));
}
void budget_tests() {
    const auto f=test::a2_fixture(false,A2Palette::same_material);
    auto attempt=[&](std::uint64_t nodes,double secs) {
        return timed(3,nodes,secs,[&](Run& r,Budget& b,A2SearchOptions o) {return exact_feasible(f.problem,f.domains(),r.stats,&b,o).feasible;});
    };
    const auto time=attempt(1000000,0),nodes=attempt(0,60);
    require(time.status=="UNRESOLVED" && time.stats.search_nodes==0,"zero-time budget failed");
    require(nodes.status=="UNRESOLVED" && nodes.stats.search_nodes==1,"zero-node budget failed");
    const auto ok=attempt(1000000,60); require(ok.status=="SAT","normal budget failed");
    auto d=f.domains(); d[0]=A2Domain(0);
    const auto contradiction=timed(3,0,60,[&](Run& r,Budget& b,A2SearchOptions o) {return exact_supports(f.problem,d,r.stats,&b,o).feasible;});
    require(contradiction.status=="UNSAT" && contradiction.stats.search_nodes==0,"root contradiction budget failed");
    std::cout<<"PASS budget boundaries: zero time, zero nodes, resolved SAT, root UNSAT without search\n";
}
} // namespace
int main(int argc,char** argv) {
    try {
        unsigned mask=0,reps=3,limit=0; int resolution=8; bool same=false,only_controls=false;
        std::uint64_t nodes=1000000; double seconds=60;
        std::string prefix,images_path;
        for(int i=1;i<argc;++i) {
            const std::string a=argv[i];
            if(a=="--self-test") { budget_tests(); return 0; }
            if(a=="--controls") { only_controls=true; continue; }
            require(i+1<argc,"missing argument"); const std::string v=argv[++i];
            if(a=="--mask") mask=static_cast<unsigned>(std::stoul(v));
            else if(a=="--palette") { require(v=="same" || v=="split","invalid palette"); same=v=="same"; }
            else if(a=="--resolution") resolution=std::stoi(v);
            else if(a=="--repetitions") reps=static_cast<unsigned>(std::stoul(v));
            else if(a=="--limit") limit=static_cast<unsigned>(std::stoul(v));
            else if(a=="--output") prefix=v;
            else if(a=="--images") images_path=v;
            else if(a=="--nodes") nodes=std::stoull(v);
            else if(a=="--seconds") seconds=std::stod(v);
            else throw std::invalid_argument("unknown argument "+a);
        }
        require(mask<256 && reps>0 && (resolution==8 || resolution==16) && !prefix.empty(),"invalid configuration");
        const auto pal=same?A2Palette::same_material:A2Palette::split_material;
        Collector c(prefix,mask,reps,limit,nodes,seconds);
        if(only_controls) { controls(c,pal); c.close(prefix); return 0; }
        A2Domains domains(8,A2Domain(3));
        for(unsigned v=0;v<8;++v) if(mask&(1U<<v)) domains[v]=A2Domain(7);
        Ids legal;
        for(unsigned i=0;i<c.worlds.size();++i) if(contains(domains,c.worlds[i])) legal.push_back(i);
        Rig rig(resolution,pal);
        std::vector<Labels> images(6561);
        for(auto id:legal) {
            auto& labels=images[id];
            for(unsigned r=0;r<rig.steps.size();++r) {
                const auto label=rig.reference[r].sample(c.worlds[id]);
                require(A2Constraint(rig.steps[r],label,pal).accepts(c.worlds[id]),"AABB/traversal disagreement");
                labels.push_back(label);
            }
        }
        if(!images_path.empty()) {
            require(resolution==8,"accepted image fixture is 8x8");
            std::ifstream in(images_path,std::ios::binary); require(in.good(),"missing accepted images");
            for(auto r:rig.raw) for(unsigned id=0;id<6561;++id) {
                const int byte=in.get(); require(byte>=0,"truncated accepted images");
                if(!images[id].empty()) require(code(images[id][r])==static_cast<unsigned>(byte),"accepted Python image mismatch");
            }
            require(in.get()==std::char_traits<char>::eof(),"extra accepted image bytes");
        }
        const auto observe=[&](unsigned id,const std::vector<unsigned>& rs) { Labels out; for(auto r:rs) out.push_back(images[id][r]); return out; };
        unsigned si=0;
        for(const auto& suite:rig.suites) {
            std::map<Labels,Ids> families;
            for(auto id:legal) families[observe(id,suite.factors)].push_back(id);
            unsigned fi=0;
            for(const auto& [labels,ids]:families) {
                if(limit && fi>=limit) break;
                c.collect("camera",suite.name,rig.problem(suite.factors,labels),domains,ids,ids.front(),fi%97==0,fi==0);
                ++fi;
            }
            std::mt19937 rng(20260929U+mask*1009U+static_cast<unsigned>(resolution)*97U+si);
            for(unsigned j=0;j<8;++j) {
                const unsigned truth=legal[rng()%legal.size()];
                auto ds=domains; auto rs=suite.factors; auto labels=observe(truth,rs);
                if(j<4) for(unsigned v=0;v<8;++v) {
                    unsigned bits=(1U+rng()%7U)&domains[v].bits();
                    if(j<2) bits|=1U<<code(c.worlds[truth][v]);
                    ds[v]=A2Domain(bits);
                }
                if(j==4 || j==5) {
                    const auto index=static_cast<unsigned>(rng()%rs.size());
                    const auto other=static_cast<PixelLabel>((code(labels[index])+1)%3);
                    if(j==4) labels[index]=other;
                    else { rs.push_back(rs[index]); labels.push_back(other); }
                }
                if(j==6) ds[rng()%8]=A2Domain(0);
                if(j==7) { const unsigned v=rng()%8; ds[v]=A2Domain(ds[v].bits()&~(1U<<code(c.worlds[truth][v]))); }
                Ids ids;
                for(auto id:legal) if(contains(ds,c.worlds[id]) && observe(id,rs)==labels) ids.push_back(id);
                c.collect("perturb_"+std::to_string(j),suite.name,rig.problem(rs,labels),ds,ids,truth,true,j==(si+mask)%8);
            }
            std::cout<<suite.name<<" families="<<families.size()<<" completed="<<fi<<"\n"<<std::flush; ++si;
        }
        c.close(prefix);
        std::cout<<"PASS cases="<<c.cases_count<<" witnesses="<<c.witnesses<<" unresolved="<<c.unresolved<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
