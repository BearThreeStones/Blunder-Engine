#include "runtime/core/log/log_system.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <chrono>
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
      ("blunder_spawn_mesh_primitives_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Resources" / "Models");
  return root;
}

// Two primitives (two materials) on one mesh node — same shape as a glTF
// character split by material (e.g. ears vs body).
constexpr char kDualPrimitiveGltf[] = R"({
  "asset": { "version": "2.0" },
  "materials": [
    { "pbrMetallicRoughness": { "baseColorFactor": [0.85, 0.12, 0.12, 1.0] } },
    { "pbrMetallicRoughness": { "baseColorFactor": [0.12, 0.12, 0.85, 1.0] } }
  ],
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [
      {
        "attributes": { "POSITION": 1 },
        "indices": 0,
        "material": 0
      },
      {
        "attributes": { "POSITION": 1 },
        "indices": 0,
        "material": 1
      }
    ]
  }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5123,
      "count": 3,
      "type": "SCALAR"
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [1.0, 1.0, 0.0],
      "min": [0.0, 0.0, 0.0]
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 36 }
  ],
  "buffers": [{
    "byteLength": 44,
    "uri": "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/"
  }]
}
)";

size_t liveMeshRendererCount(const Blunder::SceneInstance& scene) {
  size_t count = 0;
  scene.forEachMeshRenderer(
      [&](Blunder::EntityId, const Blunder::MeshRendererComponent&) { ++count; });
  return count;
}

void spawnMeshAssetAttachesEveryGltfPrimitive() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee21";
  const char* kDescriptorPath = "assets/Meshes/dog.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "dog.gltf",
                kDualPrimitiveGltf);
  writeTextFile(project / "Assets" / "Meshes" / "dog.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/dog.gltf\n" +
                    "import:\n  materials: true\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  SceneSystem scene_system;
  scene_system.initialize(SceneSystemInitInfo{&manager});
  const eastl::shared_ptr<SceneInstance> instance =
      eastl::make_shared<SceneInstance>();
  scene_system.setActiveInstance(instance.get());

  EditorSceneEditSystem editor;
  editor.initialize(&file_system, &manager, &scene_system);

  const SpawnAssetResult result =
      editor.spawnAssetAtWindowPosition(eastl::string(kDescriptorPath), 0.0f,
                                        0.0f);
  expect_true("spawn succeeds", result.success);
  expect_true("spawn returns entity", isValid(result.spawned_entity));
  expect_true("spawn attaches every glTF primitive",
              liveMeshRendererCount(*instance) >= 2u);

  expect_true("softDelete spawn root",
              instance->softDeleteEntity(result.spawned_entity));
  expect_true("child primitives hidden while spawn root tombstoned",
              liveMeshRendererCount(*instance) == 0u);
  expect_true("restore spawn root",
              instance->restoreEntity(result.spawned_entity));
  expect_true("child primitives live after spawn root restore",
              liveMeshRendererCount(*instance) >= 2u);

  scene_system.shutdown();
  manager.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void spawnChocomelRebuildsPoseBuffers() {
  using namespace Blunder;
  const fs::path project = "E:/Blunder Projects/Test";
  const fs::path descriptor = project / "Assets" / "Meshes" / "Chocomel.mesh.yaml";
  if (!fs::exists(descriptor)) {
    std::fprintf(stderr, "SKIP Chocomel pose-cache: missing %s\n",
                 descriptor.string().c_str());
    return;
  }

  ensureLogger();

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  SceneSystem scene_system;
  scene_system.initialize(SceneSystemInitInfo{&manager});
  const eastl::shared_ptr<SceneInstance> instance =
      eastl::make_shared<SceneInstance>();
  scene_system.setActiveInstance(instance.get());

  EditorSceneEditSystem editor;
  editor.initialize(&file_system, &manager, &scene_system);

  const SpawnAssetResult result = editor.spawnAssetAtWindowPosition(
      eastl::string("assets/Meshes/Chocomel.mesh.yaml"), 0.0f, 0.0f);
  expect_true("Chocomel spawn succeeds", result.success);

  instance->tick(0.0f);

  size_t skinned_count = 0;
  size_t pose_valid_count = 0;
  instance->forEachMeshRenderer(
      [&](EntityId entity_id, const MeshRendererComponent& renderer) {
        if (!renderer.mesh || !renderer.mesh->isSkinned()) {
          return;
        }
        ++skinned_count;
        Skeleton* skeleton = instance->findSkeletonForEntity(entity_id);
        if (skeleton != nullptr && skeleton->hasValidPoseBuffers()) {
          ++pose_valid_count;
        }
      });

  expect_true("Chocomel spawn leaves pose buffers valid",
              pose_valid_count == skinned_count && skinned_count > 0u);

  scene_system.shutdown();
  manager.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
}

}  // namespace

int main() {
  spawnMeshAssetAttachesEveryGltfPrimitive();
  spawnChocomelRebuildsPoseBuffers();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "spawn_mesh_primitives_test: all passed\n");
  return 0;
}
