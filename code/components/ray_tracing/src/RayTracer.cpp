#include "RayTracer.hpp"
#include "shading/Optics.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace RayTracing {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
float radicalInverse(unsigned int bits) {
    float result = 0.f, weight = 0.5f;
    while (bits) { result += (bits & 1u) * weight; bits >>= 1; weight *= 0.5f; }
    return result;
}
}

RayTracer::RayTracer(const NRenderer::Scene& scene, Backend backend, bool collectStatistics)
    : scene(scene), camera(scene.camera) {
    if (scene.renderOption.width == 0 || scene.renderOption.height == 0 ||
        scene.renderOption.samplesPerPixel == 0 || scene.renderOption.depth > 12)
        throw std::runtime_error("Use positive dimensions/samples and Depth between 0 and 12.");
    const auto begin = Clock::now();
    stats.enabled = collectStatistics;
    geometry = SceneGeometry(scene);
    for (auto m : scene.materials) materials.emplace_back(m);
    areaEmitters.reserve(scene.areaLightBuffer.size());
    for (const auto& light : scene.areaLightBuffer) {
        Primitive panel;
        panel.type = Primitive::Type::Plane;
        panel.p0 = light.position; panel.p1 = light.u; panel.p2 = light.v;
        const Vec3 normal = glm::cross(light.u, light.v);
        if (glm::dot(normal, normal) <= 1e-12f) continue;
        panel.normal = glm::normalize(normal);
        panel.prepare();
        if (!panel.prepared) continue;
        AreaEmitter emitter;
        emitter.geometry = panel;
        emitter.radiance = light.radiance;
        constexpr int samplesPerAxis = 2;
        constexpr float sampleCount = float(samplesPerAxis * samplesPerAxis);
        const float area = glm::length(normal);
        for (int u = 0; u < samplesPerAxis; ++u) for (int v = 0; v < samplesPerAxis; ++v) {
            const Vec3 position = light.position + ((u + 0.5f) / samplesPerAxis) * light.u +
                                  ((v + 0.5f) / samplesPerAxis) * light.v;
            // The cosine and distance terms depend on the hit point and remain
            // in directLighting; cache only the fixed area normalization.
            emitter.samples.push_back({position, light.radiance * (area / (sampleCount * 3.14159265f))});
        }
        areaEmitters.push_back(std::move(emitter));
    }
    stats.primitiveCount = geometry.primitives.size();
    stats.prepareMs = elapsed(begin);
    const auto buildBegin = Clock::now();
    if (backend == Backend::KDTree) {
        auto tree = std::make_unique<KDTree>(geometry.primitives);
        stats.nodeCount = tree->nodeCount(); stats.treeDepth = tree->depth();
        stats.leafReferences = tree->referenceCount();
        intersector = std::move(tree);
    } else intersector = std::make_unique<BruteForce>(geometry.primitives);
    stats.buildMs = elapsed(buildBegin);
    setupMs = stats.prepareMs + stats.buildMs;
}

std::vector<RGBA> RayTracer::render() {
    // A renderer may be called repeatedly; counters describe only this render.
    stats.primitiveTests = stats.boxTests = 0;
    stats.primaryRays = stats.secondaryRays = stats.shadowRays = 0;
    const auto begin = Clock::now();
    const auto width = scene.renderOption.width, height = scene.renderOption.height;
    const auto samples = scene.renderOption.samplesPerPixel;
    std::vector<RGBA> pixels(std::size_t(width) * height);
    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            RGB color(0.f);
            for (unsigned int sample = 0; sample < samples; ++sample) {
                // Deterministic Hammersley samples keep both backends identical.
                const float dx = samples == 1 ? 0.5f : (sample + 0.5f) / samples;
                const float dy = samples == 1 ? 0.5f : radicalInverse(sample);
                if (stats.enabled) ++stats.primaryRays;
                color += trace(camera.shoot((x + dx) / width, (y + dy) / height), 0);
            }
            color /= float(samples);
            // All lighting/recursion remains linear; gamma is applied once here.
            color = glm::sqrt(glm::clamp(color, RGB(0.f), RGB(1.f)));
            pixels[std::size_t(height - 1 - y) * width + x] = RGBA(color, 1.f);
        }
    }
    stats.traceMs = elapsed(begin);
    stats.totalMs = setupMs + stats.traceMs;
    return pixels;
}

RGB RayTracer::trace(const Ray& ray, unsigned int depth) {
    HitRecord hit;
    const bool found = intersector->closestHit(ray, rayEpsilon * 0.25f, infinity, hit, stats);
    // Finite area lights are also visible through mirrors and glass. They are
    // tested separately from scene geometry and do not cast an opaque shadow.
    RGB emission(0.f);
    bool hitEmitter = false;
    for (const auto& light : areaEmitters) {
        HitRecord lightHit;
        if (intersectPrimitive(light.geometry, ray, rayEpsilon * 0.25f, hit.t, lightHit)) {
            hit = lightHit; hitEmitter = true;
            emission = lightHit.frontFace ? light.radiance : RGB(0.f);
        }
    }
    if (hitEmitter) return emission;
    if (!found)
        return scene.ambient.type == NRenderer::Ambient::Type::CONSTANT ? scene.ambient.constant : RGB(0.f);
    const auto& material = materials[hit.material.index()];
    if (material.type <= 1) return directLighting(ray, hit, material);
    if (depth >= scene.renderOption.depth) return RGB(0.f);

    if (stats.enabled) ++stats.secondaryRays;
    const RGB reflected = trace(spawnRay(hit, glm::reflect(ray.direction, hit.normal)), depth + 1);
    if (material.type == 3) return material.reflectance * reflected;

    // Initial implementation models isolated dielectric objects in air.
    const float from = hit.frontFace ? 1.f : material.ior;
    const float to = hit.frontFace ? material.ior : 1.f;
    Vec3 refracted;
    if (!refractDirection(ray.direction, hit.normal, from / to, refracted)) return reflected;
    const float fresnel = fresnelSchlick(glm::dot(-ray.direction, hit.normal), from, to);
    if (stats.enabled) ++stats.secondaryRays;
    const RGB transmitted = trace(spawnRay(hit, refracted), depth + 1);
    return fresnel * reflected + (1.f - fresnel) * material.transmittance * transmitted;
}

RGB RayTracer::directLighting(const Ray& ray, const HitRecord& hit,
                              const MaterialParameters& material) {
    // Explicit ambient fill is an approximation, not simulated bounce lighting.
    // It is applied once at opaque diffuse/Phong hits, never added to ideal
    // mirror/glass directly. Scene Ambient also continues to serve as background.
    RGB ambient = material.ambientIrradiance;
    if (scene.ambient.type == NRenderer::Ambient::Type::CONSTANT)
        ambient += glm::max(scene.ambient.constant, RGB(0.f));
    RGB result = material.diffuse * ambient;
    auto sampleLight = [&](const Vec3& position, const RGB& intensity) {
        const Vec3 toLight = position - hit.point;
        if (glm::dot(toLight, toLight) <= rayEpsilon * rayEpsilon) return;
        const Vec3 direction = glm::normalize(toLight);
        const float cosine = std::max(0.f, glm::dot(hit.normal, direction));
        if (cosine <= 0.f) return;
        const Ray shadow = spawnRay(hit, direction);
        const Vec3 offsetToLight = position - shadow.origin;
        const float distance = glm::length(offsetToLight);
        const Ray boundedShadow{shadow.origin, offsetToLight / distance};
        if (stats.enabled) ++stats.shadowRays;
        // Objects beyond the light must not cast shadows on this point.
        if (intersector->occluded(boundedShadow, rayEpsilon * 0.25f, distance - rayEpsilon, stats)) return;
        RGB response = material.diffuse * cosine;
        if (material.type == 1) {
            const Vec3 reflected = glm::reflect(-direction, hit.normal);
            const float specular = std::pow(std::max(0.f, glm::dot(-ray.direction, reflected)), material.shininess);
            response += material.specular * specular;
        }
        result += response * intensity;
    };
    // Match the teaching RayCast scene's point-light intensity convention;
    // its IRV=1 is a lighting multiplier, not inverse-square physical power.
    for (const auto& light : scene.pointLightBuffer) sampleLight(light.position, light.intensity);
    // Midpoint stratification keeps area-light visibility deterministic while
    // limiting shadow-ray cost. The average estimates the same area integral.
    for (const auto& light : areaEmitters) {
        const Vec3 normal = light.geometry.normal;
        for (const auto& sample : light.samples) {
            const Vec3& position = sample.position;
            const Vec3 delta = hit.point - position;
            const float distanceSquared = glm::dot(delta, delta);
            if (distanceSquared <= rayEpsilon * rayEpsilon) continue;
            const float cosine = std::max(0.f, glm::dot(normal, delta / std::sqrt(distanceSquared)));
            sampleLight(position, sample.intensity * (cosine / distanceSquared));
        }
    }
    return result;
}
}
