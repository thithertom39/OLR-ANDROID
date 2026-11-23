#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#define RENDER_NAMESPACE test
#include "../engine/src/Layers/xrRenderGL/glDetailShadowCull.h"
namespace D = xray::render::test::GLDetailShadowCull;
struct V { float x,y,z,w; };
struct Box { V vMin,vMax; };
struct M { float _11,_12,_13,_21,_22,_23,_31,_32,_33,_41,_42,_43; };
struct Plane { V n; float d; };
struct Instance { Box box; M matrix; float scale; };
int main()
{
    assert(D::CellCacheEnabled(nullptr));
    assert(D::CellCacheEnabled("-gles_detail_cells"));
    assert(!D::CellCacheEnabled("-gles_no_detail_cells"));
    assert(!D::CellCacheEnabled("-gles_detail_cells -gles_no_detail_cells"));
    assert(D::CellCacheEnabled("-gles_no_detail_cells_extra"));
    std::mt19937 rng(0x52424);
    std::uniform_real_distribution<float> unit(-1,1);
    unsigned rejected=0, comparisons=0;
    for (unsigned group=0;group<1000;++group)
    {
        const bool animated=group%2;
        const float amplitude=.2f+std::abs(unit(rng));
        V envelope{amplitude,0,amplitude,0}, wave{.1f,.2f,.3f,.4f};
        const V wind{amplitude*unit(rng),0,amplitude*unit(rng),0};
        assert(D::WindWithinEnvelope(amplitude,wind,wave));
        std::vector<Instance> instances;
        D::Bounds cell;
        for (unsigned i=0;i<8;++i)
        {
            const float angle=unit(rng)*3.14159265f,c=std::cos(angle),s=std::sin(angle);
            Instance in{{{-.2f,0,-.3f,0},{.3f,2,.2f,0}},
                {c,0,-s,0,1,0,s,0,c,unit(rng)*5,unit(rng),unit(rng)*5},.1f+std::abs(unit(rng))};
            instances.push_back(in);
            const auto bound=D::Build(in.box,in.matrix,in.scale,envelope,wave,animated);
            assert(bound.valid);
            cell=cell.valid?D::UnionBounds(cell,bound):bound;
        }
        cell=D::UnionBounds(cell,cell);
        for (unsigned p=0;p<50;++p)
        {
            Plane plane{{unit(rng),unit(rng),unit(rng),0},unit(rng)*15};
            D::PlaneSet frustum(&plane,1);
            if (!frustum.Outside(cell)) continue;
            ++rejected;
            for (const auto& in:instances)
            {
                const auto original=D::Build(in.box,in.matrix,in.scale,wind,wave,animated);
                assert(frustum.Outside(original));
                ++comparisons;
            }
        }
    }
    // Invalid/over-capacity data retains the baseline predicate; scratch data
    // is package-local even though its allocation can be reused.
    assert(!D::PlaneSet(static_cast<const Plane*>(nullptr),0).Valid());
    int owner;
    unsigned builds=0;
    auto build=[&](D::Bounds* cells){++builds; D::Bounds b{{1,2,3},{1,1,1},true};if(cells) cells[0]=b;return b;};
    for(unsigned pass=0;pass<2;++pass)
    {
        D::BoundsBatch batch(&owner,true);
        const auto a=D::BatchBounds(&owner,0,0,1,build);
        const auto b=D::BatchBounds(&owner,0,0,1,build);
        assert(a.cells && a.cells==b.cells && builds==pass+1);
        assert(!D::BatchBounds(&owner,3,0,1,build).envelope.valid);
        assert(!D::BatchBounds(&owner,0,64,1,build).envelope.valid);
    }
    assert(!D::activeBoundsBatch && builds==2);
    assert(rejected>1000 && comparisons>8000);
    std::cout << "{\"passed\":true,\"cell_rejections\":" << rejected
              << ",\"original_predicate_comparisons\":" << comparisons << "}\n";
}
