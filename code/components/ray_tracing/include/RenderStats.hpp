#pragma once

#include <cstdint>
#include <string>

namespace RayTracing {
struct QueryStats {
    // Timing and diagnostic runs apply the same switch to both backends.
    bool enabled = true;
    std::uint64_t primitiveTests = 0;
    std::uint64_t boxTests = 0;
    void recordPrimitive() { if (enabled) ++primitiveTests; }
    void recordBox() { if (enabled) ++boxTests; }
};

struct RenderStats : QueryStats {
    double prepareMs = 0.;
    double buildMs = 0.;
    double traceMs = 0.;
    double totalMs = 0.;
    std::uint64_t primaryRays = 0, secondaryRays = 0, shadowRays = 0;
    std::size_t primitiveCount = 0, nodeCount = 0, leafReferences = 0;
    unsigned int treeDepth = 0;
    std::string describe(const std::string& backend) const;
};
}
