#include "acceleration/Intersector.hpp"

namespace RayTracing {
bool BruteForce::closestHit(const Ray& ray, float tMin, float tMax,
                            HitRecord& hit, QueryStats& stats) const {
    hit = HitRecord{};
    bool found = false;
    for (const auto& p : primitives) {
        stats.recordPrimitive();
        HitRecord candidate;
        if (intersectPrimitive(p, ray, tMin, tMax, candidate) &&
            (!found || candidate.t < hit.t ||
             (candidate.t == hit.t && candidate.primitiveId < hit.primitiveId))) {
            found = true; hit = candidate; tMax = hit.t;
        }
    }
    return found;
}
bool BruteForce::occluded(const Ray& ray, float tMin, float tMax, QueryStats& stats) const {
    HitRecord ignored;
    for (const auto& p : primitives) {
        stats.recordPrimitive();
        if (intersectPrimitive(p, ray, tMin, tMax, ignored)) return true;
    }
    return false;
}
}
