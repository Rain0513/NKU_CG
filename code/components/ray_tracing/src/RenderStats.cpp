#include "RenderStats.hpp"
#include <iomanip>
#include <sstream>

namespace RayTracing {
std::string RenderStats::describe(const std::string& backend) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << backend << " | primitives=" << primitiveCount << " nodes=" << nodeCount
        << " tree_depth=" << treeDepth << " leaf_refs=" << leafReferences << '\n'
        << "prepare_ms=" << prepareMs << " build_ms=" << buildMs
        << " trace_ms=" << traceMs << " total_ms=" << totalMs << '\n'
        << "primary_rays=" << primaryRays << " secondary_rays=" << secondaryRays
        << " shadow_rays=" << shadowRays << " primitive_tests=" << primitiveTests
        << " box_tests=" << boxTests;
    return out.str();
}
}
