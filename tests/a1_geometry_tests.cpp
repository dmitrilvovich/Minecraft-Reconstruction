#include "test.hpp"
#include "mcr/render/a1.hpp"
#include <array>
#include <string_view>

using namespace mcr;
namespace {
std::uint64_t samples=0;
void sample(std::string_view name,const Grid& grid,const Ray& ray,const A1World& world,PixelLabel expected) {
    try {
        CHECK(A1AabbReferenceRay(grid,ray).sample(world)==expected);
        CHECK(A1TraversalRay(grid,ray).sample(world)==expected);
        ++samples;
    } catch(const std::exception& e) { throw std::runtime_error(std::string(name)+": "+e.what()); }
}
void interval(const Ray& ray,const Aabb& slab,Rational enter,Rational leave) {
    const auto hit=intersect(ray,slab);
    CHECK(hit && hit->enter==enter && hit->leave==leave);
}
}
int main() { return run_tests([] {
    const Grid grid;
    const Rational half(1,2),quarter(1,4);
    CHECK(A1Domain().bits()==15 && a1_states.size()==4);
    CHECK(material(A1State::oak_slab)==material(A1State::oak));
    // Exact interval expectations from all six directions, translated to every cell.
    for(CellId cell=0;cell<grid.size();++cell) {
        const auto p=grid.position(cell);
        const Rational x(p[0]),y(p[1]),z(p[2]);
        const Aabb slab({x,y,z},{x+1,y+half,z+1});
        A1World world(8,A1State::air); world[cell]=A1State::oak_slab;
        const std::array side_rays{
            Ray({x-1,y+quarter,z+half},{1,0,0}), Ray({x+2,y+quarter,z+half},{-1,0,0}),
            Ray({x+half,y+quarter,z-1},{0,0,1}), Ray({x+half,y+quarter,z+2},{0,0,-1})};
        for(const auto& ray:side_rays) {
            interval(ray,slab,1,2); sample("slab side entry",grid,ray,world,PixelLabel::oak);
        }
        const Ray bottom({x+half,y-1,z+half},{0,1,0});
        const Ray top({x+half,y+2,z+half},{0,-1,0});
        interval(bottom,slab,1,1+half); interval(top,slab,1+half,2);
        sample("slab bottom entry",grid,bottom,world,PixelLabel::oak);
        sample("slab top entry",grid,top,world,PixelLabel::oak);
        const Ray top_parallel({x-1,y+half,z+half},{1,0,0});
        CHECK(!intersect(top_parallel,slab));
        sample("excluded top face parallel",grid,top_parallel,world,PixelLabel::background);
        const Ray bottom_parallel({x-1,y,z+half},{1,0,0});
        interval(bottom_parallel,slab,1,2);
        sample("included bottom face parallel",grid,bottom_parallel,world,PixelLabel::oak);
        const Ray tangent({x-1,y-half,z+half},{1,1,0});
        CHECK(!intersect(tangent,slab));
        sample("zero-length top corner contact",grid,tangent,world,PixelLabel::background);
        const Ray crossing_entry({x-1,y+1+half,z+half},{1,-1,0});
        interval(crossing_entry,slab,1,1+half);
        sample("enter through excluded top then interior",grid,crossing_entry,world,PixelLabel::oak);
        const Ray crossing_exit({x-1,y+1+half,z+half},{1,-half,0});
        CHECK(!intersect(crossing_exit,slab));
        sample("top crossing only at cell exit",grid,crossing_exit,world,PixelLabel::background);
        const Ray inside({x+half,y+quarter,z+half},{1,0,0});
        interval(inside,slab,0,half);
        sample("origin inside slab",grid,inside,world,PixelLabel::oak);
        sample("origin above slab horizontal",grid,Ray({x+half,y+3*quarter,z+half},{1,0,0}),world,PixelLabel::background);
        sample("origin on top pointing inward",grid,Ray({x+half,y+half,z+half},{0,-1,0}),world,PixelLabel::oak);
        sample("origin on top pointing outward",grid,Ray({x+half,y+half,z+half},{0,1,0}),world,PixelLabel::background);
        sample("origin on bottom pointing outward",grid,Ray({x+half,y,z+half},{0,-1,0}),world,PixelLabel::background);
    }
    const auto front=grid.id({0,0,0}),back=grid.id({1,0,0});
    const Ray lower({-1,quarter,half},{1,0,0}),upper({-1,3*quarter,half},{1,0,0});
    const Ray reverse({3,quarter,half},{-1,0,0});
    const auto steps=[](const A1TraversalRay& ray) { return std::vector<A1RayCell>(ray.steps().begin(),ray.steps().end()); };
    CHECK(steps(A1TraversalRay(grid,lower))==std::vector<A1RayCell>({{front,true},{back,true}}));
    CHECK(steps(A1TraversalRay(grid,upper))==std::vector<A1RayCell>({{front,false},{back,false}}));
    CHECK(steps(A1TraversalRay(grid,reverse))==std::vector<A1RayCell>({{back,true},{front,true}}));
    A1World world(8,A1State::air);
    world[front]=A1State::oak_slab; world[back]=A1State::stone;
    sample("front slab hides rear cube",grid,lower,world,PixelLabel::oak);
    sample("slab gap reveals rear cube",grid,upper,world,PixelLabel::stone);
    sample("reversed order exposes cube first",grid,reverse,world,PixelLabel::stone);
    sample("slab hit delayed within front cell",grid,Ray({-1,5*quarter,half},{1,-half,0}),world,PixelLabel::oak);
    sample("origin in gap reveals rear cube",grid,Ray({quarter,3*quarter,half},{1,0,0}),world,PixelLabel::stone);
    world[front]=A1State::stone; world[back]=A1State::oak_slab;
    sample("front full cube hides rear slab",grid,lower,world,PixelLabel::stone);
    sample("reverse slab hides full cube",grid,reverse,world,PixelLabel::oak);
    for(auto hidden:a1_states) {
        world[front]=A1State::oak_slab; world[back]=hidden;
        sample("changing occluded state preserves lower pixel",grid,lower,world,PixelLabel::oak);
        const auto exposed=hidden==A1State::oak_slab ? PixelLabel::background : material(hidden);
        sample("upper pixel observes through slab gap",grid,upper,world,exposed);
        world[front]=A1State::oak;
        sample("full cube occludes above half height",grid,upper,world,PixelLabel::oak);
        world[front]=A1State::air;
        sample("air does not occlude lower pixel",grid,lower,world,material(hidden));
    }
    // Adjacent vertical cells must be visited by distance, not state/rank order.
    world.assign(8,A1State::air);
    world[front]=A1State::stone; world[grid.id({0,1,0})]=A1State::oak_slab;
    sample("upper slab occludes lower full cube",grid,Ray({half,3,half},{0,-1,0}),world,PixelLabel::oak);
    sample("lower full cube occludes upper slab",grid,Ray({half,-1,half},{0,1,0}),world,PixelLabel::stone);
    std::cout << samples << " named A1 geometry samples checked in both renderers; exact intervals and traversal order checked\n";
}); }
