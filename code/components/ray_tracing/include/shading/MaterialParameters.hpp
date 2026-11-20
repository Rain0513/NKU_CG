#pragma once

#include "scene/Material.hpp"

namespace RayTracing {
struct MaterialParameters {
    unsigned int type = 0;
    NRenderer::RGB diffuse{0.73f}, specular{0.2f}, reflectance{1.f}, transmittance{1.f};
    // Optional artistic ambient irradiance; zero preserves direct-light-only scenes.
    NRenderer::RGB ambientIrradiance{0.f};
    float shininess = 32.f, ior = 1.5f;
    explicit MaterialParameters(NRenderer::Material material);
};
}
