#pragma once
class IRenderable;
namespace xray::render::RENDER_NAMESPACE
{
class dxRender_Visual;
namespace OlrR1Shadows
{
void BeginCollect();
void Collect(IRenderable* object, dxRender_Visual* visual, const Fmatrix& world);
void Calculate();
void Render();
void Destroy();
}
}
