#pragma once

#include "Camera.hpp"
#include "geometry/SceneGeometry.hpp"
#include "acceleration/KDTree.hpp"
#include "shading/MaterialParameters.hpp"
#include <memory>

namespace RayTracing {
enum class Backend { BruteForce, KDTree };

class RayTracer {
public:
    explicit RayTracer(const NRenderer::Scene& scene, Backend backend, bool collectStatistics = false);
    std::vector<RGBA> render();
    const RenderStats& statistics() const { return stats; }
private:
    struct AreaEmitter {
        struct Sample {
            Vec3 position{0.f};
            RGB intensity{0.f};
        };
        Primitive geometry;
        RGB radiance{0.f};
        std::vector<Sample> samples;
    };
    const NRenderer::Scene& scene;
    Camera camera;
    SceneGeometry geometry;
    std::vector<MaterialParameters> materials;
    std::vector<AreaEmitter> areaEmitters;
    std::unique_ptr<Intersector> intersector;
    RenderStats stats;
    double setupMs = 0.;
    RGB trace(const Ray&, unsigned int depth);
    RGB directLighting(const Ray&, const HitRecord&, const MaterialParameters&);
};
}
