#include "test.hpp"
#include "mcr/experiments/cameras.hpp"
#include "mcr/render/a1.hpp"
#include "mcr/render/a0.hpp"
#include <fstream>
#include <filesystem>

using namespace mcr;
int main(int argc,char** argv) { return run_tests([&] {
    CHECK(argc==1 || argc==2);
    std::ifstream image,rays;
    if(argc==2) {
        const std::filesystem::path dir(argv[1]);
        image.open(dir/"images.bin",std::ios::binary);
        rays.open(dir/"rays.txt");
        std::string magic; unsigned pixels=0,worlds=0;
        CHECK(static_cast<bool>(rays>>magic>>pixels>>worlds));
        CHECK(magic=="MCR_A1_RAYS_V1" && pixels==512 && worlds==65536 && image.good());
    }
    const Grid grid;
    std::vector<A1World> worlds;
    for(std::uint64_t id=0;id<65536;++id) {
        worlds.push_back(a1_world(8,id)); CHECK(a1_world_id(worlds.back())==id);
    }
    CHECK(a1_world_count(8)==65536);
    check_throws<std::invalid_argument>([]{ (void)A1Domain(16); });
    check_throws<std::invalid_argument>([]{ (void)maximal_geometry(A1Domain(0)); });
    CHECK(maximal_geometry(A1Domain(15))==A1State::stone);
    CHECK(maximal_geometry(A1Domain(12))==A1State::oak);
    CHECK(maximal_geometry(A1Domain(9))==A1State::oak_slab);
    for(unsigned mask=1;mask<16;++mask) for(auto s:a1_states)
        if(A1Domain(mask).contains(s)) CHECK(geometry_rank(s)<=geometry_rank(maximal_geometry(A1Domain(mask))));
    std::uint64_t comparisons=0;
    for(const auto& camera:experiments::phase_a_cameras(grid))
        for(int row=0;row<8;++row) for(int column=0;column<8;++column) {
            const auto ray=camera.pixel(column,row);
            if(argc==2) for(const auto& vec:{ray.origin,ray.direction}) for(const auto& q:vec) {
                std::int64_t n=0,d=0; CHECK(static_cast<bool>(rays>>n>>d)); CHECK(q==Rational(n,d));
            }
            const A1AabbReferenceRay reference(grid,ray);
            const A1TraversalRay traversal(grid,ray);
            for(const auto& world:worlds) {
                const auto label=reference.sample(world);
                CHECK(label==traversal.sample(world));
                if(argc==2) CHECK(image.get()==static_cast<int>(code(label)));
                ++comparisons;
            }
            // Original A0 renderer remains a separate compatibility check.
            const AabbReferenceRay a0(grid,ray);
            for(std::uint64_t id=0;id<6561;++id) {
                const auto old=a0_world(8,id);
                A1World lifted; for(auto s:old) lifted.push_back(static_cast<A1State>(s));
                CHECK(reference.sample(lifted)==a0.sample(old));
            }
        }
    if(argc==2) { CHECK(image.get()==std::char_traits<char>::eof()); rays>>std::ws; CHECK(rays.eof()); }
    // Sweep rays on cell and slab boundaries, including both sides of y=1/2.
    for(int x=-2;x<=4;++x) for(int y=-2;y<=4;++y) for(int z=-2;z<=4;++z)
        for(int dx=-1;dx<=1;++dx) for(int dy=-1;dy<=1;++dy) for(int dz=-1;dz<=1;++dz) {
            if(dx==0 && dy==0 && dz==0) continue;
            const Ray ray({Rational(x,2),Rational(y,2),Rational(z,2)},{dx,dy,dz});
            const A1AabbReferenceRay reference(grid,ray);
            const A1TraversalRay traversal(grid,ray);
            for(CellId v=0;v<8;++v) for(auto s:a1_states) {
                A1World world(8,A1State::air); world[v]=s;
                CHECK(reference.sample(world)==traversal.sample(world));
            }
        }
    A1World slab(8,A1State::air); slab[0]=A1State::oak_slab;
    CHECK(A1TraversalRay(grid,Ray({-1,Rational(1,2),Rational(1,2)},{1,0,0})).sample(slab)==PixelLabel::background);
    CHECK(A1TraversalRay(grid,Ray({-1,Rational(1,4),Rational(1,2)},{1,0,0})).sample(slab)==PixelLabel::oak);
    std::cout << "65536 A1 worlds, " << comparisons << " ray/world comparisons, 8918 boundary rays; A0 compatibility checked\n";
}); }
