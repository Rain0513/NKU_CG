#pragma once

#include "Ray.hpp"
#include "scene/Camera.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace RayTracing {
class Camera {
public:
    explicit Camera(const NRenderer::Camera& camera) : origin(camera.position) {
        const Vec3 forward = camera.lookAt - camera.position;
        if (glm::dot(forward, forward) < 1e-12f || !(camera.aspect > 0.f))
            throw std::runtime_error("Camera requires a valid direction and positive aspect.");
        const Vec3 w = -glm::normalize(forward);
        const Vec3 right = glm::cross(camera.up, w);
        if (glm::dot(right, right) < 1e-12f) throw std::runtime_error("Camera Up is parallel to its view.");
        const Vec3 u = glm::normalize(right), v = glm::cross(w, u);
        const float halfHeight = std::tan(glm::radians(std::clamp(camera.fov, 1.f, 160.f)) * 0.5f);
        horizontal = 2.f * halfHeight * camera.aspect * u;
        vertical = 2.f * halfHeight * v;
        lowerLeft = origin - horizontal * 0.5f - vertical * 0.5f - w;
    }
    // Pinhole camera: focus distance cancels; aperture is deliberately unused.
    Ray shoot(float x, float y) const {
        return {origin, glm::normalize(lowerLeft + x * horizontal + y * vertical - origin)};
    }
private:
    Vec3 origin, horizontal, vertical, lowerLeft;
};
}
