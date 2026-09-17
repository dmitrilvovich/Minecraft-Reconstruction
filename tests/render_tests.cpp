#include "test.hpp"
#include "mcr/experiments/cameras.hpp"
#include "mcr/render/a0.hpp"
#include "mcr/inference/constraint.hpp"
#include <filesystem>
#include <fstream>

using namespace mcr;
Rational read_rational(std::istream& input) {
    std::int64_t n=0,d=0;
    CHECK(static_cast<bool>(input>>n>>d));
    return {n,d};
}
int main(int argc,char** argv) { return run_tests([&] {
    const Grid grid;
    std::ifstream rays,images;
    if(argc==2) {
        const std::filesystem::path dir(argv[1]);
        rays.open(dir/"rays.txt");
        images.open(dir/"images.bin",std::ios::binary);
        CHECK(rays.good() && images.good());
        std::string magic; std::size_t count=0,world_count=0;
        rays>>magic>>count>>world_count;
        CHECK(magic=="MCR_A0_RAYS_V1" && count==512 && world_count==6561);
    } else CHECK(argc==1);
    CHECK(a0_world_count(grid.size())==6561);
    for(std::uint64_t id=0;id<6561;++id) CHECK(a0_world_id(a0_world(grid.size(),id))==id);
    check_throws<std::invalid_argument>([]{ (void)Domain(8); });
    check_throws<std::out_of_range>([]{ (void)a0_world(8,6561); });
    std::size_t comparisons=0;
    for(const auto& camera:experiments::phase_a_cameras(grid))
        for(int row=0;row<8;++row) for(int col=0;col<8;++col) {
            const auto ray=camera.pixel(col,row);
            if(argc==2) {
                Vec3 c{},d{};
                for(auto& q:c) q=read_rational(rays);
                for(auto& q:d) q=read_rational(rays);
                CHECK(ray.origin==c && ray.direction==d);
            }
            const AabbReferenceRay reference(grid,ray);
            const TraversalRay traversal(grid,ray);
            std::vector<FirstHitConstraint> automata;
            for(unsigned label=0;label<3;++label)
                automata.emplace_back(std::vector<CellId>(traversal.cells().begin(),traversal.cells().end()),
                                      static_cast<PixelLabel>(label));
            for(std::uint64_t id=0;id<6561;++id) {
                const auto world=a0_world(grid.size(),id);
                const auto a=reference.sample(world),b=traversal.sample(world);
                CHECK(a==b);
                for(unsigned label=0;label<3;++label) CHECK(automata[label].accepts(world)==(code(a)==label));
                if(argc==2) {
                    const int expected=images.get();
                    CHECK(expected!=std::char_traits<char>::eof());
                    CHECK(code(a)==static_cast<unsigned>(expected));
                }
                ++comparisons;
            }
        }
    if(argc==2) {
        CHECK(images.get()==std::char_traits<char>::eof());
        rays>>std::ws; CHECK(rays.eof());
    }
    std::cout << "6561 worlds, " << comparisons << " ray/world comparisons";
    if(argc==2) std::cout << ", exact Python rays and image labels";
    std::cout << '\n';
}); }
