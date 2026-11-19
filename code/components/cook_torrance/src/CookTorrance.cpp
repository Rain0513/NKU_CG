#include "CookTorrance.hpp"

#include <algorithm>
#include <cmath>

namespace CookTorrance
{
    using namespace NRenderer;

    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        float finiteClamp(float value, float fallback, float minimum, float maximum)
        {
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        }

        RGB finiteClamp(RGB value, RGB fallback, float minimum, float maximum)
        {
            for (int channel = 0; channel < 3; ++channel) {
                value[channel] = finiteClamp(value[channel], fallback[channel], minimum, maximum);
            }
            return value;
        }

        float floatProperty(const Material& material, const char* key, float fallback)
        {
            for (const auto& property : material.properties) {
                if (property.key == key) {
                    if (const auto* value = std::get_if<Property::Wrapper::FloatType>(&property.valueWrapper)) {
                        return value->value;
                    }
                    break;
                }
            }
            return fallback;
        }

        RGB rgbProperty(const Material& material, const char* key, RGB fallback)
        {
            for (const auto& property : material.properties) {
                if (property.key == key) {
                    if (const auto* value = std::get_if<Property::Wrapper::RGBType>(&property.valueWrapper)) {
                        return value->value;
                    }
                    break;
                }
            }
            return fallback;
        }

        Parameters sanitized(Parameters value)
        {
            value.albedo = finiteClamp(value.albedo, RGB{0.8f}, 0.f, 1.f);
            // Keep the GGX peak finite and resolvable in this single-sample renderer.
            value.roughness = finiteClamp(value.roughness, 0.35f, 0.045f, 1.f);
            value.metallic = finiteClamp(value.metallic, 0.f, 0.f, 1.f);
            value.ior = finiteClamp(value.ior, 1.5f, 0.001f, 10000.f);
            value.eta = finiteClamp(value.eta, RGB{value.ior}, 0.001f, 10000.f);
            value.extinction = finiteClamp(value.extinction, RGB{0.f}, 0.f, 10000.f);
            return value;
        }

        bool unitVector(Vec3 input, Vec3& output)
        {
            const float lengthSquared = glm::dot(input, input);
            if (!std::isfinite(lengthSquared) || lengthSquared <= 1e-12f) return false;
            output = input / std::sqrt(lengthSquared);
            return true;
        }
    }

    Parameters Parameters::fromMaterial(const Material& material)
    {
        Parameters result;
        result.albedo = rgbProperty(material, "diffuseColor", result.albedo);
        result.roughness = floatProperty(material, "roughness", result.roughness);
        result.metallic = floatProperty(material, "metallic", material.type == 3 ? 1.f : 0.f);
        result.ior = floatProperty(material, "ior", result.ior);
        result.eta = rgbProperty(material, "eta", RGB{result.ior});
        result.extinction = rgbProperty(material, "extinction", result.extinction);
        return sanitized(result);
    }

    Brdf::Brdf(const Parameters& value)
        : parameters(sanitized(value))
    {
        const RGB etaMinusOne = parameters.eta - RGB{1.f};
        const RGB etaPlusOne = parameters.eta + RGB{1.f};
        const RGB kSquared = parameters.extinction * parameters.extinction;
        const RGB conductorF0 = (etaMinusOne * etaMinusOne + kSquared)
            / (etaPlusOne * etaPlusOne + kSquared);
        const float ratio = (parameters.ior - 1.f) / (parameters.ior + 1.f);
        const RGB dielectricF0{ratio * ratio};
        f0 = glm::mix(dielectricF0, conductorF0, parameters.metallic);
    }

    RGB Brdf::normalReflectance() const
    {
        return f0;
    }

    RGB Brdf::evaluate(const Vec3& normal, const Vec3& view, const Vec3& light) const
    {
        Vec3 n, v, l, h;
        if (!unitVector(normal, n) || !unitVector(view, v) || !unitVector(light, l)) return RGB{0.f};
        const float nDotV = glm::dot(n, v);
        const float nDotL = glm::dot(n, l);
        if (nDotV <= 0.f || nDotL <= 0.f || !unitVector(v + l, h)) return RGB{0.f};

        const float nDotH = std::clamp(glm::dot(n, h), 0.f, 1.f);
        const float vDotH = std::clamp(glm::dot(v, h), 0.f, 1.f);
        const float alpha = parameters.roughness * parameters.roughness;
        const float alphaSquared = alpha * alpha;

        // GGX NDF. This form avoids cancellation at N dot H = 1.
        const float denominator = (1.f - nDotH * nDotH) + alphaSquared * nDotH * nDotH;
        const float distribution = alphaSquared / (kPi * denominator * denominator);

        // Schlick Fresnel with dielectric or RGB complex-IOR normal reflectance.
        const float oneMinusCosine = 1.f - vDotH;
        const float fresnelFactor = std::pow(oneMinusCosine, 5.f);
        const RGB fresnel = f0 + (RGB{1.f} - f0) * fresnelFactor;

        // Smith product using Schlick-GGX G1; alpha = roughness^2.
        const float k = (alpha + 1.f) * (alpha + 1.f) / 8.f;
        const float geometryV = nDotV / (nDotV * (1.f - k) + k);
        const float geometryL = nDotL / (nDotL * (1.f - k) + k);
        const RGB specular = distribution * geometryV * geometryL * fresnel
            / std::max(4.f * nDotV * nDotL, 1e-7f);

        // Apply entry and exit Fresnel transmission to the diffuse substrate.
        // The symmetric product avoids adding a full diffuse lobe at grazing views.
        const RGB diffuseViewFresnel = f0 + (RGB{1.f} - f0)
            * std::pow(1.f - std::clamp(nDotV, 0.f, 1.f), 5.f);
        const RGB diffuseLightFresnel = f0 + (RGB{1.f} - f0)
            * std::pow(1.f - std::clamp(nDotL, 0.f, 1.f), 5.f);
        const RGB diffuse = (RGB{1.f} - diffuseViewFresnel) * (RGB{1.f} - diffuseLightFresnel)
            * (1.f - parameters.metallic) * parameters.albedo / kPi;
        return diffuse + specular;
    }
}
