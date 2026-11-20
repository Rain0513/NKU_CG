#pragma once

#include "Ray.hpp"
#include <algorithm>
#include <cmath>

namespace RayTracing {
inline float fresnelSchlick(float cosine, float iorFrom, float iorTo) {
    const float ratio = (iorFrom - iorTo) / (iorFrom + iorTo);
    const float r0 = ratio * ratio;
    return r0 + (1.f - r0) * std::pow(1.f - std::clamp(cosine, 0.f, 1.f), 5.f);
}

// normal points into the incident medium. False means total internal reflection.
inline bool refractDirection(const Vec3& incoming, const Vec3& normal,
                             float eta, Vec3& transmitted) {
    const float cosine = std::clamp(glm::dot(-incoming, normal), 0.f, 1.f);
    const float sinSquared = eta * eta * std::max(0.f, 1.f - cosine * cosine);
    if (sinSquared > 1.f) return false;
    transmitted = glm::normalize(eta * (incoming + cosine * normal) -
                                  std::sqrt(std::max(0.f, 1.f - sinSquared)) * normal);
    return true;
}
}
