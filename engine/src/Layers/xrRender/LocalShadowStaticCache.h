#pragma once

namespace xray::render::RENDER_NAMESPACE
{
class light;
class dxRender_Visual;
struct R_dsgraph_structure;

// Caches only the ordered traversal of immutable level geometry. Replaying
// the traversal still runs per-frame SSA, shader selection and draw insertion.
class LocalShadowStaticCache
{
public:
    enum class Result { Bypass, Built, Hit, Verified, Mismatch };

    static LocalShadowStaticCache* Acquire(const light* source, u32 frame);
    static void Clear();
    bool Replay(R_dsgraph_structure& graph, bool verify);

    Result result = Result::Bypass;
    u32 skippedTests = 0;

private:
    struct View
    {
        dxRender_Visual* root;
        CFrustum frustum;
    };
    enum class Action { Insert, Visit, VisitLeaves };
    struct Step
    {
        dxRender_Visual* visual;
        Action action;
        u32 planes;
        u16 view;

        bool operator==(const Step& other) const
        {
            return visual == other.visual && action == other.action &&
                planes == other.planes && view == other.view;
        }
    };
    static bool Capture(xr_vector<Step>& steps, dxRender_Visual* visual, const CFrustum* frustum,
        u32 planes, u16 view, u32& tests);
    bool CaptureViews(xr_vector<Step>& steps, u32& tests) const;
    bool Matches(const R_dsgraph_structure& graph) const;

    xr_vector<View> views;
    xr_vector<Step> steps;
    u32 tests = 0;
    bool usable = false;
};
} // namespace xray::render::RENDER_NAMESPACE
