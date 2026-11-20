#include "geometry/SceneGeometry.hpp"
#include <cmath>
#include <stdexcept>

namespace RayTracing {
SceneGeometry::SceneGeometry(const NRenderer::Scene& scene) {
    auto append = [this, &scene](Primitive p) {
        if (!p.material.valid() || p.material.index() >= scene.materials.size())
            throw std::runtime_error("Geometry has an invalid material handle.");
        if (p.type != Primitive::Type::Sphere) {
            const Vec3 cross = glm::cross(p.type == Primitive::Type::Triangle ? p.p1 - p.p0 : p.p1,
                                         p.type == Primitive::Type::Triangle ? p.p2 - p.p0 : p.p2);
            if (glm::dot(cross, cross) < 1e-16f)
                throw std::runtime_error("Degenerate triangle or plane in scene.");
            // Use a true geometric normal, keeping the authored outward side.
            const Vec3 normal = glm::normalize(cross);
            p.normal = glm::dot(normal, p.normal) < 0.f ? -normal : normal;
        }
        if (p.type == Primitive::Type::Sphere) {
            const Vec3 extent = glm::abs(p.scale) * p.radius;
            p.bounds.expand(p.p0 - extent);
            p.bounds.expand(p.p0 + extent);
        } else if (p.type == Primitive::Type::Triangle) {
            p.bounds.expand(p.p0); p.bounds.expand(p.p1); p.bounds.expand(p.p2);
        } else {
            p.bounds.expand(p.p0); p.bounds.expand(p.p0 + p.p1);
            p.bounds.expand(p.p0 + p.p2); p.bounds.expand(p.p0 + p.p1 + p.p2);
        }
        // Flat geometry needs a conservative, nonzero thickness for slab tests.
        p.bounds.minimum -= Vec3(rayEpsilon);
        p.bounds.maximum += Vec3(rayEpsilon);
        p.id = primitives.size();
        p.prepare();
        primitives.push_back(p);
    };
    for (const auto& node : scene.nodes) {
        if (node.model >= scene.models.size()) throw std::runtime_error("Invalid model index.");
        const auto& model = scene.models[node.model];
        if (std::abs(model.scale.x) < 1e-8f || std::abs(model.scale.y) < 1e-8f ||
            std::abs(model.scale.z) < 1e-8f) throw std::runtime_error("Model scale cannot be zero.");
        const auto position = [&model](const Vec3& p) { return p * model.scale + model.translation; };
        Primitive p;
        using Type = NRenderer::Node::Type;
        if (node.type == Type::SPHERE) {
            const auto& source = scene.sphereBuffer.at(node.entity);
            if (!(source.radius > 0.f)) throw std::runtime_error("Sphere radius must be positive.");
            p.type = Primitive::Type::Sphere;
            p.p0 = position(source.position); p.scale = model.scale;
            p.radius = source.radius; p.material = source.material;
            append(p);
        } else if (node.type == Type::TRIANGLE) {
            const auto& source = scene.triangleBuffer.at(node.entity);
            p.type = Primitive::Type::Triangle;
            p.p0 = position(source.v1); p.p1 = position(source.v2); p.p2 = position(source.v3);
            p.normal = source.normal / model.scale; p.material = source.material;
            append(p);
        } else if (node.type == Type::PLANE) {
            const auto& source = scene.planeBuffer.at(node.entity);
            p.type = Primitive::Type::Plane;
            p.p0 = position(source.position);
            p.p1 = source.u * model.scale; p.p2 = source.v * model.scale;
            p.normal = source.normal / model.scale; p.material = source.material;
            append(p);
        } else if (node.type == Type::MESH) {
            // OBJ faces are already triangulated by the application's importer.
            const auto& source = scene.meshBuffer.at(node.entity);
            if (source.positionIndices.size() % 3 != 0) throw std::runtime_error("Mesh is not triangulated.");
            for (std::size_t i = 0; i < source.positionIndices.size(); i += 3) {
                p.type = Primitive::Type::Triangle;
                p.p0 = position(source.positions.at(source.positionIndices[i]));
                p.p1 = position(source.positions.at(source.positionIndices[i + 1]));
                p.p2 = position(source.positions.at(source.positionIndices[i + 2]));
                p.normal = glm::cross(p.p1 - p.p0, p.p2 - p.p0);
                p.material = source.material;
                append(p);
            }
        }
    }
}
}
