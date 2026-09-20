#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/job/job_system.h"
#include "runtime/function/render/mesh_loader.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <glm/glm.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_eq_u32(const char* label, uint32_t actual, uint32_t expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s (got %u want %u)\n", label, actual, expected);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_mesh_loader_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

eastl::vector<Blunder::MeshVertex> makeTriangle(float x_offset) {
  using namespace Blunder;
  eastl::vector<MeshVertex> vertices(3);
  vertices[0].position = glm::vec3(x_offset, 0.0f, 0.0f);
  vertices[1].position = glm::vec3(x_offset + 1.0f, 0.0f, 0.0f);
  vertices[2].position = glm::vec3(x_offset, 1.0f, 0.0f);
  return vertices;
}

eastl::vector<uint32_t> makeTriangleIndices() { return {0, 1, 2}; }

void writeMeshYaml(const fs::path& path, const char* guid) {
  writeTextFile(path, std::string("type: Mesh\n") + "guid: " + guid + "\n" +
                          "source: resources/Models/unused.gltf\n" +
                          "import:\n  materials: false\n  animations: false\n"
                          "  scale: 1\n");
}

size_t boundMeshCount(const Blunder::SceneInstance& scene) {
  size_t count = 0;
  scene.forEachMeshRenderer(
      [&](Blunder::EntityId, const Blunder::MeshRendererComponent& renderer) {
        if (renderer.mesh) {
          ++count;
        }
      });
  return count;
}

size_t pendingMeshCount(const Blunder::SceneInstance& scene) {
  size_t count = 0;
  scene.forEachMeshRenderer(
      [&](Blunder::EntityId, const Blunder::MeshRendererComponent& renderer) {
        if (!renderer.pending_mesh_key.empty()) {
          ++count;
        }
      });
  return count;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuidA = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee01";
  const char* kGuidB = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee02";
  const char* kGuidMissing = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee03";

  writeMeshYaml(project / "Assets" / "Meshes" / "a.mesh.yaml", kGuidA);
  writeMeshYaml(project / "Assets" / "Meshes" / "b.mesh.yaml", kGuidB);
  writeMeshYaml(project / "Assets" / "Meshes" / "missing.mesh.yaml", kGuidMissing);
  expect_true("write meshbin A",
              writeMeshCookFile(project / ".blunder" / "cooked" /
                                    (std::string(kGuidA) + ".meshbin"),
                                makeTriangle(0.0f), makeTriangleIndices()));
  expect_true("write meshbin B",
              writeMeshCookFile(project / ".blunder" / "cooked" /
                                    (std::string(kGuidB) + ".meshbin"),
                                makeTriangle(2.0f), makeTriangleIndices()));

  writeTextFile(project / "Assets" / "Scenes" / "stream.scene.asset",
                "{\n  \"type\": \"Scene\",\n"
                "  \"guid\": \"bbbbbbbb-cccc-4ddd-8eee-ffffffffffff\",\n"
                "  \"entities\": [\n"
                "    { \"name\": \"A0\", \"position\": [0, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"mesh\": \"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee01\" },\n"
                "    { \"name\": \"A1\", \"position\": [1, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"mesh\": \"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee01\" },\n"
                "    { \"name\": \"B0\", \"position\": [2, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"mesh\": \"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee02\" },\n"
                "    { \"name\": \"Missing\", \"position\": [3, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"mesh\": \"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee03\" }\n"
                "  ]\n}\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  JobSystem jobs;
  jobs.initialize(0);

  {
    MeshLoader loader;
    MeshLoader::InitInfo info;
    info.job_system = &jobs;
    loader.initialize(info);
    expect_true("cpu-only loader skips GPU", !loader.isGpuEnabled());

    MeshLoader::Request request_a{};
    request_a.key = kGuidA;
    request_a.guid = kGuidA;
    request_a.virtual_path = "assets/Meshes/a.mesh.yaml";
    request_a.cooked_path =
        project / ".blunder" / "cooked" / (std::string(kGuidA) + ".meshbin");
    loader.request(request_a);
    loader.request(request_a);
    expect_eq_u32("duplicate GUID submits one Job", loader.submittedJobCount(),
                  1u);
    expect_eq_u32("one in-flight record", loader.inFlightCount(), 1u);
    loader.shutdown();
  }

  MeshLoader loader;
  MeshLoader::InitInfo info;
  info.job_system = &jobs;
  info.asset_manager = &manager;
  info.gpu_budget = 1;
  loader.initialize(info);

  SceneSystem scene_system;
  scene_system.initialize(SceneSystemInitInfo{&manager, &loader});

  const eastl::shared_ptr<SceneInstance> instance =
      scene_system.loadScene(eastl::string("assets/Scenes/stream.scene.asset"));
  expect_true("loadScene returns instance", instance != nullptr);
  expect_eq_u32("GUID stream does not open glTF per renderer",
                static_cast<uint32_t>(manager.gltfDocumentOpenCount()), 0u);
  expect_eq_u32("unique Jobs A then B then missing", loader.submittedJobCount(),
                3u);
  expect_true("loadScene returned before Jobs finished",
              loader.inFlightCount() == 3u);
  expect_true("entity table exists", instance && instance->getEntityCount() == 4u);
  expect_eq_u32("pending renderers before Jobs",
                static_cast<uint32_t>(pendingMeshCount(*instance)), 4u);
  expect_eq_u32("no CPU meshes bound before Jobs",
                static_cast<uint32_t>(boundMeshCount(*instance)), 0u);

  const eastl::vector<eastl::string> queued = loader.queuedKeys();
  expect_true("document order queues A before B",
              queued.size() >= 2 && queued[0] == eastl::string(kGuidA) &&
                  queued[1] == eastl::string(kGuidB));

  const uint64_t generation_after_load = loader.generation();
  scene_system.setActiveInstance(instance.get());
  expect_true("first activate does not bump generation",
              loader.generation() == generation_after_load);
  expect_eq_u32("first activate keeps unique-mesh Jobs", loader.inFlightCount(),
                3u);

  const char* kGuidC = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee04";
  writeMeshYaml(project / "Assets" / "Meshes" / "c.mesh.yaml", kGuidC);
  expect_true("write meshbin C",
              writeMeshCookFile(project / ".blunder" / "cooked" /
                                    (std::string(kGuidC) + ".meshbin"),
                                makeTriangle(4.0f), makeTriangleIndices()));
  writeTextFile(project / "Assets" / "Scenes" / "other.scene.asset",
                "{\n  \"type\": \"Scene\",\n"
                "  \"guid\": \"cccccccc-cccc-4ddd-8eee-ffffffffffff\",\n"
                "  \"entities\": [\n"
                "    { \"name\": \"C0\", \"position\": [4, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"mesh\": \"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee04\" }\n"
                "  ]\n}\n");
  const eastl::shared_ptr<SceneInstance> other =
      scene_system.loadScene(eastl::string("assets/Scenes/other.scene.asset"));
  expect_true("load other scene", other != nullptr);
  const uint64_t generation_before_switch = loader.generation();
  scene_system.setActiveInstance(other.get());
  expect_true("scene switch bumps generation",
              loader.generation() == generation_before_switch + 1u);
  expect_true("switch requeues other-scene Jobs", loader.inFlightCount() >= 1u);
  expect_eq_u32("other pending renderer kept",
                static_cast<uint32_t>(pendingMeshCount(*other)), 1u);

  const uint64_t generation_before_reload = loader.generation();
  scene_system.setActiveInstance(instance.get());
  expect_true("reload while Jobs in flight",
              scene_system.reloadActiveFromDisk());
  expect_true("reload bumps generation",
              loader.generation() > generation_before_reload);
  expect_true("reload requeues unique-mesh Jobs", loader.inFlightCount() >= 3u);
  SceneInstance* reloaded = scene_system.getActiveInstance();
  expect_true("reload swapped instance", reloaded != nullptr &&
                                            reloaded != instance.get());
  expect_eq_u32("reloaded pending renderers",
                static_cast<uint32_t>(pendingMeshCount(*reloaded)), 4u);

  jobs.wait();
  expect_true("Jobs still unpublished until tick", loader.cpuMesh(kGuidA) == nullptr);

  loader.tick();
  reloaded->bindStreamedMeshes(loader);
  expect_true("GUID A CPU-resident", loader.cpuMesh(kGuidA) != nullptr);
  expect_true("GUID B CPU-resident", loader.cpuMesh(kGuidB) != nullptr);
  expect_true("missing GUID failed", loader.isFailed(kGuidMissing));
  expect_eq_u32("valid renderers bound",
                static_cast<uint32_t>(boundMeshCount(*reloaded)), 3u);
  expect_true("shared GUID A pointer",
              reloaded->getMeshRenderer(reloaded->findEntityByName("A0")) &&
                  reloaded->getMeshRenderer(reloaded->findEntityByName("A1")) &&
                  reloaded->getMeshRenderer(reloaded->findEntityByName("A0"))
                          ->mesh ==
                      reloaded->getMeshRenderer(reloaded->findEntityByName("A1"))
                          ->mesh);
  expect_true("failed GUID has no mesh",
              reloaded->getMeshRenderer(reloaded->findEntityByName("Missing")) &&
                  reloaded->getMeshRenderer(reloaded->findEntityByName("Missing"))
                          ->mesh == nullptr);
  expect_true("scene instance kept after one fail",
              reloaded->getEntityCount() == 4u);

  const eastl::vector<eastl::string> first_budget = loader.gpuPendingKeys();
  expect_true("GPU pending has unique CPU meshes", first_budget.size() >= 2);
  loader.markGpuUploaded(first_budget.front());
  expect_eq_u32("one sync does not upload every unique mesh",
                loader.gpuEnqueueCount(), 1u);
  expect_true("remaining unique meshes stay pending",
              loader.gpuPendingKeys().size() + 1u == first_budget.size() ||
                  !loader.gpuPendingKeys().empty());

  syncSceneToRender(nullptr, instance.get());
  expect_eq_u32("headless sync does not create GpuMesh", loader.gpuEnqueueCount(),
                1u);

  const uint64_t generation_before = loader.generation();
  loader.dropScene();
  expect_true("scene drop bumps generation",
              loader.generation() == generation_before + 1u);

  scene_system.shutdown();
  loader.shutdown();
  jobs.shutdown();
  manager.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d mesh_loader_test failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
