#include "test.hpp"
#include "fixtures/a2_scenes.hpp"
#include "mcr/experiments/a0.hpp"
#include "mcr/experiments/cameras.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>

using namespace mcr;
namespace {
using Ids=std::vector<std::uint32_t>;
using Observation=std::vector<PixelLabel>;
using Families=std::map<Observation,Ids>;
const std::array<const char*,4> modes{"search","decomposition","fixed_hit","both"};
struct Reader {
    std::ifstream in;
    explicit Reader(const std::filesystem::path& p):in(p,std::ios::binary) { CHECK(in.good()); }
    unsigned byte() { const auto b=in.get(); CHECK(b!=std::char_traits<char>::eof()); return static_cast<unsigned>(b); }
    std::uint32_t u32() { std::uint32_t x=0; for(unsigned k=0;k<4;++k) x|=byte()<<(8*k); return x; }
    std::string text(std::size_t n) { CHECK(n<1000); std::string s; while(n--) s+=static_cast<char>(byte()); return s; }
    A2Domains masks(std::size_t n=8) { A2Domains d; while(n--) d.emplace_back(byte()); return d; }
    Ids ids() {
        Ids result(u32()); CHECK(result.size()<=6561);
        for(auto& id:result) { id=u32(); CHECK(id<6561); }
        CHECK(std::is_sorted(result.begin(),result.end()));
        CHECK(std::adjacent_find(result.begin(),result.end())==result.end()); return result;
    }
    void eof() { CHECK(in.get()==std::char_traits<char>::eof()); }
};
struct Case {
    Ids ids;
    A2Domains domains;
    bool consistent;
    A2Domains root,expected;
    explicit Case(Reader& r):ids(r.ids()),domains(r.masks()),consistent(r.byte()!=0),root(r.masks()),expected(r.masks()) {}
};
A2Domains supports(std::span<const A2World> worlds,const Ids& ids,std::size_t n=8) {
    A2Domains out(n,A2Domain(0));
    for(auto id:ids) for(CellId v=0;v<n;++v) out[v]=A2Domain(out[v].bits() | (1U<<code(worlds[id][v])));
    return out;
}
// Independent scalar evaluation, with no production emit/transition/GAC/search calls.
PixelLabel direct(const A2Constraint& f,std::span<const A2State> w) {
    for(const auto& step:f.steps()) {
        if(w[step.cell]==A2State::bottom_slab && step.bottom_hit) return PixelLabel::oak;
        if(w[step.cell]==A2State::top_slab && step.top_hit)
            return f.palette()==A2Palette::same_material ? PixelLabel::oak : PixelLabel::stone;
    }
    return PixelLabel::background;
}
A2Domains local_support(const A2Constraint& f,const A2Domains& domains) {
    A2Domains out(domains.size(),A2Domain(0));
    A2World w(domains.size(),A2State::air);
    std::function<void(std::size_t)> visit=[&](std::size_t j) {
        if(j==f.cells().size()) {
            if(direct(f,w)==f.target()) for(auto v:f.cells()) out[v]=A2Domain(out[v].bits() | (1U<<code(w[v])));
            return;
        }
        const auto v=f.cells()[j];
        for(auto s:a2_states) if(domains[v].contains(s)) { w[v]=s; visit(j+1); }
    };
    visit(0); return out;
}
struct Audit final : A2InferenceObserver {
    const A2Problem& problem;
    std::span<const A2World> worlds;
    const Ids& family;
    A2Domains ceiling,current,expected;
    std::uint64_t contexts=0,prunes=0,deletions=0,contradictions=0,factor_events=0;
    bool feasible=false;
    Audit(const A2Problem& p,std::span<const A2World> w,const Ids& f):problem(p),worlds(w),family(f) {}
    void begin(QueryKind,std::span<const A2Domain> domains) override {
        CHECK(domains.size()==ceiling.size());
        current.assign(domains.begin(),domains.end());
        for(CellId v=0;v<domains.size();++v) CHECK((domains[v] & ceiling[v])==domains[v]);
        Ids conditioned;
        for(auto id:family) if(contains(domains,worlds[id])) conditioned.push_back(id);
        feasible=!conditioned.empty(); expected=supports(worlds,conditioned,domains.size()); ++contexts;
    }
    void prune(const BasicPruneEvent<A2Domain>& e) override {
        CHECK(current.at(e.cell)==e.before && e.before!=e.after);
        CHECK((e.after & e.before)==e.after && (e.after & expected[e.cell])==expected[e.cell]);
        if(e.factor) {
            CHECK(e.reason==PruneReason::ray_arc_support && *e.factor<problem.constraints().size());
            const auto& f=problem.constraints()[*e.factor];
            CHECK(std::find(f.cells().begin(),f.cells().end(),e.cell)!=f.cells().end());
            const auto local=local_support(f,current);
            CHECK((e.after & local[e.cell])==local[e.cell]); // original factor really implies the deletion
            ++factor_events;
        } else CHECK(e.reason==PruneReason::unsupported_state);
        current[e.cell]=e.after; ++prunes; deletions+=e.before.size()-e.after.size();
    }
    void contradiction(ContradictionReason reason,std::optional<std::size_t> factor) override {
        CHECK(!feasible);
        if(factor) {
            CHECK(*factor<problem.constraints().size() && reason==ContradictionReason::no_ray_support);
            const auto& f=problem.constraints()[*factor];
            if(f.cells().empty()) CHECK(f.target()!=PixelLabel::background);
            else CHECK(local_support(f,current)[f.cells().front()].empty());
            ++factor_events;
        } else CHECK(std::any_of(current.begin(),current.end(),[](auto d){return d.empty();}));
        ++contradictions;
    }
};
struct Distribution {
    std::map<std::uint64_t,std::uint64_t> histogram;
    std::uint64_t sum=0,count=0;
    void add(std::uint64_t x) { ++histogram[x]; sum+=x; ++count; }
    std::uint64_t percentile(unsigned p) const {
        std::uint64_t seen=0,threshold=(count*p+99)/100;
        for(auto [value,n]:histogram) { seen+=n; if(seen>=threshold) return value; }
        return 0;
    }
    void json(std::ostream& o) const {
        o<<"{\"calls\":"<<count<<",\"sum\":"<<sum<<",\"median\":"<<percentile(50)
         <<",\"p95\":"<<percentile(95)<<",\"maximum\":"<<(histogram.empty()?0:histogram.rbegin()->first)<<",\"histogram\":{";
        bool first=true; for(auto [v,n]:histogram) { o<<(first?"":",")<<'"'<<v<<"\":"<<n; first=false; } o<<"}}";
    }
};
struct Totals {
    std::uint64_t cases=0,infeasible=0,queries=0,supported_queries=0,witnesses=0,supported_literals=0,gac_gap=0;
    std::uint64_t contexts=0,prunes=0,deletions=0,contradictions=0,factor_events=0;
    std::uint64_t fixed_calls=0,decompositions=0,components=0,fixed_checks=0,fixed_rejections=0;
    Distribution decision_nodes,decision_branches,projection_nodes,projection_branches,query_nodes,query_branches,unsat_query_nodes;
    void json(std::ostream& o) const {
        o<<"{\"cases\":"<<cases<<",\"infeasible\":"<<infeasible<<",\"direct_queries\":"<<queries
         <<",\"supported_queries\":"<<supported_queries<<",\"witnesses\":"<<witnesses<<",\"supported_literals\":"<<supported_literals
         <<",\"gac_unsupported_survivors\":"<<gac_gap<<",\"audit_contexts\":"<<contexts<<",\"audited_prunes\":"<<prunes
         <<",\"audited_deletions\":"<<deletions<<",\"audited_contradictions\":"<<contradictions<<",\"audited_factor_ids\":"<<factor_events
         <<",\"fixed_calls\":"<<fixed_calls<<",\"decompositions\":"<<decompositions<<",\"component_solves\":"<<components
         <<",\"fixed_hit_checks\":"<<fixed_checks<<",\"fixed_hit_rejections\":"<<fixed_rejections;
        const std::array<std::pair<const char*,const Distribution*>,7> ds{{
            {"decision_nodes",&decision_nodes},{"decision_branches",&decision_branches},
            {"projection_nodes",&projection_nodes},{"projection_branches",&projection_branches},
            {"query_nodes",&query_nodes},{"query_branches",&query_branches},{"unsat_query_nodes",&unsat_query_nodes}}};
        for(auto [name,d]:ds) { o<<",\""<<name<<"\":"; d->json(o); } o<<'}';
    }
};
void check_witness(const A2World& w,const A2Problem& p,const Case& c,std::span<const A2World> worlds,Totals& t) {
    CHECK(w.size()==8 && contains(c.domains,w));
    const auto id=static_cast<std::uint32_t>(a2_world_id(w));
    CHECK(id<worlds.size() && worlds[id]==w && std::binary_search(c.ids.begin(),c.ids.end(),id));
    for(const auto& f:p.constraints()) CHECK(direct(f,w)==f.target());
    CHECK(p.accepts(w)); ++t.witnesses;
}
void validate(const A2Problem& p,const Case& c,std::span<const A2World> worlds,const Ids& family,std::array<Totals,4>& totals) {
    Ids conditioned;
    for(auto id:family) if(contains(c.domains,worlds[id])) conditioned.push_back(id);
    CHECK(conditioned==c.ids && supports(worlds,c.ids)==c.expected);
    for(unsigned mode=0;mode<4;++mode) {
        auto& t=totals[mode]; Audit audit(p,worlds,family); audit.ceiling=c.domains;
        A2SearchDiagnostics diag;
        const A2SearchOptions options{(mode & 1U)!=0,(mode & 2U)!=0,&diag};
        InferenceStats local_stats;
        const auto local=gac(p,c.domains,local_stats,&audit);
        CHECK(local.consistent==c.consistent);
        if(local.consistent) CHECK(local.domains==c.root);
        InferenceStats ds,ps;
        const auto decision=exact_feasible(p,c.domains,ds,&audit,options);
        CHECK(decision.feasible==!c.ids.empty() && decision.feasible==decision.witness.has_value());
        if(decision.witness) check_witness(*decision.witness,p,c,worlds,t);
        const auto projection=exact_supports(p,c.domains,ps,&audit,options);
        CHECK(projection.feasible==decision.feasible && projection.supported==c.expected);
        CHECK(projection.root_domains==local.domains);
        A2Domains covered(8,A2Domain(0));
        for(const auto& w:projection.witnesses) {
            check_witness(w,p,c,worlds,t);
            for(CellId v=0;v<8;++v) covered[v]=A2Domain(covered[v].bits() | (1U<<code(w[v])));
        }
        CHECK(covered==c.expected);
        if(!projection.feasible) CHECK(projection.witnesses.empty());
        t.decision_nodes.add(ds.search_nodes); t.decision_branches.add(ds.branches);
        t.projection_nodes.add(ps.search_nodes); t.projection_branches.add(ps.branches);
        t.fixed_calls+=ds.fixed_calls+ps.fixed_calls;
        for(CellId v=0;v<8;++v) {
            if(local.consistent) t.gac_gap+=local.domains[v].size()-c.expected[v].size();
            t.supported_literals+=c.expected[v].size();
            for(auto s:a2_states) {
                audit.ceiling=c.domains; audit.ceiling[v]=audit.ceiling[v] & A2Domain::singleton(s);
                InferenceStats qs;
                const auto query=query_support(p,c.domains,v,s,qs,&audit,options);
                CHECK(query.supported==c.expected[v].contains(s) && query.supported==query.witness.has_value());
                if(query.witness) { check_witness(*query.witness,p,c,worlds,t); CHECK((*query.witness)[v]==s); ++t.supported_queries; }
                else t.unsat_query_nodes.add(qs.search_nodes);
                t.query_nodes.add(qs.search_nodes); t.query_branches.add(qs.branches);
                CHECK(qs.envelope_calls==0 && qs.envelope_advances==0); t.fixed_calls+=qs.fixed_calls; ++t.queries;
            }
        }
        CHECK(ds.envelope_calls==0 && ps.envelope_calls==0 && local_stats.envelope_calls==0);
        CHECK((options.decompose || diag.decompositions==0) && (options.fixed_hit || diag.fixed_hit_checks==0));
        ++t.cases; t.infeasible+=c.ids.empty();
        t.contexts+=audit.contexts; t.prunes+=audit.prunes; t.deletions+=audit.deletions;
        t.contradictions+=audit.contradictions; t.factor_events+=audit.factor_events;
        t.decompositions+=diag.decompositions; t.components+=diag.component_solves;
        t.fixed_checks+=diag.fixed_hit_checks; t.fixed_rejections+=diag.fixed_hit_rejections;
    }
}
struct Rig {
    A2Palette palette;
    std::vector<std::vector<A2RayCell>> factors;
    std::vector<experiments::ViewSuite> suites;
    std::vector<A2World> worlds;
    std::vector<Observation> images;
    std::vector<unsigned> raw;
    Rig(A2Palette p,Reader& reference):palette(p) {
        const Grid grid;
        for(unsigned id=0;id<6561;++id) worlds.push_back(a2_world(8,id));
        images.resize(6561);
        std::map<std::vector<A2RayCell>,std::size_t> interned;
        std::vector<std::vector<std::size_t>> cameras;
        for(const auto& camera:experiments::phase_a_cameras(grid)) {
            std::vector<std::size_t> indices;
            for(int row=0;row<8;++row) for(int col=0;col<8;++col) {
                const auto ray=camera.pixel(col,row);
                const A2TraversalRay traversal(grid,ray,p); const A2AabbReferenceRay aabb(grid,ray,p);
                const std::vector<A2RayCell> steps(traversal.steps().begin(),traversal.steps().end());
                const auto [it,inserted]=interned.emplace(steps,factors.size());
                if(inserted) factors.push_back(steps);
                indices.push_back(it->second);
                for(unsigned id=0;id<6561;++id) {
                    const auto label=aabb.sample(worlds[id]);
                    CHECK(label==traversal.sample(worlds[id]) && code(label)==reference.byte());
                    if(inserted) images[id].push_back(label);
                    else CHECK(images[id][it->second]==label);
                    raw.push_back(code(label));
                }
            }
            cameras.push_back(indices);
        }
        reference.eof();
        const auto add=[&](std::string name,unsigned views,std::size_t first,std::size_t end) {
            std::set<std::size_t> indices;
            for(auto j=first;j<end;++j) indices.insert(cameras[j].begin(),cameras[j].end());
            suites.push_back({name,views,{indices.begin(),indices.end()}});
        };
        for(unsigned views=1;views<=6;++views) add("axis_"+std::to_string(views),views,0,views);
        add("oblique_1",1,6,7); add("oblique_2",2,6,8);
    }
    Observation observation(unsigned id,std::span<const std::size_t> indices) const {
        Observation out; for(auto r:indices) out.push_back(images.at(id).at(r)); return out;
    }
    A2Problem problem(std::span<const std::size_t> indices,std::span<const PixelLabel> labels) const {
        CHECK(indices.size()==labels.size());
        std::vector<A2Constraint> fs;
        for(std::size_t j=0;j<indices.size();++j) fs.emplace_back(factors.at(indices[j]),labels[j],palette);
        return A2Problem(8,fs,palette);
    }
};
struct PaletteReport {
    std::map<std::string,std::size_t> sizes;
    std::array<Totals,4> camera,restricted,physical;
    std::vector<std::vector<A2Domains>> per_world;
    void json(std::ostream& o) const {
        o<<"{\"classes_by_suite\":{"; bool first=true;
        for(auto [name,n]:sizes) { o<<(first?"":",")<<'"'<<name<<"\":"<<n; first=false; } o<<'}';
        const std::array<std::pair<const char*,const std::array<Totals,4>*>,3> parts{{
            {"camera",&camera},{"restricted_camera",&restricted},{"physical_restrictions",&physical}}};
        for(auto [name,ts]:parts) {
            o<<",\""<<name<<"\":{";
            for(unsigned mode=0;mode<4;++mode) { o<<(mode?",":"")<<'"'<<modes[mode]<<"\":"; (*ts)[mode].json(o); } o<<'}';
        }
        o<<'}';
    }
};
PaletteReport camera_cases(const std::filesystem::path& dir,const std::string& name,const Rig& rig) {
    Reader in(dir/(name+"_cases.bin")); CHECK(in.text(8)=="MCRA2C1\n" && in.u32()==rig.factors.size());
    for(const auto& steps:rig.factors) {
        CHECK(in.u32()==steps.size());
        for(const auto& s:steps) { CHECK(in.u32()==s.cell); CHECK(in.byte()==s.bottom_hit); CHECK(in.byte()==s.top_hit); }
    }
    CHECK(in.u32()==rig.suites.size()); PaletteReport report;
    for(const auto& suite:rig.suites) {
        CHECK(in.text(in.u32())==suite.name && in.u32()==suite.views && in.u32()==suite.factors.size());
        for(auto r:suite.factors) CHECK(in.u32()==r);
        Families families;
        for(unsigned id=0;id<6561;++id) families[rig.observation(id,suite.factors)].push_back(id);
        CHECK(in.u32()==families.size()); report.sizes[suite.name]=families.size();
        std::vector<bool> visited(6561,false); std::vector<A2Domains> per_world(6561);
        for(std::size_t k=0;k<families.size();++k) {
            const Case c(in); CHECK(!c.ids.empty() && c.domains==A2Domains(8));
            const auto labels=rig.observation(c.ids.front(),suite.factors);
            const auto& family=families.at(labels); CHECK(family==c.ids);
            try { validate(rig.problem(suite.factors,labels),c,rig.worlds,family,report.camera); }
            catch(const std::exception& e) { throw std::runtime_error(name+" "+suite.name+" world="+std::to_string(c.ids.front())+": "+e.what()); }
            for(auto id:c.ids) { CHECK(!visited[id]); visited[id]=true; per_world[id]=c.expected; }
        }
        CHECK(std::all_of(visited.begin(),visited.end(),[](bool b){return b;}));
        if(suite.name.starts_with("axis_") && !report.per_world.empty())
            for(unsigned id=0;id<6561;++id) for(CellId v=0;v<8;++v)
                CHECK((per_world[id][v] & report.per_world.back()[id][v])==per_world[id][v]);
        report.per_world.push_back(std::move(per_world));
        std::cout<<name<<' '<<suite.name<<": "<<families.size()<<" complete families x four modes\n"<<std::flush;
    }
    CHECK(in.u32()==1024);
    for(unsigned k=0;k<1024;++k) {
        const auto n=in.u32(); CHECK(n<=rig.factors.size()+1);
        std::vector<std::size_t> indices; Observation labels;
        for(unsigned j=0;j<n;++j) { indices.push_back(in.u32()); labels.push_back(static_cast<PixelLabel>(in.byte())); }
        const Case c(in); Ids family;
        for(unsigned id=0;id<6561;++id) if(rig.observation(id,indices)==labels) family.push_back(id);
        try { validate(rig.problem(indices,labels),c,rig.worlds,family,report.restricted); }
        catch(const std::exception& e) { throw std::runtime_error(name+" restricted="+std::to_string(k)+": "+e.what()); }
    }
    for(bool triangle:{false,true}) {
        const auto fixture=test::a2_fixture(triangle,rig.palette); Ids family;
        std::vector<A2AabbReferenceRay> rays;
        for(const auto& ray:fixture.rays) rays.emplace_back(fixture.grid,ray,rig.palette);
        for(unsigned id=0;id<6561;++id) {
            bool accepted=true;
            for(std::size_t r=0;r<rays.size();++r) accepted=accepted && rays[r].sample(rig.worlds[id])==fixture.targets[r];
            if(accepted) family.push_back(id);
        }
        for(unsigned k=0;k<512;++k) {
            const Case c(in);
            if(k==511) {
                CHECK(c.consistent && c.root==fixture.domains() && c.ids.size()==(triangle?0U:2U));
                for(auto v:fixture.active) CHECK(c.expected[v].bits()==(triangle?0U:6U));
            }
            validate(fixture.problem,c,rig.worlds,family,report.physical);
        }
    }
    in.eof(); return report;
}
std::uint64_t boundaries(const std::filesystem::path& dir) {
    std::ifstream in(dir/"boundaries.txt"); std::string magic; unsigned count;
    CHECK(in>>magic>>count); CHECK(magic=="MCRA2B1" && count==4550);
    const Grid grid({1,1,1});
    for(auto palette:{A2Palette::split_material,A2Palette::same_material}) for(unsigned k=0;k<count;++k) {
        std::array<Rational,6> coords;
        for(auto& q:coords) { std::int64_t n,d; CHECK(in>>n>>d); q=Rational(n,d); }
        const Ray ray({coords[0],coords[1],coords[2]},{coords[3],coords[4],coords[5]});
        const A2AabbReferenceRay aabb(grid,ray,palette); const A2TraversalRay traversal(grid,ray,palette);
        for(auto s:a2_states) {
            unsigned label; CHECK(in>>label); const A2World w{s};
            CHECK(code(aabb.sample(w))==label && code(traversal.sample(w))==label);
        }
    }
    in>>std::ws; CHECK(in.eof()); return 2*count*3;
}
std::uint64_t local_factors(const std::filesystem::path& dir) {
    Reader in(dir/"local_supports.bin"); CHECK(in.text(8)=="MCRA2F1\n" && in.u32()==196608);
    for(auto palette:{A2Palette::split_material,A2Palette::same_material}) for(unsigned flags=0;flags<64;++flags)
        for(unsigned encoded=0;encoded<512;++encoded) for(unsigned target=0;target<3;++target) {
            A2Domains domains; for(unsigned v=0;v<3;++v) domains.emplace_back((encoded>>(3*v)) & 7U);
            const std::array<CellId,3> cells{2,0,1}; std::vector<A2RayCell> steps;
            for(unsigned j=0;j<3;++j) steps.push_back({cells[j],(flags & (1U<<(2*j)))!=0,(flags & (2U<<(2*j)))!=0});
            const A2Constraint f(steps,static_cast<PixelLabel>(target),palette);
            const auto expected=local_support(f,domains); const auto actual=factor_supports(f,domains);
            const bool feasible=!expected[0].empty(); CHECK(actual.feasible==feasible && in.byte()==static_cast<unsigned>(feasible));
            for(unsigned j=0;j<3;++j) {
                CHECK(in.byte()==expected[cells[j]].bits());
                if(feasible) CHECK(actual.masks[j]==expected[cells[j]]);
            }
            if(!feasible) CHECK(actual.masks.empty());
        }
    in.eof(); return 196608;
}
void audit_faults() {
    const A2Problem p(8,{{{{0,true,false}},PixelLabel::oak},{{{1,true,false}},PixelLabel::oak}});
    std::vector<A2World> worlds{A2World(8,A2State::bottom_slab)}; const Ids family{0};
    Audit audit(p,worlds,family); audit.ceiling=A2Domains(8); audit.begin(QueryKind::gac,audit.ceiling);
    check_throws<std::runtime_error>([&]{ audit.prune({0,A2Domain(),A2Domain(1),PruneReason::ray_arc_support,0}); });
    check_throws<std::runtime_error>([&]{ audit.prune({0,A2Domain(),A2Domain(2),PruneReason::ray_arc_support,1}); });
    check_throws<std::runtime_error>([&]{ audit.contradiction(ContradictionReason::no_ray_support,0); });
    audit.ceiling[0]=A2Domain(2);
    check_throws<std::runtime_error>([&]{ audit.begin(QueryKind::gac,A2Domains(8)); });
    // Even an in-range factor containing the right cell must imply its deletion.
    const A2Problem overlap(8,{{{{0,true,false}},PixelLabel::oak},{{{0,false,true}},PixelLabel::background}});
    Audit overlapping(overlap,worlds,family); overlapping.ceiling=A2Domains(8);
    overlapping.begin(QueryKind::gac,overlapping.ceiling);
    check_throws<std::runtime_error>([&]{ overlapping.prune({0,A2Domain(),A2Domain(2),PruneReason::ray_arc_support,1}); });
}
}
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==3); const std::filesystem::path dir(argv[1]); audit_faults();
    const auto boundary_count=boundaries(dir),local_count=local_factors(dir);
    Reader split_images(dir/"split_images.bin"); const Rig split(A2Palette::split_material,split_images);
    const auto split_report=camera_cases(dir,"split",split);
    Reader same_images(dir/"same_images.bin"); const Rig same(A2Palette::same_material,same_images);
    const auto same_report=camera_cases(dir,"same",same);
    CHECK(split.raw.size()==6561*512 && same.raw.size()==split.raw.size());
    for(std::size_t j=0;j<split.raw.size();++j) CHECK(same.raw[j]==(split.raw[j]==1?2U:split.raw[j]));
    for(std::size_t suite=0;suite<8;++suite) for(unsigned id=0;id<6561;++id) for(CellId v=0;v<8;++v) {
        const auto a=split_report.per_world[suite][id][v],b=same_report.per_world[suite][id][v]; CHECK((a & b)==a);
    }
    const std::filesystem::path output(argv[2]),temporary=output.string()+".tmp";
    {
        std::ofstream out(temporary); CHECK(out.good());
        out<<"{\"status\":\"PASS\",\"schema\":\"MCRA2G1\",\"worlds_per_palette\":6561,\"raw_rays\":512,\"suites\":8,"
           <<"\"render_comparisons\":"<<2*split.raw.size()<<",\"boundary_comparisons\":"<<boundary_count
           <<",\"local_support_cases\":"<<local_count<<",\"split\":";
        split_report.json(out); out<<",\"same\":"; same_report.json(out); out<<"}\n"; CHECK(out.good());
    }
    std::filesystem::rename(temporary,output);
    std::cout<<"Both palettes: exhaustive rendering/families, all queries, masks, conditioned audits and witnesses agree in all four modes\n";
}); }
