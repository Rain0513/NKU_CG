#include "PhongRenderer.hpp"
#include "component/RenderComponent.hpp"
#include "server/Server.hpp"

namespace PhongShading
{
    using namespace NRenderer;

    class GouraudAdapter : public RenderComponent
    {
        void render(SharedScene scene) override {
            Renderer renderer{scene, ShadingMode::GOURAUD};
            auto pixels = renderer.render();
            getServer().screen.set(pixels.data(), scene->renderOption.width, scene->renderOption.height);
        }
    };

}

// Keep the macro-generated registration type local to this translation unit.
namespace
{
    REGISTER_RENDERER(PhongGouraud,
        "Phong illumination evaluated at triangle vertices and interpolated across each triangle.",
        PhongShading::GouraudAdapter);
}

