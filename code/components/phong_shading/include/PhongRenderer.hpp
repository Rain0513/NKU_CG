#pragma once
#ifndef __PHONG_SHADING_RENDERER_HPP__
#define __PHONG_SHADING_RENDERER_HPP__

#include "scene/Scene.hpp"
#include <functional>
#include <vector>

namespace PhongShading
{
    using namespace NRenderer;

    enum class ShadingMode
    {
        GOURAUD,
        PHONG
    };

    class Renderer
    {
    public:
        // A BRDF returns linear RGB reflectance, before incident light and N dot L.
        using Brdf = std::function<RGB(const Vec3& normal, const Vec3& view, const Vec3& light)>;
        using BrdfFactory = std::function<Brdf(const Material&)>;

        Renderer(SharedScene scene, ShadingMode mode, const BrdfFactory& brdfFactory = {});
        ~Renderer();
        std::vector<RGBA> render();

    private:
        struct TrianglePrimitive;
        struct BvhNode;
        struct MaterialParameters;
        struct Ray;

        SharedScene scene;
        ShadingMode mode;
        std::vector<TrianglePrimitive> triangles;
        std::vector<BvhNode> bvh;
        std::vector<unsigned int> triangleOrder;
        std::vector<MaterialParameters> materials;

        void buildGeometry();
        void buildBvh();
        int buildBvhNode(unsigned int begin, unsigned int end);
        RGB shade(const Vec3& point, const Vec3& normal, Handle material) const;
        bool occluded(const Vec3& point, const Vec3& normal, const Vec3& direction, float maxDistance) const;
        bool intersectTriangle(const Ray& ray, unsigned int triangleIndex, float maxDistance,
            float& distance, float& weight1, float& weight2) const;
        bool intersectBounds(const Ray& ray, const BvhNode& node, float maxDistance) const;
        bool findClosest(const Ray& ray, float maxDistance, unsigned int& triangleIndex,
            float& distance, float& weight1, float& weight2) const;
    };
}

#endif
