#include "acceleration/AABB.hpp"
#include <algorithm>
#include <cmath>

namespace RayTracing {
void AABB::expand(const Vec3& p) {
    minimum = glm::min(minimum, p);
    maximum = glm::max(maximum, p);
}
void AABB::expand(const AABB& other) {
    expand(other.minimum);
    expand(other.maximum);
}
bool AABB::intersect(const Ray& ray, float tMin, float tMax,
                     float& enter, float& exit) const {
    double low = tMin, high = tMax;
    for (int axis = 0; axis < 3; ++axis) {
        const double direction = ray.direction[axis];
        // Avoid 0 * infinity and preserve rays exactly on a slab boundary.
        if (direction == 0.) {
            if (ray.origin[axis] < minimum[axis] || ray.origin[axis] > maximum[axis]) return false;
            continue;
        }
        double a = (double(minimum[axis]) - ray.origin[axis]) / direction;
        double b = (double(maximum[axis]) - ray.origin[axis]) / direction;
        if (a > b) std::swap(a, b);
        low = std::max(low, a);
        high = std::min(high, b);
        if (low > high) return false;
    }
    enter = static_cast<float>(low);
    exit = static_cast<float>(high);
    return true;
}
}
