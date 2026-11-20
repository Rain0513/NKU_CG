#pragma once

#include "geometry/Primitive.hpp"
#include "RenderStats.hpp"
#include <vector>

namespace RayTracing {
// closestHit is for camera/reflection/refraction rays. occluded is a bounded
// any-hit query: shadows need no nearest hit and stop at the first blocker.
class Intersector {
public:
    virtual ~Intersector() = default;
    virtual bool closestHit(const Ray&, float, float, HitRecord&, QueryStats&) const = 0;
    virtual bool occluded(const Ray&, float, float, QueryStats&) const = 0;
};

class BruteForce final : public Intersector {
public:
    explicit BruteForce(const std::vector<Primitive>& primitives) : primitives(primitives) {}
    bool closestHit(const Ray&, float, float, HitRecord&, QueryStats&) const override;
    bool occluded(const Ray&, float, float, QueryStats&) const override;
private:
    const std::vector<Primitive>& primitives;
};
}
