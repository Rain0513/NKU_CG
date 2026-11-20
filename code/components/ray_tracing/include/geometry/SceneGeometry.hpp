#pragma once

#include "geometry/Primitive.hpp"
#include "scene/Scene.hpp"
#include <vector>

namespace RayTracing {
// Prepared geometry owns its data. Rendering never translates the UI's Scene
// in place, so repeated renders and backend comparisons use identical inputs.
class SceneGeometry {
public:
    SceneGeometry() = default;
    explicit SceneGeometry(const NRenderer::Scene& scene);
    std::vector<Primitive> primitives;
};
}
