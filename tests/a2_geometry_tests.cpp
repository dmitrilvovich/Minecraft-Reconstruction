#include "test.hpp"
#include "fixtures/a2_scenes.hpp"
#include "mcr/experiments/cameras.hpp"
#include "mcr/render/a1.hpp"
#include <array>

using namespace mcr;
namespace {
std::uint64_t samples=0,camera_comparisons=0;
void sample(const Grid& grid,const Ray& ray,const A2World& world,A2Palette palette,PixelLabel expected) {
    CHECK(A2AabbReferenceRay(grid,ray,palette).sample(world)==expected);
    CHECK(A2TraversalRay(grid,ray,palette).sample(world)==expected);
    ++samples;
}
}
int main() { return run_tests([] {
    const Grid grid;
    const Rational half(1,2),quarter(1,4);
    CHECK(a2_world_count(8)==6561 && a2_world_count(0)==1);
    for(unsigned id=0;id<27;++id) CHECK(a2_world_id(a2_world(3,id))==id);
    check_throws<std::overflow_error>([]{ (void)a2_world_count(41); });
    check_throws<std::out_of_range>([]{ (void)a2_world(3,27); });
    check_throws<std::invalid_argument>([]{ (void)A2Domain(8); });
    check_throws<std::invalid_argument>([]{ (void)material(static_cast<A2State>(3),A2Palette::same_material); });
    check_throws<std::invalid_argument>([]{ (void)material(A2State::air,static_cast<A2Palette>(2)); });
    for(auto palette:{A2Palette::split_material,A2Palette::same_material}) {
        for(CellId v=0;v<grid.size();++v) {
            const auto p=grid.position(v);
            const Rational x(p[0]),y(p[1]),z(p[2]);
            for(auto s:{A2State::bottom_slab,A2State::top_slab}) {
                A2World world(grid.size(),A2State::air); world[v]=s;
                const auto color=material(s,palette);
                for(auto h:{Rational(0),quarter,half,3*quarter,Rational(1)}) {
                    const bool hit=s==A2State::bottom_slab ? h<half : half<=h && h<1;
                    for(const auto& ray:std::array{
                        Ray({x-1,y+h,z+half},{1,0,0}),Ray({x+2,y+h,z+half},{-1,0,0}),
                        Ray({x+half,y+h,z-1},{0,0,1}),Ray({x+half,y+h,z+2},{0,0,-1})})
                        sample(grid,ray,world,palette,hit ? color : PixelLabel::background);
                }
                sample(grid,Ray({x+half,y-1,z+half},{0,1,0}),world,palette,color);
                sample(grid,Ray({x+half,y+2,z+half},{0,-1,0}),world,palette,color);
                sample(grid,Ray({x+half,y+half,z+half},{0,1,0}),world,palette,
                       s==A2State::top_slab ? color : PixelLabel::background);
                sample(grid,Ray({x+half,y+half,z+half},{0,-1,0}),world,palette,
                       s==A2State::bottom_slab ? color : PixelLabel::background);
                sample(grid,Ray({x+half,y+1,z+half},{0,-1,0}),world,palette,color);
                sample(grid,Ray({x-1,y+3*half,z+half},{1,-1,0}),world,palette,
                       s==A2State::bottom_slab ? color : PixelLabel::background);
                sample(grid,Ray({x-1,y-half,z+half},{1,1,0}),world,palette,
                       s==A2State::top_slab ? color : PixelLabel::background);
            }
        }
        // First-hit order still follows cells even when entry into a slab is delayed.
        A2World world(grid.size(),A2State::air);
        world[grid.id({0,0,0})]=A2State::top_slab;
        world[grid.id({1,0,0})]=A2State::bottom_slab;
        sample(grid,Ray({-1,quarter,half},{1,0,0}),world,palette,PixelLabel::oak);
        sample(grid,Ray({-1,3*quarter,half},{1,0,0}),world,palette,material(A2State::top_slab,palette));
        sample(grid,Ray({-1,-quarter,half},{1,half,0}),world,palette,material(A2State::top_slab,palette));
        const A2TraversalRay vertical(grid,Ray({half,-1,half},{0,1,0}),palette);
        for(const auto& step:vertical.steps()) CHECK(step.bottom_hit && step.top_hit);
        const auto triangle=test::a2_fixture(true,palette);
        const A2TraversalRay diagonal(grid,triangle.rays[4],palette);
        CHECK(diagonal.steps().size()==2);
        CHECK(diagonal.steps()[0].cell==triangle.active[1]);
        CHECK(diagonal.steps()[1].cell==triangle.active[2]);

        // A bounded sample through every established camera ray, not an A2 acceptance corpus.
        for(const auto& camera:experiments::phase_a_cameras(grid))
            for(int row=0;row<8;++row) for(int col=0;col<8;++col) {
                const auto ray=camera.pixel(col,row);
                const A2AabbReferenceRay reference(grid,ray,palette);
                const A2TraversalRay traversal(grid,ray,palette);
                for(unsigned k=0;k<32;++k) {
                    const auto w=a2_world(grid.size(),k==31 ? 6560 : k*211);
                    CHECK(reference.sample(w)==traversal.sample(w));
                    ++camera_comparisons;
                }
                // Identical bottom-only worlds must retain A1 rendering semantics.
                const A1AabbReferenceRay a1(grid,ray);
                for(unsigned id:{0U,1U,85U,170U,255U}) {
                    A1World old(grid.size(),A1State::air); A2World now(grid.size(),A2State::air);
                    for(CellId v=0;v<grid.size();++v) if(id & (1U<<v)) {
                        old[v]=A1State::oak_slab; now[v]=A2State::bottom_slab;
                    }
                    CHECK(reference.sample(now)==a1.sample(old));
                }
            }
    }
    check_throws<std::invalid_argument>([&]{ (void)A2TraversalRay(grid,Ray({-1,0,0},{1,0,0})).sample({}); });
    std::cout << samples << " named half-slab samples; " << camera_comparisons
              << " sampled camera ray/world comparisons; both palettes and A1 bottom-slab compatibility\n";
}); }
