#pragma once

// The GLES backend, scene graph and game postprocess are shared. Deferred
// lighting and its targets belong exclusively to the R2 frame route.
namespace xray::render::RENDER_NAMESPACE::OlrR1Pipeline
{
bool Independent();
void BeginFrame();
void EnterDeferredStage(const char* stage);
void ReportFrame(unsigned unsupported, unsigned nativeDraws);
}
