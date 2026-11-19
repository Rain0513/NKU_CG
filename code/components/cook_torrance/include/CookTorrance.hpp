#pragma once

#include "scene/Material.hpp"

namespace CookTorrance
{
    // RGB values are linear. Extinction is the dimensionless imaginary part
    // of eta + i*k, not an absorption coefficient expressed in inverse length.
    struct Parameters
    {
        NRenderer::RGB albedo{0.8f};
        float roughness = 0.35f;
        float metallic = 0.f;
        float ior = 1.5f;
        NRenderer::RGB eta{1.5f};
        NRenderer::RGB extinction{0.f};

        static Parameters fromMaterial(const NRenderer::Material& material);
    };

    class Brdf
    {
    public:
        explicit Brdf(const Parameters& parameters);
        NRenderer::RGB evaluate(const NRenderer::Vec3& normal,
            const NRenderer::Vec3& view, const NRenderer::Vec3& light) const;
        NRenderer::RGB normalReflectance() const;

    private:
        Parameters parameters;
        NRenderer::RGB f0;
    };
}
