#pragma once

#include "geometry/vec.hpp"
#include <limits>

namespace RayTracing {
using NRenderer::Vec3;
using NRenderer::RGB;
using NRenderer::RGBA;
constexpr float infinity = std::numeric_limits<float>::infinity();
constexpr float rayEpsilon = 0.001f;

// World-space rays have unit directions, so t is a world-space distance.
// Object-space sphere intersection deliberately preserves t without normalizing.
struct Ray {
    Vec3 origin;
    Vec3 direction;
    Vec3 at(float t) const { return origin + t * direction; }
};
}
