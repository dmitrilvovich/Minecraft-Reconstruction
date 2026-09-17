#include "test.hpp"
#include "mcr/experiments/cameras.hpp"
#include "mcr/grid/traversal.hpp"
#include <algorithm>
#include <limits>
#include <set>

using namespace mcr;
void check_ray(const Grid& grid, const Ray& ray) {
    const auto visits = traverse(grid,ray);
    std::vector<CellVisit> reference;
    for (CellId v=0; v<grid.size(); ++v)
        if (auto hit=intersect(ray,grid.bounds(v))) reference.push_back({v,hit->enter,hit->leave});
    std::sort(reference.begin(),reference.end(),[](const auto& a,const auto& b){return a.enter<b.enter;});
    CHECK(reference.size()==visits.size());
    std::set<CellId> seen;
    std::int64_t previous=-1;
    for (std::size_t k=0; k<visits.size(); ++k) {
        CHECK(visits[k].cell==reference[k].cell);
        CHECK(visits[k].enter==reference[k].enter);
        CHECK(visits[k].leave==reference[k].leave);
        CHECK(visits[k].enter<visits[k].leave);
        CHECK(seen.insert(visits[k].cell).second);
        const auto p=grid.position(visits[k].cell);
        std::int64_t rank=0;
        for (std::size_t j=0;j<3;++j) rank+=std::abs(p[j]-ray.origin[j].floor());
        CHECK(rank>previous); previous=rank;
    }
}
int main() { return run_tests([] {
    CHECK(Rational(2,-4)==Rational(-1,2));
    CHECK(Rational(-1,3).floor()==-1);
    CHECK(Rational(-6,3).floor()==-2);
    CHECK(Rational(1,3)+Rational(1,6)==Rational(1,2));
    CHECK(Rational(-2,3)*Rational(9,4)==Rational(-3,2));
    const auto max=std::numeric_limits<std::int64_t>::max();
    CHECK(Rational(max-1,max)<Rational(max,max-1));
    CHECK(Rational(-max,max-1)<Rational(-(max-1),max));
    CHECK(Rational(max,3)*Rational(3,max)==1);
    check_throws<std::overflow_error>([&]{ (void)(Rational(max)+1); });
    check_throws<std::overflow_error>([&]{ (void)(Rational(-max)-1); });
    check_throws<std::overflow_error>([&]{ (void)(Rational(max)*2); });
    check_throws<std::overflow_error>([]{ (void)Rational(std::numeric_limits<std::int64_t>::min()); });
    check_throws<std::invalid_argument>([]{ (void)Rational(1,0); });
    check_throws<std::invalid_argument>([]{ (void)(Rational(1)/0); });
    for (int a=-8;a<=8;++a) for(int b=1;b<=8;++b)
        for(int c=-8;c<=8;++c) for(int d=1;d<=8;++d) {
            CHECK((Rational(a,b)<Rational(c,d))==(a*d<c*b));
            CHECK(Rational(a,b)+Rational(c,d)==Rational(a*d+c*b,b*d));
        }
    const Grid grid;
    CHECK(grid.id({1,1,1})==7);
    CHECK(grid.position(4)==CellPosition({1,0,0}));
    for(CellId v=0;v<grid.size();++v) CHECK(grid.id(grid.position(v))==v);
    CHECK(intersect(Ray({-1,0,0},{1,0,0}),grid.bounds(0)).has_value());
    CHECK(!intersect(Ray({-1,1,0},{1,0,0}),grid.bounds(0)));
    CHECK(!intersect(Ray({-1,0,0},{1,1,0}),grid.bounds(0))); // corner tangent
    CHECK(traverse(grid,Ray({2,0,0},{1,0,0})).empty());
    CHECK(traverse(grid,Ray({2,0,0},{-1,0,0})).size()==2);
    CHECK(traverse(grid,Ray({-1,-1,-1},{1,1,1})).size()==2);
    check_throws<std::invalid_argument>([]{ (void)Ray({0,0,0},{0,0,0}); });
    for (int x=-2;x<=4;++x) for (int y=-2;y<=4;++y) for (int z=-2;z<=4;++z)
        for(int dx=-1;dx<=1;++dx) for(int dy=-1;dy<=1;++dy) for(int dz=-1;dz<=1;++dz) {
            if(dx==0 && dy==0 && dz==0) continue;
            check_ray(grid,Ray({Rational(x,2),Rational(y,2),Rational(z,2)},{dx,dy,dz}));
        }
    const auto cameras=experiments::phase_a_cameras(grid);
    CHECK(cameras.size()==8);
    CHECK(cameras[0].center==Vec3({-5,1,1}));
    CHECK(cameras[6].center==Vec3({-3,5,3}));
    CHECK(cameras[0].pixel(0,0).direction==Vec3({1,Rational(-7,24),Rational(7,24)}));
    for (const auto& c:cameras) for(int row=0;row<c.height;++row) for(int col=0;col<c.width;++col)
        check_ray(grid,c.pixel(col,row));
    std::cout << "8,918 boundary rays and 512 camera rays checked\n";
}); }
