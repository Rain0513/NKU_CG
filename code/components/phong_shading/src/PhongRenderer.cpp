#include "PhongRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace PhongShading
{
    namespace
    {
        constexpr float kRayEpsilon = 0.001f;
        constexpr unsigned int kSphereStacks = 12;
        constexpr unsigned int kSphereSlices = 24;
        constexpr unsigned int kBvhLeafSize = 4;
        constexpr float kPi = 3.14159265358979323846f;

        Vec3 transformPosition(const Vec3& point, const Model& model)
        {
            return point * model.scale + model.translation;
        }

        Vec3 transformNormal(const Vec3& normal, const Vec3& scale)
        {
            Vec3 transformed{
                scale.x == 0.f ? 0.f : normal.x / scale.x,
                scale.y == 0.f ? 0.f : normal.y / scale.y,
                scale.z == 0.f ? 0.f : normal.z / scale.z
            };
            const float lengthSquared = glm::dot(transformed, transformed);
            return lengthSquared > 0.f ? transformed / std::sqrt(lengthSquared) : Vec3{0.f, 1.f, 0.f};
        }

        float component(const Vec3& value, unsigned int axis)
        {
            return axis == 0 ? value.x : axis == 1 ? value.y : value.z;
        }
    }

    struct Renderer::Ray
    {
        Vec3 origin;
        Vec3 direction;
    };

    struct Renderer::TrianglePrimitive
    {
        std::array<Vec3, 3> positions;
        std::array<Vec3, 3> normals;
        std::array<RGB, 3> vertexColors{};
        Handle material;
        Vec3 boundsMin;
        Vec3 boundsMax;
        Vec3 centroid;
    };

    struct Renderer::BvhNode
    {
        Vec3 boundsMin;
        Vec3 boundsMax;
        unsigned int begin = 0;
        unsigned int count = 0;
        int left = -1;
        int right = -1;
    };

    struct Renderer::MaterialParameters
    {
        RGB ambient{0.8f};
        RGB diffuse{0.8f};
        RGB specular{0.2f};
        float shininess = 32.f;
        Brdf brdf;
    };

    Renderer::Renderer(SharedScene scene, ShadingMode mode, const BrdfFactory& brdfFactory)
        : scene(std::move(scene))
        , mode(mode)
    {
        materials.reserve(this->scene->materials.size());
        for (auto material : this->scene->materials) {
            MaterialParameters parameters;
            using Wrapper = Property::Wrapper;
            if (auto value = material.getProperty<Wrapper::RGBType>("diffuseColor")) {
                parameters.diffuse = value->value;
                parameters.ambient = value->value;
            }
            if (auto value = material.getProperty<Wrapper::RGBType>("ambientColor")) {
                parameters.ambient = value->value;
            }
            if (brdfFactory) {
                parameters.brdf = brdfFactory(material);
            }
            else {
                if (auto value = material.getProperty<Wrapper::RGBType>("specularColor")) {
                    parameters.specular = value->value;
                }
                if (auto value = material.getProperty<Wrapper::FloatType>("specularEx")) {
                    parameters.shininess = std::max(value->value, 1.f);
                }
            }
            materials.push_back(parameters);
        }
        buildGeometry();
        buildBvh();

        if (mode == ShadingMode::GOURAUD) {
            for (auto& triangle : triangles) {
                for (unsigned int vertex = 0; vertex < 3; ++vertex) {
                    triangle.vertexColors[vertex] = shade(
                        triangle.positions[vertex], triangle.normals[vertex], triangle.material);
                }
            }
        }
    }

    Renderer::~Renderer() = default;

    void Renderer::buildGeometry()
    {
        auto addTriangle = [this](const Vec3& p0, const Vec3& p1, const Vec3& p2,
            const Vec3& n0, const Vec3& n1, const Vec3& n2, Handle material) {
            if (!material.valid() || material.index() >= materials.size()) return;

            Vec3 faceNormal = glm::cross(p1 - p0, p2 - p0);
            const float faceLengthSquared = glm::dot(faceNormal, faceNormal);
            if (faceLengthSquared <= 1e-12f) return;
            faceNormal /= std::sqrt(faceLengthSquared);

            TrianglePrimitive triangle;
            triangle.positions = {p0, p1, p2};
            triangle.normals = {
                glm::dot(n0, n0) > 1e-12f ? glm::normalize(n0) : faceNormal,
                glm::dot(n1, n1) > 1e-12f ? glm::normalize(n1) : faceNormal,
                glm::dot(n2, n2) > 1e-12f ? glm::normalize(n2) : faceNormal
            };
            triangle.material = material;
            triangle.boundsMin = glm::min(p0, glm::min(p1, p2)) - Vec3{kRayEpsilon};
            triangle.boundsMax = glm::max(p0, glm::max(p1, p2)) + Vec3{kRayEpsilon};
            triangle.centroid = (p0 + p1 + p2) / 3.f;
            triangles.push_back(std::move(triangle));
        };

        for (const auto& node : scene->nodes) {
            if (node.model >= scene->models.size()) continue;
            const auto& model = scene->models[node.model];

            if (node.type == Node::Type::TRIANGLE && node.entity < scene->triangleBuffer.size()) {
                const auto& triangle = scene->triangleBuffer[node.entity];
                const auto p0 = transformPosition(triangle.v[0], model);
                const auto p1 = transformPosition(triangle.v[1], model);
                const auto p2 = transformPosition(triangle.v[2], model);
                const auto normal = transformNormal(triangle.normal, model.scale);
                addTriangle(p0, p1, p2, normal, normal, normal, triangle.material);
            }
            else if (node.type == Node::Type::PLANE && node.entity < scene->planeBuffer.size()) {
                const auto& plane = scene->planeBuffer[node.entity];
                const auto p0 = transformPosition(plane.position, model);
                const auto p1 = transformPosition(plane.position + plane.u, model);
                const auto p2 = transformPosition(plane.position + plane.u + plane.v, model);
                const auto p3 = transformPosition(plane.position + plane.v, model);
                const auto normal = transformNormal(plane.normal, model.scale);
                addTriangle(p0, p1, p2, normal, normal, normal, plane.material);
                addTriangle(p0, p2, p3, normal, normal, normal, plane.material);
            }
            else if (node.type == Node::Type::SPHERE && node.entity < scene->sphereBuffer.size()) {
                const auto& sphere = scene->sphereBuffer[node.entity];
                auto pointAt = [&sphere, &model](unsigned int stack, unsigned int slice) {
                    const float phi = kPi * static_cast<float>(stack) / kSphereStacks;
                    const float theta = 2.f * kPi * static_cast<float>(slice) / kSphereSlices;
                    const Vec3 normal{
                        std::sin(phi) * std::cos(theta),
                        std::cos(phi),
                        std::sin(phi) * std::sin(theta)
                    };
                    return std::pair<Vec3, Vec3>{
                        transformPosition(sphere.position + sphere.radius * normal, model),
                        transformNormal(normal, model.scale)
                    };
                };

                for (unsigned int stack = 0; stack < kSphereStacks; ++stack) {
                    for (unsigned int slice = 0; slice < kSphereSlices; ++slice) {
                        const unsigned int nextSlice = (slice + 1) % kSphereSlices;
                        const auto a = pointAt(stack, slice);
                        const auto b = pointAt(stack + 1, slice);
                        const auto c = pointAt(stack + 1, nextSlice);
                        const auto d = pointAt(stack, nextSlice);
                        if (stack != 0) {
                            addTriangle(a.first, b.first, d.first, a.second, b.second, d.second, sphere.material);
                        }
                        if (stack + 1 != kSphereStacks) {
                            addTriangle(d.first, b.first, c.first, d.second, b.second, c.second, sphere.material);
                        }
                    }
                }
            }
            else if (node.type == Node::Type::MESH && node.entity < scene->meshBuffer.size()) {
                const auto& mesh = scene->meshBuffer[node.entity];
                for (std::size_t face = 0; face + 2 < mesh.positionIndices.size(); face += 3) {
                    const auto i0 = mesh.positionIndices[face];
                    const auto i1 = mesh.positionIndices[face + 1];
                    const auto i2 = mesh.positionIndices[face + 2];
                    if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() || i2 >= mesh.positions.size()) continue;

                    const auto p0 = transformPosition(mesh.positions[i0], model);
                    const auto p1 = transformPosition(mesh.positions[i1], model);
                    const auto p2 = transformPosition(mesh.positions[i2], model);
                    Vec3 n0{}, n1{}, n2{};
                    if (face + 2 < mesh.normalIndices.size()) {
                        const auto ni0 = mesh.normalIndices[face];
                        const auto ni1 = mesh.normalIndices[face + 1];
                        const auto ni2 = mesh.normalIndices[face + 2];
                        if (ni0 < mesh.normals.size() && ni1 < mesh.normals.size() && ni2 < mesh.normals.size()) {
                            n0 = transformNormal(mesh.normals[ni0], model.scale);
                            n1 = transformNormal(mesh.normals[ni1], model.scale);
                            n2 = transformNormal(mesh.normals[ni2], model.scale);
                        }
                    }
                    addTriangle(p0, p1, p2, n0, n1, n2, mesh.material);
                }
            }
        }
    }

    void Renderer::buildBvh()
    {
        triangleOrder.resize(triangles.size());
        std::iota(triangleOrder.begin(), triangleOrder.end(), 0u);
        if (!triangles.empty()) {
            buildBvhNode(0, static_cast<unsigned int>(triangles.size()));
        }
    }

    int Renderer::buildBvhNode(unsigned int begin, unsigned int end)
    {
        const int nodeIndex = static_cast<int>(bvh.size());
        bvh.emplace_back();

        Vec3 boundsMin{std::numeric_limits<float>::infinity()};
        Vec3 boundsMax{-std::numeric_limits<float>::infinity()};
        Vec3 centroidMin{std::numeric_limits<float>::infinity()};
        Vec3 centroidMax{-std::numeric_limits<float>::infinity()};
        for (unsigned int i = begin; i < end; ++i) {
            const auto& triangle = triangles[triangleOrder[i]];
            boundsMin = glm::min(boundsMin, triangle.boundsMin);
            boundsMax = glm::max(boundsMax, triangle.boundsMax);
            centroidMin = glm::min(centroidMin, triangle.centroid);
            centroidMax = glm::max(centroidMax, triangle.centroid);
        }

        bvh[nodeIndex].boundsMin = boundsMin;
        bvh[nodeIndex].boundsMax = boundsMax;
        const unsigned int count = end - begin;
        if (count <= kBvhLeafSize) {
            bvh[nodeIndex].begin = begin;
            bvh[nodeIndex].count = count;
            return nodeIndex;
        }

        const Vec3 extent = centroidMax - centroidMin;
        unsigned int axis = extent.y > extent.x ? 1u : 0u;
        if (component(extent, 2) > component(extent, axis)) axis = 2;
        const unsigned int middle = begin + count / 2;
        std::nth_element(triangleOrder.begin() + begin, triangleOrder.begin() + middle,
            triangleOrder.begin() + end, [this, axis](unsigned int left, unsigned int right) {
                return component(triangles[left].centroid, axis) < component(triangles[right].centroid, axis);
            });

        const int left = buildBvhNode(begin, middle);
        const int right = buildBvhNode(middle, end);
        bvh[nodeIndex].left = left;
        bvh[nodeIndex].right = right;
        return nodeIndex;
    }

    bool Renderer::intersectTriangle(const Ray& ray, unsigned int triangleIndex, float maxDistance,
        float& distance, float& weight1, float& weight2) const
    {
        const auto& triangle = triangles[triangleIndex];
        const Vec3 edge1 = triangle.positions[1] - triangle.positions[0];
        const Vec3 edge2 = triangle.positions[2] - triangle.positions[0];
        const Vec3 p = glm::cross(ray.direction, edge2);
        const float determinant = glm::dot(edge1, p);
        if (std::abs(determinant) < 1e-8f) return false;

        const float inverseDeterminant = 1.f / determinant;
        const Vec3 offset = ray.origin - triangle.positions[0];
        const float u = glm::dot(offset, p) * inverseDeterminant;
        if (u < 0.f || u > 1.f) return false;
        const Vec3 q = glm::cross(offset, edge1);
        const float v = glm::dot(ray.direction, q) * inverseDeterminant;
        if (v < 0.f || u + v > 1.f) return false;

        const float t = glm::dot(edge2, q) * inverseDeterminant;
        if (t <= kRayEpsilon || t >= maxDistance) return false;
        distance = t;
        weight1 = u;
        weight2 = v;
        return true;
    }

    bool Renderer::intersectBounds(const Ray& ray, const BvhNode& node, float maxDistance) const
    {
        float nearDistance = 0.f;
        float farDistance = maxDistance;
        for (unsigned int axis = 0; axis < 3; ++axis) {
            const float origin = component(ray.origin, axis);
            const float direction = component(ray.direction, axis);
            const float minimum = component(node.boundsMin, axis);
            const float maximum = component(node.boundsMax, axis);
            if (std::abs(direction) < 1e-12f) {
                if (origin < minimum || origin > maximum) return false;
                continue;
            }
            float first = (minimum - origin) / direction;
            float second = (maximum - origin) / direction;
            if (first > second) std::swap(first, second);
            nearDistance = std::max(nearDistance, first);
            farDistance = std::min(farDistance, second);
            if (farDistance < nearDistance) return false;
        }
        return true;
    }

    bool Renderer::findClosest(const Ray& ray, float maxDistance, unsigned int& triangleIndex,
        float& distance, float& weight1, float& weight2) const
    {
        if (bvh.empty()) return false;
        std::array<int, 128> stack{};
        unsigned int stackSize = 1;
        stack[0] = 0;
        bool found = false;
        distance = maxDistance;

        while (stackSize > 0) {
            const auto& node = bvh[stack[--stackSize]];
            if (!intersectBounds(ray, node, distance)) continue;
            if (node.count != 0) {
                for (unsigned int i = node.begin; i < node.begin + node.count; ++i) {
                    float t, u, v;
                    if (intersectTriangle(ray, triangleOrder[i], distance, t, u, v)) {
                        found = true;
                        triangleIndex = triangleOrder[i];
                        distance = t;
                        weight1 = u;
                        weight2 = v;
                    }
                }
            }
            else if (stackSize + 2 <= stack.size()) {
                stack[stackSize++] = node.left;
                stack[stackSize++] = node.right;
            }
        }
        return found;
    }

    bool Renderer::occluded(const Vec3& point, const Vec3& normal, const Vec3& direction, float maxDistance) const
    {
        const Vec3 origin = point + normal * kRayEpsilon;
        const float limit = std::isfinite(maxDistance) ? std::max(maxDistance - kRayEpsilon, kRayEpsilon) : maxDistance;
        unsigned int triangleIndex;
        float distance, u, v;
        return findClosest({origin, direction}, limit, triangleIndex, distance, u, v);
    }

    RGB Renderer::shade(const Vec3& point, const Vec3& normal, Handle material) const
    {
        if (!material.valid() || material.index() >= materials.size()) return RGB{0.f};
        const auto& parameters = materials[material.index()];
        const Vec3 n = glm::normalize(normal);
        const Vec3 view = glm::normalize(scene->camera.position - point);
        RGB color = parameters.ambient * scene->ambient.constant;

        auto addLight = [this, &color, &parameters, &point, &n, &view](const Vec3& lightDirection,
            const RGB& intensity, float maxDistance) {
            const float diffuseFactor = std::max(glm::dot(n, lightDirection), 0.f);
            if (diffuseFactor <= 0.f) return;
            if (occluded(point, n, lightDirection, maxDistance)) return;

            if (parameters.brdf) {
                color += intensity * parameters.brdf(n, view, lightDirection) * diffuseFactor;
                return;
            }

            const Vec3 reflected = glm::reflect(-lightDirection, n);
            const float specularFactor = std::pow(std::max(glm::dot(reflected, view), 0.f), parameters.shininess);
            color += intensity * (parameters.diffuse * diffuseFactor + parameters.specular * specularFactor);
        };

        for (const auto& light : scene->lights) {
            if (light.type == Light::Type::POINT && light.entity < scene->pointLightBuffer.size()) {
                const auto& pointLight = scene->pointLightBuffer[light.entity];
                const Vec3 offset = pointLight.position - point;
                const float distance = glm::length(offset);
                if (distance > kRayEpsilon) addLight(offset / distance, pointLight.intensity, distance);
            }
            else if (light.type == Light::Type::DIRECTIONAL && light.entity < scene->directionalLightBuffer.size()) {
                const auto& directional = scene->directionalLightBuffer[light.entity];
                const Vec3 direction = -directional.direction;
                if (glm::dot(direction, direction) > 1e-12f) {
                    addLight(glm::normalize(direction), directional.irradiance, std::numeric_limits<float>::infinity());
                }
            }
            else if (light.type == Light::Type::SPOT && light.entity < scene->spotLightBuffer.size()) {
                const auto& spot = scene->spotLightBuffer[light.entity];
                const Vec3 offset = spot.position - point;
                const float distance = glm::length(offset);
                if (distance <= kRayEpsilon) continue;
                const Vec3 direction = offset / distance;
                const Vec3 fromLight = -direction;
                const float cosine = glm::dot(glm::normalize(spot.direction), fromLight);
                const float angle = std::acos(clamp(cosine, 1.f, -1.f));
                float falloff = 1.f;
                if (angle > spot.fallout) falloff = 0.f;
                else if (angle > spot.hotSpot && spot.fallout > spot.hotSpot) {
                    falloff = (spot.fallout - angle) / (spot.fallout - spot.hotSpot);
                }
                if (falloff > 0.f) addLight(direction, spot.intensity * falloff, distance);
            }
            else if (light.type == Light::Type::AREA && light.entity < scene->areaLightBuffer.size()) {
                const auto& area = scene->areaLightBuffer[light.entity];
                const Vec3 center = area.position + 0.5f * (area.u + area.v);
                const Vec3 offset = center - point;
                const float distance = glm::length(offset);
                if (distance > kRayEpsilon) addLight(offset / distance, area.radiance, distance);
            }
        }
        return color;
    }

    std::vector<RGBA> Renderer::render()
    {
        const unsigned int width = scene->renderOption.width;
        const unsigned int height = scene->renderOption.height;
        std::vector<RGBA> pixels(static_cast<std::size_t>(width) * height, RGBA{0.f, 0.f, 0.f, 1.f});
        if (width == 0 || height == 0) return pixels;

        const auto& camera = scene->camera;
        const float fov = glm::radians(clamp(camera.fov, 160.f, 20.f));
        const float focus = std::max(camera.focusDistance, 1e-4f);
        const Vec3 w = glm::normalize(camera.position - camera.lookAt);
        const Vec3 u = glm::normalize(glm::cross(camera.up, w));
        const Vec3 v = glm::cross(w, u);
        const float halfHeight = std::tan(fov * 0.5f) * focus;
        const float aspect = camera.aspect > 0.f ? camera.aspect : static_cast<float>(width) / height;
        const float halfWidth = halfHeight * aspect;
        const Vec3 lowerLeft = camera.position - halfWidth * u - halfHeight * v - focus * w;
        const Vec3 horizontal = 2.f * halfWidth * u;
        const Vec3 vertical = 2.f * halfHeight * v;

        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                const float screenX = (static_cast<float>(x) + 0.5f) / width;
                const float screenY = (static_cast<float>(y) + 0.5f) / height;
                const Vec3 direction = glm::normalize(lowerLeft + screenX * horizontal + screenY * vertical - camera.position);
                const Ray ray{camera.position, direction};
                unsigned int triangleIndex;
                float distance, weight1, weight2;
                if (!findClosest(ray, std::numeric_limits<float>::infinity(), triangleIndex, distance, weight1, weight2)) continue;

                const auto& triangle = triangles[triangleIndex];
                const float weight0 = 1.f - weight1 - weight2;
                const Vec3 hitPoint = ray.origin + distance * ray.direction;
                RGB color;
                if (mode == ShadingMode::GOURAUD) {
                    color = weight0 * triangle.vertexColors[0]
                        + weight1 * triangle.vertexColors[1]
                        + weight2 * triangle.vertexColors[2];
                }
                else {
                    const Vec3 normal = glm::normalize(
                        weight0 * triangle.normals[0]
                        + weight1 * triangle.normals[1]
                        + weight2 * triangle.normals[2]);
                    color = shade(hitPoint, normal, triangle.material);
                }

                color = glm::sqrt(glm::max(color, RGB{0.f}));
                color = clamp(color);
                pixels[static_cast<std::size_t>(height - y - 1) * width + x] = RGBA{color, 1.f};
            }
        }
        return pixels;
    }
}
