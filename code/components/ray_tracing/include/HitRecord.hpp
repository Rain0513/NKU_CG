#pragma once

#include "Ray.hpp"
#include <cstddef>

namespace RayTracing {
struct HitRecord {
    float t = infinity;
    Vec3 point{0.f};
    Vec3 normal{0.f};          // Always opposes the incoming ray.
    Vec3 outwardNormal{0.f};   // Geometric normal used for origin offsets.
    bool frontFace = true;
    NRenderer::Handle material;
    std::size_t primitiveId = std::numeric_limits<std::size_t>::max();

    void orient(const Ray& ray, const Vec3& outward) {
        outwardNormal = outward;
        frontFace = glm::dot(ray.direction, outward) < 0.f;
        normal = frontFace ? outward : -outward;
    }
};

// Offset toward the side the new ray enters, including transmitted rays.
inline Ray spawnRay(const HitRecord& hit, const Vec3& direction) {
    const Vec3 unit = glm::normalize(direction);
    const float side = glm::dot(unit, hit.outwardNormal) >= 0.f ? 1.f : -1.f;
    return {hit.point + side * rayEpsilon * hit.outwardNormal, unit};
}
}
