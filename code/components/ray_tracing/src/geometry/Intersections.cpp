#include "geometry/Primitive.hpp"
#include <algorithm>
#include <cmath>

namespace RayTracing {
void Primitive::prepare() {
    if (type == Type::Plane) {
        const glm::dvec3 u(p1), v(p2);
        const double uu = glm::dot(u,u), uv = glm::dot(u,v), vv = glm::dot(v,v);
        const double determinant = uu * vv - uv * uv;
        if (determinant <= 1e-20) return;
        // These dual vectors turn each later (u,v) solve into two dot products.
        planeDualU = (vv * u - uv * v) / determinant;
        planeDualV = (uu * v - uv * u) / determinant;
    }
    prepared = true;
}
bool intersectPrimitive(const Primitive& p, const Ray& ray,
                        float tMin, float tMax, HitRecord& hit) {
    // HitRecord and traversal bounds both store float t. Round candidate roots
    // to that representation before the interval test, so a double root does
    // not reject an equal float hit at a shared edge depending on visit order.
    float t = infinity;
    Vec3 outward = p.normal;
    if (p.type == Primitive::Type::Sphere) {
        // Transform the ray, not the sphere. Its t still measures world distance.
        const glm::dvec3 origin = glm::dvec3((ray.origin - p.p0) / p.scale);
        const glm::dvec3 direction = glm::dvec3(ray.direction / p.scale);
        const double a = glm::dot(direction, direction);
        const double halfB = glm::dot(origin, direction);
        const double c = glm::dot(origin, origin) - double(p.radius) * p.radius;
        const double discriminant = halfB * halfB - a * c;
        if (discriminant < 0. || a == 0.) return false;
        const double root = std::sqrt(discriminant);
        double distance = (-halfB - root) / a;
        if (static_cast<float>(distance) <= tMin || static_cast<float>(distance) > tMax)
            distance = (-halfB + root) / a;
        if (static_cast<float>(distance) <= tMin || static_cast<float>(distance) > tMax) return false;
        t = static_cast<float>(distance);
        outward = glm::normalize((ray.at(t) - p.p0) / (p.scale * p.scale));
    } else if (p.type == Primitive::Type::Triangle) {
        // Double-sided Moller-Trumbore intersection, including edge hits.
        const glm::dvec3 e1 = glm::dvec3(p.p1) - glm::dvec3(p.p0);
        const glm::dvec3 e2 = glm::dvec3(p.p2) - glm::dvec3(p.p0);
        const glm::dvec3 q = glm::cross(glm::dvec3(ray.direction), e2);
        const double det = glm::dot(e1, q);
        if (std::abs(det) < 1e-12) return false;
        const glm::dvec3 offset = glm::dvec3(ray.origin) - glm::dvec3(p.p0);
        const double u = glm::dot(offset, q) / det;
        const glm::dvec3 r = glm::cross(offset, e1);
        const double v = glm::dot(glm::dvec3(ray.direction), r) / det;
        if (u < -1e-9 || v < -1e-9 || u + v > 1. + 1e-9) return false;
        const double distance = glm::dot(e2, r) / det;
        if (static_cast<float>(distance) <= tMin || static_cast<float>(distance) > tMax) return false;
        t = static_cast<float>(distance);
    } else {
        const double denominator = glm::dot(glm::dvec3(p.normal), glm::dvec3(ray.direction));
        if (std::abs(denominator) < 1e-12) return false;
        const double distance = glm::dot(glm::dvec3(p.p0) - glm::dvec3(ray.origin),
                                        glm::dvec3(p.normal)) / denominator;
        if (static_cast<float>(distance) <= tMin || static_cast<float>(distance) > tMax) return false;
        t = static_cast<float>(distance);
        const glm::dvec3 offset = glm::dvec3(ray.at(t)) - glm::dvec3(p.p0);
        // Standalone tests can use an unprepared primitive; production scene
        // geometry prepares once. Both backends take the same production path.
        Primitive fallback;
        const Primitive* coefficients = &p;
        if (!p.prepared) {
            fallback = p; fallback.prepare(); coefficients = &fallback;
            if (!fallback.prepared) return false;
        }
        const double u = glm::dot(offset, coefficients->planeDualU);
        const double v = glm::dot(offset, coefficients->planeDualV);
        if (u < -1e-7 || u > 1. + 1e-7 || v < -1e-7 || v > 1. + 1e-7) return false;
    }
    hit.t = t;
    hit.point = ray.at(t);
    hit.material = p.material;
    hit.primitiveId = p.id;
    hit.orient(ray, outward);
    return true;
}
}
