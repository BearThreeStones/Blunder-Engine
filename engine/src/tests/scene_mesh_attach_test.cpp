#include "runtime/core/log/log_system.h"
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

void expect_eq_size(const char* label, size_t actual, size_t expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s (got %zu want %zu)\n", label, actual, expected);
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
      ("blunder_scene_mesh_attach_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Resources" / "Models");
  return root;
}

// Two primitives on one mesh node — same shape as spawn_mesh_primitives_test.
constexpr char kDualPrimitiveGltf[] = R"({
  "asset": { "version": "2.0" },
  "materials": [
    { "pbrMetallicRoughness": { "baseColorFactor": [0.85, 0.12, 0.12, 1.0] } },
    { "pbrMetallicRoughness": { "baseColorFactor": [0.12, 0.12, 0.85, 1.0] } }
  ],
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "Cube", "mesh": 0 }],
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

constexpr int k_shared_mesh_entity_count = 8;

size_t liveMeshRendererCount(const Blunder::SceneInstance& scene) {
  size_t count = 0;
  scene.forEachMeshRenderer(
      [&](Blunder::EntityId, const Blunder::MeshRendererComponent&) { ++count; });
  return count;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee42";
  const char* kDescriptorPath = "assets/Meshes/cube.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "cube.gltf",
                kDualPrimitiveGltf);
  writeTextFile(project / "Assets" / "Meshes" / "cube.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/cube.gltf\n" +
                    "import:\n  materials: true\n  animations: false\n"
                    "  scale: 1\n");

  std::string scene_json = "{\n  \"type\": \"Scene\",\n"
                           "  \"guid\": \"bbbbbbbb-cccc-4ddd-8eee-ffffffffffff\",\n"
                           "  \"entities\": [\n";
  for (int i = 0; i < k_shared_mesh_entity_count; ++i) {
    if (i != 0) {
      scene_json += ",\n";
    }
    scene_json += "    { \"name\": \"Box";
    scene_json += std::to_string(i);
    scene_json += "\", \"position\": [0, 0, 0], \"rotation\": [0, 0, 0], "
                  "\"rotationMode\": \"euler_degrees\", \"mesh\": \"";
    scene_json += kDescriptorPath;
    scene_json += "\" }";
  }
  scene_json += "\n  ]\n}\n";
  writeTextFile(project / "Assets" / "Scenes" / "shared_mesh.scene.asset",
                scene_json);

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

  const auto started = std::chrono::steady_clock::now();
  const eastl::shared_ptr<SceneInstance> instance =
      scene_system.loadScene(eastl::string("assets/Scenes/shared_mesh.scene.asset"));
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - started)
                              .count();

  expect_true("loadScene returns instance", instance != nullptr);
  if (instance) {
    expect_eq_size("named mesh entities exist",
                   static_cast<size_t>(k_shared_mesh_entity_count),
                   [&] {
                     size_t named = 0;
                     for (int i = 0; i < k_shared_mesh_entity_count; ++i) {
                       const eastl::string name =
                           eastl::string("Box") + eastl::string(std::to_string(i).c_str());
                       if (isValid(instance->findEntityByName(name))) {
                         ++named;
                       }
                     }
                     return named;
                   }());
    expect_true("each shared-mesh entity has primitive renderers",
                liveMeshRendererCount(*instance) >=
                    static_cast<size_t>(k_shared_mesh_entity_count) * 2u);
  }
  expect_eq_size("shared mesh glTF parsed once on scene open",
                 manager.gltfDocumentOpenCount(), 1u);
  expect_true("shared-mesh scene open stays under 500 ms", elapsed_ms < 500);

  writeTextFile(project / "Assets" / "Scenes" / "reused_children.scene.asset",
                "{\n  \"type\": \"Scene\",\n"
                "  \"guid\": \"cccccccc-dddd-4eee-8fff-000000000001\",\n"
                "  \"entities\": [\n"
                "    { \"name\": \"Box0\", \"position\": [0, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"mesh\": \"assets/Meshes/cube.mesh.yaml\" },\n"
                "    { \"name\": \"Cube\", \"position\": [0, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"parent\": \"Box0\" },\n"
                "    { \"name\": \"Cube_prim0\", \"position\": [0, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"parent\": \"Cube\" },\n"
                "    { \"name\": \"Cube_prim1\", \"position\": [0, 0, 0], "
                "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                "\"parent\": \"Cube\" }\n"
                "  ]\n}\n");

  SceneSystem reused_system;
  reused_system.initialize(SceneSystemInitInfo{&manager});
  const eastl::shared_ptr<SceneInstance> reused = reused_system.loadScene(
      eastl::string("assets/Scenes/reused_children.scene.asset"));
  expect_true("reused-children loadScene returns instance", reused != nullptr);
  if (reused) {
    expect_eq_size("saved glTF children are not duplicated on reopen",
                   reused->getEntityCount(), 4u);
    const EntityId prim0 = reused->findEntityByName("Cube_prim0");
    expect_true("saved primitive entity kept", isValid(prim0));
    expect_true("saved primitive received renderer",
                reused->getMeshRenderer(prim0) != nullptr &&
                    reused->getMeshRenderer(prim0)->mesh);
  }

  reused_system.shutdown();
  scene_system.shutdown();
  manager.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "scene_mesh_attach_test: all passed\n");
  return 0;
}
