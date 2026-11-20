#pragma once

#include "Ray.hpp"

namespace RayTracing {
struct AABB {
    Vec3 minimum{infinity};
    Vec3 maximum{-infinity};
    void expand(const Vec3& p);
    void expand(const AABB& other);
    Vec3 center() const { return (minimum + maximum) * 0.5f; }

    // Inclusive slab intervals retain rays on split planes and box edges.
    bool intersect(const Ray& ray, float tMin, float tMax,
                   float& enter, float& exit) const;
};
}
