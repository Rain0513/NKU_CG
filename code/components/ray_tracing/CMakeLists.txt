cmake_minimum_required(VERSION 3.18)

# Keep the algorithms separate from plugin registration, so the verification
# executable exercises exactly the same implementation as the visible DLL.
add_library(RayTracingCore STATIC
    src/RayTracer.cpp
    src/RenderStats.cpp
    src/geometry/SceneGeometry.cpp
    src/geometry/Intersections.cpp
    src/acceleration/AABB.cpp
    src/acceleration/BruteForce.cpp
    src/acceleration/KDTree.cpp
    src/shading/MaterialParameters.cpp
)
target_include_directories(RayTracingCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(RayTracingCore PUBLIC cxx_std_17)
target_compile_definitions(RayTracingCore PUBLIC NOMINMAX)
if(MSVC)
    target_compile_options(RayTracingCore PRIVATE /utf-8 /W4)
endif()

add_library(RayTracing SHARED src/Adapter.cpp)
target_link_libraries(RayTracing PRIVATE RayTracingCore NRServer)
add_dependencies(NRenderer RayTracing)

# One maintained integration check replaces disposable test programs. The
# default application build stays small; enable this target for verification.
option(NR_BUILD_RAY_TRACING_CHECK "Build ray tracing regression/benchmark tool" OFF)
if(NR_BUILD_RAY_TRACING_CHECK)
    add_executable(RayTracingCheck tools/RayTracingCheck.cpp)
    target_include_directories(RayTracingCheck PRIVATE "${APP_DIR}/include")
    target_link_libraries(RayTracingCheck PRIVATE RayTracingCore NRApp)
    set_target_properties(RayTracingCheck PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
