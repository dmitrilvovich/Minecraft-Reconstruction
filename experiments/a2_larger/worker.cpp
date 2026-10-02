// Experiment transport/measurement only. Accepted inference sources are linked unchanged.
#include "mcr/experiments/cameras.hpp"
#include "mcr/inference/a2_solver.hpp"
#include "../../tests/fixtures/a2_scenes.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>

using namespace mcr;
using Clock=std::chrono::steady_clock;
static std::uint64_t now_ns() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); }
static void require(bool b,const char* s) { if(!b) throw std::runtime_error(s); }
static void json_string(const std::string& s) {
    std::cout<<'"'; for(unsigned char c:s) { if(c=='"'||c=='\\') std::cout<<'\\'; if(c>=32) std::cout<<c; } std::cout<<'"';
}
static void rational(const Rational& q) { std::cout<<'['<<q.numerator()<<','<<q.denominator()<<']'; }
static void vec(const Vec3& v) { std::cout<<'['; for(int i=0;i<3;++i) { if(i) std::cout<<','; rational(v[i]); } std::cout<<']'; }
static void steps(std::span<const A2RayCell> s) {
    std::cout<<'['; bool first=true; for(auto x:s) { if(!first) std::cout<<','; first=false; std::cout<<'['<<x.cell<<','<<x.bottom_hit<<','<<x.top_hit<<']'; } std::cout<<']';
}
static void domains(std::span<const A2Domain> d) { std::cout<<'['; for(std::size_t i=0;i<d.size();++i) std::cout<<(i?",":"")<<d[i].bits(); std::cout<<']'; }
static void world(std::span<const A2State> w) { std::cout<<'['; for(std::size_t i=0;i<w.size();++i) std::cout<<(i?",":"")<<code(w[i]); std::cout<<']'; }
static void ray(const Ray& r) { std::cout<<"\"origin\":"; vec(r.origin); std::cout<<",\"direction\":"; vec(r.direction); }

// Geometry preparation calls both existing C++ rendering paths, never inference.
static int rig(int nx,int ny,int nz,int resolution) {
    const Grid grid({nx,ny,nz}); const auto cameras=experiments::phase_a_cameras(grid,resolution);
    std::vector<Ray> rays; std::vector<A2TraversalRay> traversal;
    std::vector<A2AabbReferenceRay> reference;
    std::vector<A2TraversalRay> same_traversal;
    std::vector<A2AabbReferenceRay> same_reference;
    std::map<std::vector<A2RayCell>,std::size_t> factors;
    for(const auto& camera:cameras) for(int y=0;y<resolution;++y) for(int x=0;x<resolution;++x) {
        rays.push_back(camera.pixel(x,y)); traversal.emplace_back(grid,rays.back()); reference.emplace_back(grid,rays.back());
        same_traversal.emplace_back(grid,rays.back(),A2Palette::same_material);
        same_reference.emplace_back(grid,rays.back(),A2Palette::same_material);
        factors.emplace(std::vector<A2RayCell>(traversal.back().steps().begin(),traversal.back().steps().end()),0);
    }
    std::size_t index=0; for(auto& [key,id]:factors) id=index++;
    std::cout<<"{\"kind\":\"rig\",\"shape\":["<<nx<<','<<ny<<','<<nz<<"],\"resolution\":"<<resolution<<",\"cameras\":[";
    for(std::size_t i=0;i<cameras.size();++i) {
        const auto& c=cameras[i]; if(i) std::cout<<','; std::cout<<"{\"name\":"; json_string(c.name);
        std::cout<<",\"center\":"; vec(c.center); std::cout<<",\"forward\":"; vec(c.forward);
        std::cout<<",\"right\":"; vec(c.right); std::cout<<",\"up\":"; vec(c.up);
        std::cout<<",\"fx\":"; rational(c.fx); std::cout<<",\"fy\":"; rational(c.fy);
        std::cout<<",\"cx\":"; rational(c.cx); std::cout<<",\"cy\":"; rational(c.cy); std::cout<<'}';
    }
    std::cout<<"],\"factors\":["; bool first=true;
    for(const auto& [key,id]:factors) { (void)id; if(!first) std::cout<<','; first=false; steps(key); }
    std::cout<<"]}\n";
    std::vector<std::size_t> mapping;
    for(std::size_t i=0;i<rays.size();++i) {
        const auto s=traversal[i].steps(); const auto id=factors.at({s.begin(),s.end()}); mapping.push_back(id);
        std::cout<<"{\"kind\":\"ray\",\"index\":"<<i<<",\"camera\":"<<i/(resolution*resolution)
                 <<",\"factor\":"<<id<<','; ray(rays[i]); std::cout<<"}\n";
    }
    std::cout<<"{\"kind\":\"rig_end\"}\n"<<std::flush;
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line); std::uint64_t token; if(!(in>>token)) break;
        A2World w(grid.size()); for(auto& s:w) { unsigned v; require(bool(in>>v)&&v<3,"invalid render state"); s=static_cast<A2State>(v); }
        std::vector<int> labels(factors.size(),-1);
        for(std::size_t i=0;i<rays.size();++i) {
            const auto a=reference[i].sample(w),b=traversal[i].sample(w);
            require(a==b,"C++ AABB/traversal disagreement on source truth");
            const auto sa=same_reference[i].sample(w),sb=same_traversal[i].sample(w);
            require(sa==sb && code(sa)==(code(a)==1?2:code(a)),"same-material C++ renderer disagreement");
            auto& label=labels[mapping[i]]; require(label<0||label==int(code(a)),"inconsistent collapsed geometry"); label=code(a);
        }
        std::cout<<"{\"kind\":\"render\",\"token\":"<<token<<",\"split_material\":[";
        for(std::size_t i=0;i<labels.size();++i) std::cout<<(i?",":"")<<labels[i];
        std::cout<<"],\"same_material\":[";
        // Both palette-specific rendering paths have been checked on every ray.
        for(std::size_t i=0;i<labels.size();++i) std::cout<<(i?",":"")<<(labels[i]==1?2:labels[i]);
        std::cout<<"],\"all_raw_rays_checked\":"<<rays.size()<<"}\n"<<std::flush;
    }
    return 0;
}
static int anchor(bool triangle,bool same) {
    const auto f=test::a2_fixture(triangle,same?A2Palette::same_material:A2Palette::split_material);
    std::cout<<"{\"shape\":[2,2,2],\"domains\":"; domains(f.domains()); std::cout<<",\"rays\":[";
    for(std::size_t i=0;i<f.rays.size();++i) { if(i) std::cout<<','; std::cout<<'{'; ray(f.rays[i]); std::cout<<",\"target\":"<<code(f.targets[i])<<'}'; }
    std::cout<<"],\"factors\":[";
    for(std::size_t i=0;i<f.problem.constraints().size();++i) { if(i) std::cout<<','; const auto& c=f.problem.constraints()[i]; std::cout<<"{\"target\":"<<code(c.target())<<",\"steps\":"; steps(c.steps()); std::cout<<'}'; }
    std::cout<<"]}\n"; return 0;
}
struct Exhausted { const char* reason; };
struct Budget final:A2InferenceObserver {
    InferenceStats& stats; std::uint64_t nodes,start; double seconds;
    Budget(InferenceStats& s,std::uint64_t n,std::uint64_t t,double sec):stats(s),nodes(n),start(t),seconds(sec) {}
    void check() const { if(stats.search_nodes>nodes) throw Exhausted{"node"}; if(double(now_ns()-start)/1e9>=seconds) throw Exhausted{"time"}; }
    void begin(QueryKind,std::span<const A2Domain>) override { check(); }
    void prune(const BasicPruneEvent<A2Domain>&) override { check(); }
    void contradiction(ContradictionReason,std::optional<std::size_t>) override { check(); }
};
static void root_metrics(const A2Problem& p,const A2Domains& d,bool consistent) {
    std::vector<std::size_t> parent(d.size()),degree(d.size()); std::vector<bool> shape(d.size());
    std::iota(parent.begin(),parent.end(),0); const auto find=[&](std::size_t v) { while(v!=parent[v]) v=parent[v]; return v; };
    std::size_t factors=0;
    if(consistent) for(const auto& f:p.constraints()) {
        unsigned outputs=0; bool live=true; std::vector<std::size_t> scope,shapes;
        for(const auto& s:f.steps()) {
            unsigned emissions=0,hits=0; for(auto state:a2_states) if(d[s.cell].contains(state)) {
                const auto e=s.emission(state,p.palette()); emissions|=1U<<code(e); if(state!=A2State::air) hits|=1U<<(e!=PixelLabel::background);
            }
            outputs|=emissions&~1U; if(std::popcount(emissions)>1) scope.push_back(s.cell); if(hits==3U) shapes.push_back(s.cell);
            if(!(emissions&1U)) { live=false; break; }
        }
        if(live) outputs|=1U;
        if(outputs==(1U<<code(f.target()))) continue;
        require(!scope.empty(),"empty non-entailed residual scope"); ++factors;
        for(auto v:scope) { ++degree[v]; parent[find(v)]=find(scope.front()); }
        for(auto v:shapes) shape[v]=true;
    }
    std::map<std::size_t,std::vector<std::size_t>> components;
    for(std::size_t v=0;v<d.size();++v) if(degree[v]) components[find(v)].push_back(v);
    std::cout<<"{\"consistent\":"<<(consistent?"true":"false")<<",\"domains\":"; domains(d);
    std::cout<<",\"factors\":"<<factors<<",\"shape_variables\":["; bool first=true;
    for(std::size_t v=0;v<d.size();++v) if(shape[v]) { std::cout<<(first?"":",")<<v; first=false; }
    std::cout<<"],\"components\":["; first=true;
    for(const auto& [id,vs]:components) { (void)id; if(!first) std::cout<<','; first=false; std::cout<<'['; for(std::size_t i=0;i<vs.size();++i) std::cout<<(i?",":"")<<vs[i]; std::cout<<']'; }
    std::cout<<"]}";
}
static int worker(const char* control_path,bool fault_tests) {
    int fd=open(control_path,O_RDWR); require(fd>=0,"control open failed");
    auto* control=static_cast<std::uint64_t*>(mmap(nullptr,24,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0)); require(control!=MAP_FAILED,"control mmap failed"); close(fd);
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line); std::uint64_t sequence,node_limit; unsigned mode,palette; std::size_t n,m; int cell,state; char op; double seconds;
        require(bool(in>>sequence>>mode>>op>>node_limit>>seconds>>palette>>n>>m>>cell>>state),"invalid call header");
        require(n>0&&n<=12&&mode<4&&palette<2&&m<1000000,"invalid call range");
        A2Domains initial; for(std::size_t i=0;i<n;++i) { unsigned b; require(bool(in>>b)&&b<=7,"bad domain"); initial.emplace_back(b); }
        const auto pal=palette?A2Palette::same_material:A2Palette::split_material; std::vector<A2Constraint> factors;
        for(std::size_t i=0;i<m;++i) {
            unsigned target; std::size_t count; require(bool(in>>target>>count)&&target<3&&count<=n,"bad factor"); std::vector<A2RayCell> ss;
            for(std::size_t j=0;j<count;++j) { std::size_t v; unsigned b,t; require(bool(in>>v>>b>>t)&&v<n&&b<2&&t<2,"bad step"); ss.push_back({v,bool(b),bool(t)}); }
            factors.emplace_back(std::move(ss),static_cast<PixelLabel>(target),pal);
        }
        const A2Problem problem(n,std::move(factors),pal); InferenceStats stats; A2SearchDiagnostics diag;
        A2Domains supported; std::vector<A2World> witnesses; A2PropagationResult root{};
        std::string status="UNRESOLVED",reason; bool technical=false;
        const auto start=now_ns(); __atomic_store_n(control+2,0,__ATOMIC_RELEASE); __atomic_store_n(control+1,start,__ATOMIC_RELEASE); __atomic_store_n(control,sequence,__ATOMIC_RELEASE);
        Budget budget(stats,node_limit,start,seconds); const A2SearchOptions options{bool(mode&1U),bool(mode&2U),&diag};
        try {
            budget.check(); bool sat=false;
            if(op=='F') { auto r=exact_feasible(problem,initial,stats,&budget,options); sat=r.feasible; if(r.witness) witnesses.push_back(std::move(*r.witness)); }
            else if(op=='P') { auto r=exact_supports(problem,initial,stats,&budget,options); sat=r.feasible; supported=std::move(r.supported); witnesses=std::move(r.witnesses); }
            else if(op=='Q') { auto r=query_support(problem,initial,cell,static_cast<A2State>(state),stats,&budget,options); sat=r.supported; if(r.witness) witnesses.push_back(std::move(*r.witness)); }
            else if(op=='G') { root=gac(problem,initial,stats,&budget); sat=root.consistent; }
            else if(fault_tests&&op=='H') { std::this_thread::sleep_for(std::chrono::seconds(10)); }
            else if(fault_tests&&op=='M') { std::vector<char> huge(5ULL*1024*1024*1024,1); sat=huge.back()!=0; }
            else if(fault_tests&&op=='X') { _exit(73); }
            else throw std::runtime_error("unknown or disabled operation");
            budget.check(); status=sat?"SAT":"UNSAT";
        } catch(const Exhausted& e) { reason=e.reason; }
        catch(const std::bad_alloc&) { reason="memory"; }
        catch(const std::exception& e) { technical=true; reason=e.what(); }
        const auto end=now_ns(); __atomic_store_n(control+2,end,__ATOMIC_RELEASE);
        if(status=="UNRESOLVED") { supported.clear(); witnesses.clear(); }
        rusage usage{}; getrusage(RUSAGE_SELF,&usage);
        std::cout<<"{\"sequence\":"<<sequence<<",\"status\":"; if(technical) std::cout<<"null"; else json_string(status);
        std::cout<<",\"reason\":"; json_string(reason); std::cout<<",\"technical_error\":"<<(technical?"true":"false")<<",\"ns\":"<<end-start<<",\"start_ns\":"<<start<<",\"worker_peak_rss_bytes\":"<<usage.ru_maxrss*1024ULL;
        std::cout<<",\"stats\":{\"search_nodes\":"<<stats.search_nodes<<",\"branches\":"<<stats.branches<<",\"gac_calls\":"<<stats.gac_calls<<",\"factor_updates\":"<<stats.factor_updates<<",\"deletions\":"<<stats.deletions<<",\"literal_queries\":"<<stats.literal_queries<<",\"infeasible_queries\":"<<stats.infeasible_queries<<",\"fixed_calls\":"<<stats.fixed_calls<<",\"decompositions\":"<<diag.decompositions<<",\"component_solves\":"<<diag.component_solves<<",\"fixed_hit_checks\":"<<diag.fixed_hit_checks<<",\"fixed_hit_rejections\":"<<diag.fixed_hit_rejections<<",\"witness_count\":"<<witnesses.size()<<'}';
        std::cout<<",\"supported\":"; if(op=='P'&&status!="UNRESOLVED"&&!technical) domains(supported); else std::cout<<"null";
        std::cout<<",\"witnesses\":["; for(std::size_t i=0;i<witnesses.size();++i) { if(i) std::cout<<','; world(witnesses[i]); } std::cout<<']';
        std::cout<<",\"root\":"; if(op=='G'&&status!="UNRESOLVED"&&!technical) root_metrics(problem,root.domains,root.consistent); else std::cout<<"null";
        std::cout<<"}\n"<<std::flush;
    }
    munmap(control,24); return 0;
}
int main(int argc,char** argv) {
    try {
        if(argc==6&&std::string(argv[1])=="rig") return rig(std::stoi(argv[2]),std::stoi(argv[3]),std::stoi(argv[4]),std::stoi(argv[5]));
        if(argc==4&&std::string(argv[1])=="anchor") return anchor(std::stoi(argv[2]),std::stoi(argv[3]));
        if(argc>=3&&std::string(argv[1])=="worker") return worker(argv[2],argc==4&&std::string(argv[3])=="--preflight-faults");
        throw std::runtime_error("expected rig/anchor/worker command");
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 2; }
}
