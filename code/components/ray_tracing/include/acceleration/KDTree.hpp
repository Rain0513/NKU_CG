#pragma once

#include "acceleration/Intersector.hpp"

namespace RayTracing {
class KDTree final : public Intersector {
public:
    explicit KDTree(const std::vector<Primitive>& primitives,
                    unsigned int leafSize = 4, unsigned int maxDepth = 24);
    bool closestHit(const Ray&, float, float, HitRecord&, QueryStats&) const override;
    bool occluded(const Ray&, float, float, QueryStats&) const override;
    std::size_t nodeCount() const { return nodes.size(); }
    std::size_t referenceCount() const { return references; }
    unsigned int depth() const { return actualDepth; }
private:
    struct Node {
        int axis = -1;
        float split = 0.f;
        int left = -1, right = -1;
        std::size_t begin = 0, count = 0;
    };
    const std::vector<Primitive>& primitives;
    std::vector<Node> nodes;
    AABB rootBounds;
    std::vector<std::size_t> leafIds; // Contiguous references avoid per-leaf allocation.
    unsigned int leafSize, maxDepth, actualDepth = 0;
    std::size_t references = 0;
    int build(const AABB&, std::vector<std::size_t>, unsigned int);
    bool query(const Ray&, float, float, HitRecord&, QueryStats&, bool) const;
};
}
