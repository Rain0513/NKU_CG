#include "RayTracer.hpp"
#include "component/RenderComponent.hpp"
#include "server/Server.hpp"
#include <exception>

namespace RayTracing {
template<Backend backend>
class Adapter : public NRenderer::RenderComponent {
    void render(NRenderer::SharedScene scene) override {
        try {
            if (!scene) throw std::runtime_error("Scene is null.");
            if (!scene->directionalLightBuffer.empty() || !scene->spotLightBuffer.empty() ||
                scene->ambient.type == NRenderer::Ambient::Type::ENVIROMENT_MAP)
                getServer().logger.warning("RayTracing: spot/directional lights and environment maps are unsupported.");
            RayTracer tracer(*scene, backend);
            auto pixels = tracer.render();
            getServer().screen.set(pixels.data(), scene->renderOption.width, scene->renderOption.height);
            // The framework reports one total duration after this function
            // returns. Detailed counters are opt-in in RayTracingCheck/CSV.
        } catch (const std::exception& error) {
            getServer().logger.error(std::string("RayTracing: ") + error.what());
        }
    }
};
}

// The legacy registration macro defines fixed local names; separate namespaces
// let one DLL safely register both selectable versions of the same renderer.
namespace RegisterBruteForce {
using Adapter = RayTracing::Adapter<RayTracing::Backend::BruteForce>;
REGISTER_RENDERER(RayTracingBruteForce,
    "Recursive ray tracing, brute-force intersections. Materials 0..3; point/area lights. "
    "Use ray_tracing_lab3.scn. Depth 6, Sample Nums 1 recommended. Total time is in Log.", Adapter);
}
namespace RegisterKDTree {
using Adapter = RayTracing::Adapter<RayTracing::Backend::KDTree>;
REGISTER_RENDERER(RayTracingKDTree,
    "Same recursive renderer with a spatial KD-Tree. Materials 0..3; point/area lights. "
    "Use ray_tracing_lab3.scn with identical settings to RayTracingBruteForce. Total time is in Log.", Adapter);
}
