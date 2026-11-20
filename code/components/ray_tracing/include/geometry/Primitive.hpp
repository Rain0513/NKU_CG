#pragma once

#include "HitRecord.hpp"
#include "acceleration/AABB.hpp"

namespace RayTracing {
// A plane is the finite patch p0 + u*p1 + v*p2, with u,v in [0,1].
// Sphere scale is retained so nonuniform model scale produces an ellipsoid.
struct Primitive {
    enum class Type { Sphere, Triangle, Plane };
    Type type = Type::Sphere;
    Vec3 p0{0.f}, p1{0.f}, p2{0.f};
    Vec3 normal{0.f, 1.f, 0.f};
    Vec3 scale{1.f};
    float radius = 1.f;
    NRenderer::Handle material;
    std::size_t id = 0;
    AABB bounds;
    // Ray-independent finite-plane coefficients, shared by both backends.
    bool prepared = false;
    glm::dvec3 planeDualU{0.}, planeDualV{0.};
    void prepare();
};

// Both acceleration backends call this exact function, including tie handling.
bool intersectPrimitive(const Primitive& primitive, const Ray& ray,
                        float tMin, float tMax, HitRecord& hit);
}
