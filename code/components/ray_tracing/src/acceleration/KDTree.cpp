#include "acceleration/KDTree.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace RayTracing {
namespace {
double surfaceArea(const AABB& box) {
    const glm::dvec3 d = glm::max(glm::dvec3(box.maximum) - glm::dvec3(box.minimum), glm::dvec3(0.));
    return 2. * (d.x * d.y + d.x * d.z + d.y * d.z);
}
}
KDTree::KDTree(const std::vector<Primitive>& primitives, unsigned int leafSize, unsigned int maxDepth)
    : primitives(primitives), leafSize(std::max(leafSize, 1u)), maxDepth(std::min(maxDepth, 48u)) {
    if (primitives.empty()) return;
    for (const auto& p : primitives) rootBounds.expand(p.bounds);
    nodes.reserve(primitives.size() * 2);
    std::vector<std::size_t> ids(primitives.size());
    std::iota(ids.begin(), ids.end(), std::size_t(0));
    build(rootBounds, std::move(ids), 0);
}

int KDTree::build(const AABB& bounds, std::vector<std::size_t> ids, unsigned int level) {
    const int index = static_cast<int>(nodes.size());
    nodes.emplace_back();
    actualDepth = std::max(actualDepth, level);
    auto makeLeaf = [&]() {
        nodes[index].begin = leafIds.size(); nodes[index].count = ids.size();
        leafIds.insert(leafIds.end(), ids.begin(), ids.end());
        references += ids.size();
        return index;
    };
    if (ids.size() <= leafSize || level >= maxDepth || nodes.size() >= primitives.size() * 16 + 1)
        return makeLeaf();

    // Sampled SAH: fixed spatial candidates plus the center median on all axes.
    // Split only if expected traversal + intersection cost is below leaf cost.
    // Straddlers remain in both child cells; this is spatial subdivision.
    const double parentArea = surfaceArea(bounds);
    if (parentArea <= 0.) return makeLeaf();
    double bestCost = double(ids.size());
    int bestAxis = -1;
    float bestSplit = 0.f;
    for (int axis = 0; axis < 3; ++axis) {
        const float extent = bounds.maximum[axis] - bounds.minimum[axis];
        if (extent <= 4.f * rayEpsilon) continue;
        std::vector<float> candidates;
        for (int bin = 1; bin < 16; ++bin)
            candidates.push_back(bounds.minimum[axis] + extent * (bin / 16.f));
        std::vector<float> centers;
        centers.reserve(ids.size());
        for (auto id : ids) centers.push_back(primitives[id].bounds.center()[axis]);
        auto middle = centers.begin() + centers.size() / 2;
        std::nth_element(centers.begin(), middle, centers.end());
        candidates.push_back(*middle);
        for (float split : candidates) {
            if (split <= bounds.minimum[axis] + rayEpsilon ||
                split >= bounds.maximum[axis] - rayEpsilon) continue;
            std::size_t leftCount = 0, rightCount = 0;
            for (auto id : ids) {
                leftCount += primitives[id].bounds.minimum[axis] <= split;
                rightCount += primitives[id].bounds.maximum[axis] >= split;
            }
            if (!leftCount || !rightCount || leftCount + rightCount > ids.size() * 1.8 ||
                (leftCount == ids.size() && rightCount == ids.size())) continue;
            AABB left = bounds, right = bounds;
            left.maximum[axis] = split; right.minimum[axis] = split;
            // Same unit traversal/intersection costs for every scene.
            const double cost = 1. + (surfaceArea(left) * leftCount + surfaceArea(right) * rightCount) / parentArea;
            if (cost < bestCost) { bestCost = cost; bestAxis = axis; bestSplit = split; }
        }
    }
    if (bestAxis == -1) return makeLeaf();
    std::vector<std::size_t> left, right;
    for (auto id : ids) {
        if (primitives[id].bounds.minimum[bestAxis] <= bestSplit) left.push_back(id);
        if (primitives[id].bounds.maximum[bestAxis] >= bestSplit) right.push_back(id);
    }
    AABB leftBounds = bounds, rightBounds = bounds;
    leftBounds.maximum[bestAxis] = bestSplit; rightBounds.minimum[bestAxis] = bestSplit;
    nodes[index].axis = bestAxis; nodes[index].split = bestSplit;
    const int l = build(leftBounds, std::move(left), level + 1);
    const int r = build(rightBounds, std::move(right), level + 1);
    // Never retain Node& across recursive calls, which can reallocate nodes.
    nodes[index].left = l; nodes[index].right = r;
    return index;
}

bool KDTree::query(const Ray& ray, float tMin, float tMax, HitRecord& hit,
                   QueryStats& stats, bool anyHit) const {
    hit = HitRecord{};
    if (nodes.empty()) return false;
    float enter, exit;
    stats.recordBox();
    if (!rootBounds.intersect(ray, tMin, tMax, enter, exit)) return false;
    std::array<double, 3> inverse{};
    for (int axis = 0; axis < 3; ++axis)
        if (ray.direction[axis] != 0.f) inverse[axis] = 1. / double(ray.direction[axis]);
    struct Pending { int node; double enter, exit; };
    // At most one deferred sibling per level; maxDepth is clamped to 48.
    std::array<Pending, 50> stack;
    unsigned int count = 0;
    stack[count++] = {0, std::nextafter(enter, -infinity), std::nextafter(exit, infinity)};
    bool found = false;
    while (count) {
        const auto current = stack[--count];
        // Equal distances must remain eligible for stable primitive-ID ties.
        if (current.enter > double(std::nextafter(tMax, infinity))) continue;
        const auto& node = nodes[current.node];
        if (node.axis == -1) {
            for (std::size_t i = node.begin; i < node.begin + node.count; ++i) {
                stats.recordPrimitive();
                HitRecord candidate;
                // A straddler's full-query hit may lie outside this leaf, but
                // is still a valid global candidate. No unsafe mailbox is used.
                if (!intersectPrimitive(primitives[leafIds[i]], ray, tMin, tMax, candidate)) continue;
                if (anyHit) return true;
                if (!found || candidate.t < hit.t ||
                    (candidate.t == hit.t && candidate.primitiveId < hit.primitiveId)) {
                    found = true; hit = candidate; tMax = hit.t;
                }
            }
            continue;
        }
        const int axis = node.axis;
        if (ray.direction[axis] == 0.f) {
            if (ray.origin[axis] <= node.split) stack[count++] = {node.left, current.enter, current.exit};
            if (ray.origin[axis] >= node.split) stack[count++] = {node.right, current.enter, current.exit};
            continue; // A parallel ray on the split must visit both closed cells.
        }
        const double crossing = (double(node.split) - ray.origin[axis]) * inverse[axis];
        const int nearNode = ray.direction[axis] > 0.f ? node.left : node.right;
        const int farNode = ray.direction[axis] > 0.f ? node.right : node.left;
        if (crossing < current.enter) stack[count++] = {farNode, current.enter, current.exit};
        else if (crossing > current.exit) stack[count++] = {nearNode, current.enter, current.exit};
        else {
            // Conservative endpoints retain shared-edge hits despite float hit
            // distances. LIFO ordering visits the near cell before the far one.
            const double lower = std::nextafter(float(crossing), -infinity);
            const double upper = std::nextafter(float(crossing), infinity);
            stack[count++] = {farNode, std::max(current.enter, lower), current.exit};
            stack[count++] = {nearNode, current.enter, std::min(current.exit, upper)};
        }
    }
    return found;
}
bool KDTree::closestHit(const Ray& ray, float tMin, float tMax, HitRecord& hit, QueryStats& stats) const {
    return query(ray, tMin, tMax, hit, stats, false);
}
bool KDTree::occluded(const Ray& ray, float tMin, float tMax, QueryStats& stats) const {
    HitRecord ignored;
    return query(ray, tMin, tMax, ignored, stats, true);
}
}
