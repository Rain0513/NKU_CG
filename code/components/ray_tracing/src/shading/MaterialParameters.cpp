#include "shading/MaterialParameters.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace RayTracing {
MaterialParameters::MaterialParameters(NRenderer::Material material) : type(material.type) {
    using W = NRenderer::Property::Wrapper;
    auto readColor = [&material](const char* key, NRenderer::RGB fallback) {
        const auto p = material.getProperty<W::RGBType>(key);
        return p ? glm::clamp(p->value, NRenderer::RGB(0.f), NRenderer::RGB(1.f)) : fallback;
    };
    diffuse = readColor("diffuseColor", diffuse);
    ambientIrradiance = readColor("ambientColor", ambientIrradiance);
    specular = readColor("specularColor", specular);
    reflectance = readColor("reflect", reflectance);
    // Existing scene files use absorbed=(1,1,1) for clear glass. Treat it as
    // transmission tint per crossing, rather than inventing an absorption unit.
    transmittance = readColor("absorbed", transmittance);
    if (auto p = material.getProperty<W::FloatType>("specularEx")) shininess = std::max(1.f, p->value);
    if (auto p = material.getProperty<W::FloatType>("ior")) ior = p->value;
    if (!std::isfinite(ior) || ior <= 0.f) throw std::runtime_error("Material ior must be positive and finite.");
    if (type > 3) throw std::runtime_error("RayTracing supports material types 0..3 (Lambertian/Phong/Dielectric/Conductor).");
}
}
