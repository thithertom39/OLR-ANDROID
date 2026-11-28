#pragma once

// Diagnostics are absent from the normal renderer, including per-draw hooks.
#if defined(XRAY_GPU_PROFILE_DIAGNOSTICS) && XRAY_GPU_PROFILE_DIAGNOSTICS
#include <cstdint>

namespace xray::render::RENDER_NAMESPACE::GLGpuProfiler
{
enum class Stage : unsigned
{
    RenderTotal, World, MainGeometry, Details, SunShadowAndDirect, LocalLights,
    Combine, Forward, BloomLuminance, Luminance, FinalCombineAA, ExtraPP, PresentBlit,
    ZPrefill, SyncWait, ScenePrepare, LightVisibility, Wallmarks, Rain, Emissive, LightFlush,
    Calculate, HOMWait, LocalNormal, LocalPending, LocalVisibility, LocalPack, LocalTaskWait, LocalShadowSetup, LocalShadowGeometry, LocalShadowDetails, LocalShadowSpecial, LocalSmapvisEnd, LocalPointAccum, LocalSpotAccum, LocalIndirect, LocalVolumetric, SunShadowGeometry, SunDetails, SunSpecial, SunAccum, LocalShadowAtlas,
    MainStatic, MainDynamic, MainHud, MainLods, MainDetails,
    MainIndexSetup, MainIndexDraw, ShadowIndexSetup, ShadowIndexDraw,
    MainLodBuild, MainLodUpload, MainLodSubmit, MainVisibilityWait, SharedStreamUpload,
    SharedIndexUpload, VertexRingDiscard, Count
};

void Initialize();
void Shutdown(bool contextCurrent);
void BeginFrame();
void BeginLocalLightsElapsed();
void EndLocalLightsElapsed();
void EndGpuFrame();
void FinishFrame();
void RecordDraw();
void RecordVertexBinding(bool legacy);
void RecordVisual(unsigned type);
void RecordStaticBatch(unsigned draws, unsigned bytes);
bool SamplingShadowAtlas();
bool Capturing();
bool InStage(Stage stage);
Stage SampleIndexedSubmission();
void RecordDetails(unsigned instances);
void RecordLods(unsigned impostors, unsigned draws);
void RecordStripTrees(unsigned trees, unsigned adjacentMatches);

struct LocalDetailCullStats
{
    unsigned modelTests = 0;
    unsigned modelsRejected = 0;
    unsigned modelBoundsBuilt = 0;
    unsigned cellBoundsBuilt = 0;
    unsigned cellsTested = 0;
    unsigned cellsRejected = 0;
    unsigned instanceCandidates = 0;
    unsigned instancesSkipped = 0;
    unsigned instancesSubmitted = 0;
    unsigned cellReferenceTests = 0;
    unsigned cellReferenceMismatches = 0;
};
LocalDetailCullStats* SampleLocalDetailCull();

class Scope
{
    Stage stage;
    unsigned event;
    std::uint64_t start;
    std::uint64_t threadStart;
    bool active;
public:
    explicit Scope(Stage stage, bool gpuTimer = true, bool enabled = true);
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    ~Scope();
    void Stop();
};
} // namespace xray::render::RENDER_NAMESPACE::GLGpuProfiler
#endif
