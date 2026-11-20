// Maintained regression and measurement tool. It shares RayTracingCore with the
// DLL and uses the application's actual .scn importer for end-to-end checks.
#include <Windows.h>
// Windows defines RGB(r,g,b); rendering code uses RGB as a vector type.
#undef RGB
#include "RayTracer.hpp"
#include "shading/Optics.hpp"
#include "asset/SceneBuilder.hpp"
#include "importer/ScnImporter.hpp"
#include "component/RenderComponent.hpp"
#include "server/Server.hpp"
#include "glad/glad.h"
#include "glfw3.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using namespace RayTracing;
using NRenderer::Scene;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void addSphere(Scene& scene, Vec3 center, float radius, unsigned int material = 0,
               Vec3 scale = Vec3(1.f)) {
    NRenderer::Model model;
    model.translation = center; model.scale = scale;
    NRenderer::Node node;
    node.model = static_cast<unsigned int>(scene.models.size());
    node.entity = static_cast<unsigned int>(scene.sphereBuffer.size());
    node.type = NRenderer::Node::Type::SPHERE;
    NRenderer::Sphere sphere;
    sphere.radius = radius; sphere.material = NRenderer::Handle(material);
    scene.models.push_back(model); scene.nodes.push_back(node); scene.sphereBuffer.push_back(sphere);
}
Scene makeScene() {
    Scene scene;
    scene.ambient.type = NRenderer::Ambient::Type::CONSTANT;
    scene.ambient.constant = RGB(0.05f);
    NRenderer::Material diffuse, mirror, glass;
    mirror.type = 3; glass.type = 2;
    using W = NRenderer::Property::Wrapper;
    glass.registerProperty("ior", W::FloatType{1.5f});
    scene.materials = {diffuse, mirror, glass};
    scene.camera.position = {0.f, 0.f, -7.f};
    scene.camera.lookAt = {0.f, 0.f, 0.f};
    scene.camera.fov = 45.f;
    scene.renderOption.width = scene.renderOption.height = 32;
    scene.renderOption.samplesPerPixel = 1; scene.renderOption.depth = 6;
    scene.pointLightBuffer.push_back({RGB(1.f), Vec3(-3.f, 5.f, -4.f)});
    return scene;
}
float maxDifference(const std::vector<RGBA>& a, const std::vector<RGBA>& b) {
    require(a.size() == b.size(), "Image dimensions differ.");
    float error = 0.f;
    for (std::size_t i = 0; i < a.size(); ++i) for (int c = 0; c < 4; ++c) {
        require(std::isfinite(a[i][c]) && std::isfinite(b[i][c]), "Non-finite image value.");
        error = std::max(error, std::abs(a[i][c] - b[i][c]));
    }
    return error;
}
void selfTest() {
    // Slabs: on-boundary, parallel miss, inside start, bounded segment.
    AABB box; box.expand(Vec3(-1.f)); box.expand(Vec3(1.f));
    float a, b;
    require(box.intersect({{1, 0, -2}, {0, 0, 1}}, 0, infinity, a, b), "Boundary slab miss.");
    require(!box.intersect({{2, 0, -2}, {-0.f, 0, 1}}, 0, infinity, a, b), "Parallel slab false hit.");
    require(box.intersect({{0, 0, 0}, {1, 0, 0}}, 0, infinity, a, b) && a == 0, "Inside slab miss.");
    require(!box.intersect({{0, 0, -3}, {0, 0, 1}}, 0, 1, a, b), "Segment passed light.");

    Primitive sphere; sphere.radius = 1.f;
    HitRecord hit;
    require(intersectPrimitive(sphere, {{0, 0, -3}, {0, 0, 1}}, 0, infinity, hit) &&
            std::abs(hit.t - 2.f) < 1e-5f && hit.frontFace, "Sphere entry wrong.");
    require(intersectPrimitive(sphere, {{0, 0, 0}, {0, 0, 1}}, 0, infinity, hit) &&
            std::abs(hit.t - 1.f) < 1e-5f && !hit.frontFace, "Sphere exit wrong.");
    require(intersectPrimitive(sphere, {{1, 0, -3}, {0, 0, 1}}, 0, infinity, hit), "Tangent sphere missed.");
    sphere.scale = {2, 1, 1};
    require(intersectPrimitive(sphere, {{-4, 0, 0}, {1, 0, 0}}, 0, infinity, hit) &&
            std::abs(hit.t - 2.f) < 1e-5f, "Scaled sphere t wrong.");
    Primitive plane; plane.type = Primitive::Type::Plane;
    plane.p0 = {-1,-1,0}; plane.p1 = {2,0,0}; plane.p2 = {0,2,0}; plane.normal = {0,0,-1};
    require(intersectPrimitive(plane, {{0,0,-2},{0,0,1}}, 0, infinity, hit), "Finite plane missed.");
    require(!intersectPrimitive(plane, {{2,0,-2},{0,0,1}}, 0, infinity, hit), "Finite plane treated as infinite.");
    Primitive triangle; triangle.type = Primitive::Type::Triangle;
    triangle.p0 = {-1,-1,0}; triangle.p1 = {1,-1,0}; triangle.p2 = {0,1,0}; triangle.normal = {0,0,-1};
    require(intersectPrimitive(triangle, {{0,0,-2},{0,0,1}}, 0, infinity, hit), "Triangle front missed.");
    require(intersectPrimitive(triangle, {{0,0,2},{0,0,-1}}, 0, infinity, hit), "Triangle back missed.");

    // Regression for Cornell wall corners: double roots round to the same float
    // at this scale, and either visitation order must keep both candidates.
    Primitive left = plane, bottom = plane;
    left.p0 = {278,278,1306}; left.p1 = {0,-556,0}; left.p2 = {0,0,-556}; left.normal = {-1,0,0};
    bottom.p0 = {278,-278,1306}; bottom.p1 = {-556,0,0}; bottom.p2 = {0,0,-556}; bottom.normal = {0,1,0};
    RayTracing::Camera cornerCamera{NRenderer::Camera{}};
    const Ray corner = cornerCamera.shoot(34.5f/192.f, 34.5f/192.f);
    HitRecord wallOne, wallTwo;
    require(intersectPrimitive(left, corner, 0, infinity, wallOne) &&
            intersectPrimitive(bottom, corner, 0, infinity, wallTwo) && wallOne.t == wallTwo.t,
            "Shared-edge regression setup failed.");
    require(intersectPrimitive(left, corner, 0, wallTwo.t, hit) &&
            intersectPrimitive(bottom, corner, 0, wallOne.t, hit), "Rounded shared-edge hit rejected.");

    // Refraction at normal incidence, total internal reflection, offset side.
    Vec3 transmitted;
    require(refractDirection({0,0,1}, {0,0,-1}, 1.f/1.5f, transmitted) &&
            glm::length(transmitted - Vec3(0,0,1)) < 1e-5f, "Normal refraction wrong.");
    require(!refractDirection(glm::normalize(Vec3(0.9f,0,0.43589f)), {0,0,-1}, 1.5f, transmitted),
            "Total internal reflection failed.");
    require(std::abs(fresnelSchlick(1.f, 1.f, 1.5f) - 0.04f) < 1e-5f, "Fresnel wrong.");
    hit.point = Vec3(0); hit.outwardNormal = {0,0,-1};
    require(spawnRay(hit, {0,0,1}).origin.z > 0 && spawnRay(hit, {0,0,-1}).origin.z < 0,
            "Secondary ray starts on wrong side.");

    Scene scene = makeScene();
    for (int z = 0; z < 5; ++z) for (int y = 0; y < 5; ++y) for (int x = 0; x < 5; ++x)
        addSphere(scene, Vec3((x-2)*1.2f, (y-2)*1.2f, z*1.2f), 0.35f);
    SceneGeometry geometry(scene);
    // A large finite wall and triangle cross many split cells.
    plane.id = geometry.primitives.size(); plane.material = NRenderer::Handle(0);
    plane.p0 = {-4,-4,8}; plane.p1 = {8,0,0}; plane.p2 = {0,8,0};
    plane.bounds.expand(Vec3(-4,-4,7.999f)); plane.bounds.expand(Vec3(4,4,8.001f));
    geometry.primitives.push_back(plane);
    triangle.id = geometry.primitives.size(); triangle.material = NRenderer::Handle(0);
    triangle.bounds.expand(Vec3(-1,-1,-0.001f)); triangle.bounds.expand(Vec3(1,1,0.001f));
    geometry.primitives.push_back(triangle);
    BruteForce brute(geometry.primitives);
    KDTree tree(geometry.primitives, 4);
    require(tree.nodeCount() > 1, "KD-tree did not subdivide test geometry.");
    std::mt19937 random(20261119);
    std::uniform_real_distribution<float> position(-7.f, 7.f), distance(0.01f, 20.f);
    for (int i = 0; i < 8000; ++i) {
        Vec3 direction(position(random), position(random), position(random));
        if (i % 4 == 0) { direction = Vec3(0); direction[(i / 4) % 3] = i % 8 ? 1.f : -1.f; }
        if (glm::dot(direction, direction) < 1e-8f) direction = {0,0,1};
        const Ray ray{{position(random), position(random), position(random)}, glm::normalize(direction)};
        HitRecord one, two; QueryStats oneStats, twoStats;
        const float limit = i % 3 ? infinity : distance(random);
        const bool h1 = brute.closestHit(ray, 0.0001f, limit, one, oneStats);
        const bool h2 = tree.closestHit(ray, 0.0001f, limit, two, twoStats);
        require(h1 == h2, "KD-tree hit/miss differs from brute force.");
        if (h1) require(one.primitiveId == two.primitiveId && std::abs(one.t-two.t) < 1e-4f,
                        "KD-tree closest primitive differs.");
        require(brute.occluded(ray, 0.0001f, limit, oneStats) == tree.occluded(ray, 0.0001f, limit, twoStats),
                "KD-tree bounded shadow query differs.");
    }
    // Split-plane rays: parallel directions, signed zero, both traversal orders,
    // finite segments and an empty/inverted interval must preserve the baseline.
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
        for (int originIndex = -10; originIndex <= 10; ++originIndex) {
            for (float sign : {-1.f, 1.f}) {
                Vec3 direction(0.f), origin(0.f);
                direction[axisIndex] = sign; origin[(axisIndex + 1) % 3] = originIndex * 0.3f;
                HitRecord one, two; QueryStats q;
                const Ray ray{origin, direction};
                const bool b = brute.closestHit(ray, 0.0001f, infinity, one, q);
                const bool k = tree.closestHit(ray, 0.0001f, infinity, two, q);
                require(b == k && (!b || one.primitiveId == two.primitiveId), "Axis/split traversal mismatch.");
                require(!tree.occluded(ray, 3.f, 2.f, q), "Inverted interval should miss.");
            }
        }
    }
    std::vector<Primitive> empty;
    KDTree emptyTree(empty); QueryStats query;
    require(!emptyTree.closestHit({{0,0,0},{0,0,1}}, 0, infinity, hit, query), "Empty tree hit.");
    // A miss must clear the output record consistently across both backends.
    const Ray farMiss{{1e6f, 1e6f, 1e6f}, {1.f, 0.f, 0.f}};
    hit.t = 1.f; hit.primitiveId = 7;
    require(!brute.closestHit(farMiss, 0.f, infinity, hit, query) &&
            hit.primitiveId == std::numeric_limits<std::size_t>::max(),
            "Brute-force miss retained a stale hit record.");
    hit.t = 1.f; hit.primitiveId = 7;
    require(!tree.closestHit(farMiss, 0.f, infinity, hit, query) &&
            hit.primitiveId == std::numeric_limits<std::size_t>::max(),
            "KD-tree miss retained a stale hit record.");
    // Coincident primitives must choose the same stable ID despite traversal order.
    auto duplicates = geometry.primitives;
    duplicates.push_back(duplicates.front()); duplicates.back().id = duplicates.size() - 1;
    KDTree duplicateTree(duplicates, 1, 8); BruteForce duplicateBrute(duplicates);
    HitRecord first, second;
    Ray axis{{-2.4f,-2.4f,-3.f},{0,0,1}};
    require(duplicateTree.closestHit(axis, 0, infinity, first, query) &&
            duplicateBrute.closestHit(axis, 0, infinity, second, query) &&
            first.primitiveId == second.primitiveId, "Coincident primitive tie differs.");

    Scene optical = makeScene();
    addSphere(optical, {-1.1f,0,0}, 1.f, 1);
    addSphere(optical, {1.1f,0,0}, 1.f, 2);
    addSphere(optical, {0,-101.f,0}, 100.f, 0);
    const auto original = optical.sphereBuffer[0].position;
    RayTracer aRenderer(optical, Backend::BruteForce), bRenderer(optical, Backend::KDTree);
    const auto image = aRenderer.render();
    require(maxDifference(image, bRenderer.render()) < 1e-5f, "Recursive images differ.");
    require(maxDifference(image, aRenderer.render()) == 0.f, "Repeat rendering is nondeterministic.");
    RayTracer counted(optical, Backend::KDTree, true);
    require(maxDifference(image, counted.render()) == 0.f && counted.statistics().primitiveTests > 0 &&
            bRenderer.statistics().primitiveTests == 0, "Counter mode changes pixels or is not optional.");
    require(optical.sphereBuffer[0].position == original, "Rendering changed source geometry.");
    optical.renderOption.depth = 0;
    RayTracer noRecursion(optical, Backend::BruteForce);
    require(maxDifference(image, noRecursion.render()) > 0.05f, "Reflection/refraction have no visible contribution.");

    // Constant Ambient must illuminate opaque materials, not only missed rays.
    Scene ambientScene = makeScene();
    ambientScene.pointLightBuffer.clear(); ambientScene.ambient.constant = RGB(0.f);
    ambientScene.renderOption.width = ambientScene.renderOption.height = 1;
    addSphere(ambientScene, {0,0,0}, 1.f);
    RayTracer unlit(ambientScene, Backend::BruteForce);
    require(glm::length(Vec3(unlit.render()[0])) == 0.f, "Zero fill should remain unlit.");
    ambientScene.ambient.constant = RGB(0.04f);
    RayTracer globalFill(ambientScene, Backend::KDTree);
    const auto globalFillImage = globalFill.render();
    require(glm::length(Vec3(globalFillImage[0])) > 0.1f, "UI Ambient does not light opaque surfaces.");
    ambientScene.ambient.constant = RGB(0.f);
    ambientScene.materials[0].registerProperty("ambientColor", NRenderer::Property::Wrapper::RGBType{RGB(0.04f)});
    RayTracer materialFill(ambientScene, Backend::BruteForce);
    require(maxDifference(globalFillImage, materialFill.render()) < 1e-6f,
            "Material ambientColor and global constant fill disagree.");

    Scene meshScene = makeScene();
    NRenderer::Mesh mesh;
    mesh.positions = {{-1,-1,0}, {1,-1,0}, {0,1,0}};
    mesh.positionIndices = {0,1,2}; mesh.material = NRenderer::Handle(0);
    meshScene.meshBuffer.push_back(mesh);
    NRenderer::Model meshModel; meshModel.translation = {0,0,2}; meshModel.scale = {2,1,1};
    meshScene.models.push_back(meshModel);
    NRenderer::Node meshNode; meshNode.type = NRenderer::Node::Type::MESH; meshNode.entity = 0; meshNode.model = 0;
    meshScene.nodes.push_back(meshNode);
    SceneGeometry meshGeometry(meshScene);
    require(meshGeometry.primitives.size() == 1 &&
            intersectPrimitive(meshGeometry.primitives[0], {{0,0,-2},{0,0,1}}, 0, infinity, hit) &&
            std::abs(hit.t - 4.f) < 1e-5f, "Mesh transform/triangle expansion failed.");

    // Load and execute both actual DLL registrations, not only the core library.
    const HMODULE dll = LoadLibraryA("components\\RayTracing.dll");
    require(dll != nullptr, "Cannot load components/RayTracing.dll (run from build folder).");
    for (const char* name : {"RayTracingBruteForce", "RayTracingKDTree"}) {
        auto component = getServer().componentFactory.createComponent<NRenderer::RenderComponent>("Render", name);
        require(bool(component), "Renderer registration missing.");
        bool started = false, finished = false;
        component->exec([&]() { started = true; }, [&]() { finished = true; }, std::make_shared<Scene>(optical));
        require(started && finished && getServer().screen.getWidth() == 32, "Plugin execution failed.");
    }
    FreeLibrary(dll);
    std::cout << "PASS: geometry, slabs, optics, 8000 closest/any-hit pairs, recursive images, repeatability, DLL registration.\n";
}

class ImportContext {
public:
    ImportContext() {
        require(glfwInit() != 0, "GLFW initialization failed.");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(32, 32, "Scene import validation", nullptr, nullptr);
        require(window != nullptr, "Cannot create OpenGL context for the actual scene importer.");
        glfwMakeContextCurrent(window);
        require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GLAD initialization failed.");
    }
    ~ImportContext() { if (window) glfwDestroyWindow(window); glfwTerminate(); }
private:
    GLFWwindow* window = nullptr;
};

void saveBMP(const std::filesystem::path& path, const std::vector<RGBA>& pixels, unsigned int size) {
    std::ofstream output(path, std::ios::binary);
    require(bool(output), "Cannot write preview image.");
    const std::uint32_t stride = (size * 3u + 3u) & ~3u;
    unsigned char header[54]{};
    auto put32 = [&](int offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i) header[offset + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    header[0] = 'B'; header[1] = 'M'; put32(2, 54u + stride * size); put32(10, 54);
    put32(14, 40); put32(18, size); put32(22, 0u - size); // Negative height: top-down rows.
    header[26] = 1; header[28] = 24; put32(34, stride * size);
    output.write(reinterpret_cast<const char*>(header), sizeof(header));
    std::vector<unsigned char> row(stride, 0);
    for (unsigned int y = 0; y < size; ++y) {
        for (unsigned int x = 0; x < size; ++x) for (int c = 0; c < 3; ++c)
            row[x * 3 + (2 - c)] = static_cast<unsigned char>(
                std::clamp(pixels[std::size_t(y) * size + x][c], 0.f, 1.f) * 255.f + 0.5f);
        output.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
}

void measureScene(const std::string& path, const std::filesystem::path& output,
                  unsigned int size, unsigned int samples, unsigned int depth, unsigned int repeat,
                  bool diagnostics, bool showcaseCamera) {
    ImportContext context;
    NRenderer::Asset asset;
    NRenderer::ScnImporter importer;
    if (!importer.import(asset, path)) throw std::runtime_error("Scene import failed: " + importer.getErrorInfo());
    NRenderer::RenderSettings settings;
    settings.width = settings.height = size; settings.samplesPerPixel = samples; settings.depth = depth;
    NRenderer::AmbientSettings ambient;
    NRenderer::Camera camera; // Same defaults as a fresh application session.
    if (showcaseCamera) {
        // An explicit inspection viewpoint, never silently applied to benchmarks.
        camera.position = {0.f, 165.f, 100.f};
        camera.lookAt = {0.f, -100.f, 1050.f};
        camera.fov = 43.f;
    }
    NRenderer::SceneBuilder builder(asset, settings, ambient, camera);
    const auto scene = builder.build();
    require(bool(scene), "Scene builder failed.");
    // End-to-end topology/optics validation for the generated glass tetrahedra.
    // All four vertices are equally represented, so their mean is interior.
    SceneGeometry importedGeometry(*scene);
    for (std::size_t model = 0; model < asset.modelItems.size(); ++model) {
        if (asset.modelItems[model].name != "GlassPyramid") continue;
        std::vector<const Primitive*> faces; Vec3 interior(0.f);
        for (std::size_t nodeIndex = 0; nodeIndex < scene->nodes.size(); ++nodeIndex) {
            const auto& node = scene->nodes[nodeIndex];
            if (node.model != model) continue;
            const auto& face = importedGeometry.primitives.at(nodeIndex);
            require(face.type == Primitive::Type::Triangle, "Glass pyramid contains a nontriangle face.");
            faces.push_back(&face); interior += (face.p0 + face.p1 + face.p2) / 3.f;
        }
        require(faces.size() == 4, "Glass pyramid is not a closed four-face tetrahedron.");
        interior /= 4.f;
        std::vector<Primitive> isolatedFaces;
        for (const auto* face : faces) isolatedFaces.push_back(*face);
        BruteForce solidBrute(isolatedFaces); KDTree solidTree(isolatedFaces);
        // Full-scene contact regression: the glass boundary must be hit before
        // the floor from inside. Isolated-solid checks alone miss coplanar ties.
        BruteForce fullBrute(importedGeometry.primitives);
        KDTree fullTree(importedGeometry.primitives);
        for (const Intersector* backend : {static_cast<const Intersector*>(&fullBrute),
                                           static_cast<const Intersector*>(&fullTree)}) {
            HitRecord bottomHit, floorHit; QueryStats q;
            require(backend->closestHit({interior, {0.f,-1.f,0.f}}, rayEpsilon, infinity, bottomHit, q)
                    && !bottomHit.frontFace && scene->materials[bottomHit.material.index()].type == 2,
                    "Glass base was bypassed by a coplanar floor.");
            Vec3 outside;
            require(refractDirection({0.f,-1.f,0.f}, bottomHit.normal, 1.5f, outside),
                    "Normal bottom exit refraction failed.");
            const Ray afterExit = spawnRay(bottomHit, outside);
            require(backend->closestHit(afterExit, rayEpsilon*.25f, infinity, floorHit, q)
                    && scene->materials[floorHit.material.index()].type == 0
                    && floorHit.t > rayEpsilon && floorHit.t < 0.2f,
                    "Glass-to-air clearance does not lead to the floor.");
        }
        for (const auto* face : faces) {
            require(glm::dot(face->normal, interior - face->p0) < 0.f, "Glass face normal points inward.");
            const Vec3 center = (face->p0 + face->p1 + face->p2) / 3.f;
            HitRecord entry, exit; QueryStats q;
            const Ray incoming{center + 10.f * face->normal, -face->normal};
            require(solidBrute.closestHit(incoming, rayEpsilon, infinity, entry, q) && entry.frontFace,
                    "Glass entry should be front-facing.");
            Vec3 inside;
            require(refractDirection(incoming.direction, entry.normal, 1.f/1.5f, inside), "Glass entry refraction failed.");
            const Ray insideRay = spawnRay(entry, inside);
            require(solidTree.closestHit(insideRay, rayEpsilon*.25f, infinity, exit, q) && !exit.frontFace,
                    "Glass tetrahedron did not produce an inside-to-outside hit.");
        }
    }
    // Showcase layout regressions verify the optional older prism scene.
    for (std::size_t model = 0; model < asset.modelItems.size(); ++model) {
        const auto& name = asset.modelItems[model].name;
        if (name != "MirrorPrism" && name != "DiffuseSphere") continue;
        std::vector<const Primitive*> faces;
        for (std::size_t i = 0; i < scene->nodes.size(); ++i)
            if (scene->nodes[i].model == model) faces.push_back(&importedGeometry.primitives.at(i));
        if (name == "DiffuseSphere") {
            require(faces.size() == 1 && faces[0]->type == Primitive::Type::Sphere,
                    "Showcase diffuse object must remain an analytic sphere.");
            require(faces[0]->p0.y - faces[0]->radius > -278.f,
                    "Showcase sphere intersects the Cornell floor.");
            continue;
        }
        require(faces.size() == 5 && faces[1]->type == Primitive::Type::Triangle,
                "Mirror prism must have two triangular caps and three side planes.");
        const auto& cap = *faces[1];
        const Vec3 center = (cap.p0 + cap.p1 + cap.p2) / 3.f;
        for (std::size_t i = 2; i < faces.size(); ++i)
            require(std::abs(glm::dot(cap.normal, faces[i]->normal)) < 1e-4f,
                    "Mirror prism was sheared instead of rigidly tilted.");
        for (const auto eye : {Vec3(0.f, 0.f, -3.f), Vec3(0.f, 165.f, 100.f)}) {
            require(glm::dot(cap.normal, eye - center) > 0.f,
                    "Mirror top cap faces away from an inspection camera.");
            BruteForce brute(importedGeometry.primitives); KDTree tree(importedGeometry.primitives);
            for (const Intersector* backend : {static_cast<const Intersector*>(&brute),
                                               static_cast<const Intersector*>(&tree)}) {
                HitRecord hit; QueryStats q;
                require(backend->closestHit({eye, glm::normalize(center - eye)}, rayEpsilon, infinity, hit, q)
                        && hit.frontFace && hit.primitiveId == cap.id,
                        "Mirror top cap is occluded from an inspection camera.");
            }
        }
    }
    std::filesystem::create_directories(output);
    std::ofstream csv(output / "measurements.csv");
    csv << "run,backend,width,height,samples,depth,primitives,nodes,leaf_refs,tree_depth,prepare_ms,build_ms,trace_ms,total_ms,primary_rays,secondary_rays,shadow_rays,primitive_tests,box_tests,max_pixel_error,counters_enabled,execution_ms,camera_preset\n";
    // Warm up each backend. Timed runs always construct a fresh tree/renderer.
    for (const auto backend : {Backend::BruteForce, Backend::KDTree}) {
        RayTracer warmup(*scene, backend); warmup.render();
    }
    for (unsigned int run = 0; run < repeat; ++run) {
        std::vector<RGBA> reference;
        // Alternate the first backend to reduce systematic thermal/order bias.
        const Backend order[2] = {run % 2 ? Backend::KDTree : Backend::BruteForce,
                                  run % 2 ? Backend::BruteForce : Backend::KDTree};
        for (const auto backend : order) {
            const auto started = std::chrono::steady_clock::now();
            RenderStats s;
            std::vector<RGBA> pixels;
            {
                RayTracer renderer(*scene, backend, diagnostics);
                pixels = renderer.render();
                getServer().screen.set(pixels.data(), size, size);
                s = renderer.statistics();
            } // Include destruction of prepared geometry and tree in total time.
            // Same boundary as the GUI: setup through pixel delivery, including
            // tree construction; file output and image comparison are excluded.
            const double executionMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            const char* name = backend == Backend::KDTree ? "KDTree" : "BruteForce";
            float error = 0.f;
            if (reference.empty()) reference = pixels;
            else error = maxDifference(reference, pixels);
            if (error >= 1e-4f)
                throw std::runtime_error("Imported scene: backend images differ, maximum error=" + std::to_string(error));
            csv << run + 1 << ',' << name << ',' << size << ',' << size << ',' << samples << ',' << depth << ','
                << s.primitiveCount << ',' << s.nodeCount << ',' << s.leafReferences << ',' << s.treeDepth << ','
                << s.prepareMs << ',' << s.buildMs << ',' << s.traceMs << ',' << s.totalMs << ','
                << s.primaryRays << ',' << s.secondaryRays << ',' << s.shadowRays << ','
                << s.primitiveTests << ',' << s.boxTests << ',' << error << ',' << diagnostics << ',' << executionMs
                << ',' << (showcaseCamera ? "showcase" : "default") << '\n';
            if (run == 0) saveBMP(output / (std::string(name) + ".bmp"), pixels, size);
            std::cout << name << " total_ms=" << executionMs << '\n';
        }
    }
    asset.clearModel(); asset.clearLight();
}
}

int main(int argc, char** argv) {
    try {
        std::string scene;
        std::filesystem::path output = "ray-tracing-results";
        unsigned int size = 128, samples = 1, depth = 6, repeat = 3;
        bool tests = argc == 1;
        bool diagnostics = false;
        bool showcaseCamera = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--self-test") tests = true;
            else if (arg == "--diagnostics") diagnostics = true;
            else if (arg == "--showcase-camera") showcaseCamera = true;
            else if (arg == "--scene" && i + 1 < argc) scene = argv[++i];
            else if (arg == "--output" && i + 1 < argc) output = argv[++i];
            else if (arg == "--size" && i + 1 < argc) size = std::stoul(argv[++i]);
            else if (arg == "--samples" && i + 1 < argc) samples = std::stoul(argv[++i]);
            else if (arg == "--depth" && i + 1 < argc) depth = std::stoul(argv[++i]);
            else if (arg == "--repeat" && i + 1 < argc) repeat = std::stoul(argv[++i]);
            else throw std::runtime_error("Usage: RayTracingCheck [--self-test] [--diagnostics] [--showcase-camera] [--scene file.scn --output folder --size 128 --samples 1 --depth 6 --repeat 3]");
        }
        require(size > 0 && repeat > 0, "Size/repeat must be positive.");
        if (tests) selfTest();
        if (!scene.empty()) measureScene(scene, output, size, samples, depth, repeat, diagnostics, showcaseCamera);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
