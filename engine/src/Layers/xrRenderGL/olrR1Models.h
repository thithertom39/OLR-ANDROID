#pragma once
class IRenderable;
class ISpatial;

namespace xray::render::RENDER_NAMESPACE
{
class CBackend;
class CBlender_Compile;
class dxRender_Visual;
struct R_dsgraph_structure;
namespace OlrR1Models
{
void BeginCollect();
void Observe(IRenderable* object, ISpatial* spatial);
ISpatial* SpatialOf(IRenderable* object);
bool Collect(IRenderable* object);
void CalculateProjector();
void CaptureModels(R_dsgraph_structure& graph);
void Bind(CBackend& commands, IRenderable* object, dxRender_Visual* visual);
void Compile(CBlender_Compile& compiler, bool blend, unsigned alphaRef);
void Destroy();
unsigned Receivers();
}
}
