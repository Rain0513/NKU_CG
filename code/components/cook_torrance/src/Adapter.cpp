#include "CookTorrance.hpp"
#include "PhongRenderer.hpp"
#include "component/RenderComponent.hpp"
#include "server/Server.hpp"

#include <exception>

namespace CookTorrance
{
    class Adapter : public NRenderer::RenderComponent
    {
        void render(NRenderer::SharedScene scene) override
        {
            if (!scene || scene->renderOption.width == 0 || scene->renderOption.height == 0) {
                getServer().logger.error("CookTorrance: a valid scene and positive image size are required.");
                return;
            }
            try {
                PhongShading::Renderer renderer{scene, PhongShading::ShadingMode::PHONG,
                    [](const NRenderer::Material& material) -> PhongShading::Renderer::Brdf {
                        const Brdf brdf{Parameters::fromMaterial(material)};
                        return [brdf](const NRenderer::Vec3& normal, const NRenderer::Vec3& view,
                            const NRenderer::Vec3& light) {
                            return brdf.evaluate(normal, view, light);
                        };
                    }};
                auto pixels = renderer.render();
                getServer().screen.set(pixels.data(), scene->renderOption.width, scene->renderOption.height);
                getServer().logger.success("CookTorrance rendering completed.");
            }
            catch (const std::exception& error) {
                getServer().logger.error(std::string{"CookTorrance: "} + error.what());
            }
        }
    };
}

namespace
{
    REGISTER_RENDERER(CookTorrance,
        "Cook-Torrance: GGX + Schlick Fresnel + Smith/Schlick-GGX. "
        "Properties: roughness, metallic, ior (Float), eta and extinction (RGB). "
        "Use ray_cast_cornel.scn and add these properties to its Phong material; "
        "direct lighting, area lights approximated by their centers.",
        CookTorrance::Adapter);
}
