#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/gltf_scene_importer.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/scene/se_world_flatten.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/project/editor_session_restore.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/function/scene/scene_system.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <glm/gtc/quaternion.hpp>

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

fs::path makeTempRoot(const char* tag) {
  const fs::path root =
      fs::temp_directory_path() /
      (std::string("blunder_se_world_flatten_") + tag + "_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root);
  return root;
}

constexpr char kTriangleGltfBody[] = R"(
  "asset": { "version": "2.0" },
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0
    }]
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
)";

std::string triangleGltf(const std::string& nodes_json, const std::string& scene_name) {
  return std::string("{\n") + kTriangleGltfBody +
         ",\n  \"scene\": 0,\n  \"scenes\": [{ \"name\": \"" + scene_name +
         "\", \"nodes\": [0] }],\n  \"nodes\": " + nodes_json + "\n}\n";
}

std::string triangleGltfNodes(const std::string& nodes_json,
                              const std::string& scene_name,
                              const std::string& node_indices) {
  return std::string("{\n") + kTriangleGltfBody +
         ",\n  \"scene\": 0,\n  \"scenes\": [{ \"name\": \"" + scene_name +
         "\", \"nodes\": " + node_indices + " }],\n  \"nodes\": " + nodes_json +
         "\n}\n";
}

const Blunder::SceneEntityDefinition* findEntity(const Blunder::Scene& scene,
                                                 const char* name) {
  for (const Blunder::SceneEntityDefinition& entity : scene.getEntities()) {
    if (entity.name == name) {
      return &entity;
    }
  }
  return nullptr;
}

size_t liveMeshRendererCount(const Blunder::SceneInstance& scene) {
  size_t count = 0;
  scene.forEachMeshRenderer(
      [&](Blunder::EntityId, const Blunder::MeshRendererComponent&) { ++count; });
  return count;
}

std::string assetIndexTwo(const char* id_a, const char* path_a, const char* id_b,
                          const char* path_b) {
  std::string json = "{\n  \"assets\": {\n";
  json += std::string("    \"") + id_a + "\": { \"name\": \"A\", \"filepath\": \"" +
          path_a + "\" }";
  if (id_b != nullptr) {
    json += std::string(",\n    \"") + id_b +
            "\": { \"name\": \"B\", \"filepath\": \"" + path_b + "\" }";
  }
  json += "\n  }\n}\n";
  return json;
}

void testBakerTwoInstancesUniqueNames() {
  using namespace Blunder;
  const fs::path root = makeTempRoot("two");
  writeTextFile(root / "assets" / "lib" / "box.gltf",
                triangleGltf("[{ \"name\": \"GEO-box\", \"mesh\": 0 }]", "box"));
  writeTextFile(root / "SE-world.gltf", triangleGltfNodes(
      R"([
        { "name": "Bush", "extras": { "instance_asset_id": "aaaaaaaaaaaaaaaa" },
          "translation": [1, 0, 0] },
        { "name": "Bush", "extras": { "instance_asset_id": "aaaaaaaaaaaaaaaa" },
          "translation": [2, 0, 0] },
        { "name": "Empty" }
      ])",
      "SE-world", "[0, 1, 2]"));
  writeTextFile(root / "asset_index.json",
                assetIndexTwo("aaaaaaaaaaaaaaaa", "assets/lib/box.gltf", nullptr,
                              nullptr));

  SeWorldFlattenOptions options;
  options.layout_gltf = root / "SE-world.gltf";
  options.asset_index_json = root / "asset_index.json";
  options.godot_root = root;
  Scene scene;
  const SeWorldFlattenStats stats = bakeSeWorldFlattenScene(options, scene);
  expect_true("1.1 bake ok", stats.success);
  expect_true("1.1 two layout", stats.layout_instances == 2);
  expect_true("1.1 Bush", findEntity(scene, "Bush") != nullptr);
  expect_true("1.1 Bush_1", findEntity(scene, "Bush_1") != nullptr);
  expect_true("1.1 Empty skipped", findEntity(scene, "Empty") == nullptr);
  expect_true("1.1 GEO-box not exploded under library",
              findEntity(scene, "GEO-box") == nullptr);
  expect_true("1.1 shared mesh guid on instances",
              findEntity(scene, "Bush") != nullptr &&
                  findEntity(scene, "Bush_1") != nullptr &&
                  findEntity(scene, "Bush")->mesh_virtual_path ==
                      findEntity(scene, "Bush_1")->mesh_virtual_path &&
                  !findEntity(scene, "Bush")->mesh_virtual_path.empty());
  eastl::string json;
  expect_true("1.1 serialize", SceneSerializer::serialize(scene, json));
  expect_true("1.1 no childScenes", json.find("childScenes") == eastl::string::npos);
  expect_true("1.1 no multiMesh", json.find("multiMesh") == eastl::string::npos &&
                                      json.find("MultiMesh") == eastl::string::npos);
  fs::remove_all(root);
}

void testSkipMissingId() {
  using namespace Blunder;
  const fs::path root = makeTempRoot("missing");
  writeTextFile(root / "assets" / "lib" / "box.gltf",
                triangleGltf("[{ \"name\": \"GEO-box\", \"mesh\": 0 }]", "box"));
  writeTextFile(root / "SE-world.gltf", triangleGltfNodes(
      R"([
        { "name": "Missing", "extras": { "instance_asset_id": "9c53197a476fa552" } },
        { "name": "Neighbor", "extras": { "instance_asset_id": "bbbbbbbbbbbbbbbb" } }
      ])",
      "SE-world", "[0, 1]"));
  writeTextFile(root / "asset_index.json",
                assetIndexTwo("bbbbbbbbbbbbbbbb", "assets/lib/box.gltf", nullptr,
                              nullptr));

  SeWorldFlattenOptions options;
  options.layout_gltf = root / "SE-world.gltf";
  options.asset_index_json = root / "asset_index.json";
  options.godot_root = root;
  Scene scene;
  const SeWorldFlattenStats stats = bakeSeWorldFlattenScene(options, scene);
  expect_true("1.2 bake ok", stats.success);
  expect_true("1.2 one layout", stats.layout_instances == 1);
  expect_true("1.2 skip missing id", stats.skipped_missing_id == 1);
  expect_true("1.2 neighbor kept", findEntity(scene, "Neighbor") != nullptr);
  expect_true("1.2 missing omitted", findEntity(scene, "Missing") == nullptr);
  fs::remove_all(root);
}

void testNestedLibraryAndMissingFile() {
  using namespace Blunder;
  const fs::path root = makeTempRoot("nested");
  writeTextFile(root / "assets" / "lib" / "leaf.gltf",
                triangleGltf("[{ \"name\": \"GEO-leaf\", \"mesh\": 0 }]", "leaf"));
  writeTextFile(
      root / "assets" / "lib" / "tree.gltf",
      triangleGltfNodes(
          R"([
            { "name": "GEO-tree", "mesh": 0 },
            { "name": "Needle", "extras": { "instance_asset_id": "cccccccccccccccc" },
              "translation": [0, 1, 0] }
          ])",
          "tree", "[0, 1]"));
  writeTextFile(root / "SE-world.gltf", triangleGltfNodes(
      R"([
        { "name": "Tree", "extras": { "instance_asset_id": "dddddddddddddddd" } },
        { "name": "Tree", "extras": { "instance_asset_id": "dddddddddddddddd" } },
        { "name": "Ghost", "extras": { "instance_asset_id": "eeeeeeeeeeeeeeee" } }
      ])",
      "SE-world", "[0, 1, 2]"));
  writeTextFile(
      root / "asset_index.json",
      "{\n  \"assets\": {\n"
      "    \"cccccccccccccccc\": { \"name\": \"leaf\", \"filepath\": \"assets/lib/leaf.gltf\" },\n"
      "    \"dddddddddddddddd\": { \"name\": \"tree\", \"filepath\": \"assets/lib/tree.gltf\" },\n"
      "    \"eeeeeeeeeeeeeeee\": { \"name\": \"ghost\", \"filepath\": \"assets/lib/missing.gltf\" }\n"
      "  }\n}\n");

  SeWorldFlattenOptions options;
  options.layout_gltf = root / "SE-world.gltf";
  options.asset_index_json = root / "asset_index.json";
  options.godot_root = root;
  Scene scene;
  const SeWorldFlattenStats stats = bakeSeWorldFlattenScene(options, scene);
  expect_true("1.3 bake ok", stats.success);
  expect_true("1.3 two layout", stats.layout_instances == 2);
  expect_true("1.3 two nested", stats.nested_instances == 2);
  expect_true("1.3 skip missing file", stats.skipped_missing_file == 1);
  const SceneEntityDefinition* needle = findEntity(scene, "Needle");
  const SceneEntityDefinition* needle_1 = findEntity(scene, "Needle_1");
  expect_true("1.3 needles exist", needle != nullptr && needle_1 != nullptr);
  expect_true("1.3 nested share guid on instances",
              needle != nullptr && needle_1 != nullptr &&
                  needle->mesh_virtual_path == needle_1->mesh_virtual_path &&
                  !needle->mesh_virtual_path.empty());
  const SceneEntityDefinition* tree = findEntity(scene, "Tree");
  expect_true("1.3 nested parent is layout",
              needle != nullptr && tree != nullptr && needle->parent_name == tree->name);
  expect_true("1.3 Tree instance has mesh",
              tree != nullptr && !tree->mesh_virtual_path.empty());
  expect_true("1.3 library GEO not exploded",
              findEntity(scene, "GEO-leaf") == nullptr &&
                  findEntity(scene, "GEO-tree") == nullptr);
  fs::remove_all(root);
}

void testMetresAndNegativeScale() {
  using namespace Blunder;
  const fs::path root = makeTempRoot("trs");
  writeTextFile(root / "assets" / "lib" / "box.gltf",
                triangleGltf("[{ \"name\": \"GEO-box\", \"mesh\": 0 }]", "box"));
  writeTextFile(root / "SE-world.gltf", triangleGltfNodes(
      R"([
        { "name": "Far", "extras": { "instance_asset_id": "ffffffffffffffff" },
          "translation": [70, 0, 0], "scale": [-1, 1, 1] }
      ])",
      "SE-world", "[0]"));
  writeTextFile(root / "asset_index.json",
                assetIndexTwo("ffffffffffffffff", "assets/lib/box.gltf", nullptr,
                              nullptr));

  SeWorldFlattenOptions options;
  options.layout_gltf = root / "SE-world.gltf";
  options.asset_index_json = root / "asset_index.json";
  options.godot_root = root;
  Scene scene;
  const SeWorldFlattenStats stats = bakeSeWorldFlattenScene(options, scene);
  expect_true("1.4 bake ok", stats.success);
  const SceneEntityDefinition* far = findEntity(scene, "Far");
  expect_true("1.4 entity", far != nullptr);
  if (far != nullptr) {
    expect_true("1.4 metres not 0.008", std::fabs(far->position.x) > 1.0f);
    expect_true("1.4 translation ~70", std::fabs(far->position.x - 70.0f) < 0.01f);
    expect_true("1.4 negative scale kept", far->scale.x < 0.0f);
  }
  fs::remove_all(root);
}

void testBakerOmitsColEntities() {
  using namespace Blunder;
  const fs::path root = makeTempRoot("colbake");
  writeTextFile(root / "assets" / "lib" / "box.gltf",
                triangleGltf("[{ \"name\": \"GEO-box\", \"mesh\": 0 }]", "box"));
  writeTextFile(root / "assets" / "sets" / "hub" / "ground.gltf",
                triangleGltfNodes(
                    R"([
                      { "name": "COL-ground", "mesh": 0 },
                      { "name": "GEO-ground", "mesh": 0, "translation": [1, 0, 0] },
                      { "name": "GEO-water", "mesh": 0, "translation": [0, 0, 2] }
                    ])",
                    "ground", "[0, 1, 2]"));
  writeTextFile(root / "SE-world.gltf", triangleGltfNodes(
      R"([
        { "name": "Ground", "extras": { "instance_asset_id": "1111111111111111" } },
        { "name": "COL-layout", "mesh": 0 },
        { "name": "Bush", "extras": { "instance_asset_id": "aaaaaaaaaaaaaaaa" } }
      ])",
      "SE-world", "[0, 1, 2]"));
  writeTextFile(
      root / "asset_index.json",
      "{\n  \"assets\": {\n"
      "    \"1111111111111111\": { \"name\": \"ground\", \"filepath\": "
      "\"assets/sets/hub/ground.gltf\" },\n"
      "    \"aaaaaaaaaaaaaaaa\": { \"name\": \"box\", \"filepath\": "
      "\"assets/lib/box.gltf\" }\n"
      "  }\n}\n");

  SeWorldFlattenOptions options;
  options.layout_gltf = root / "SE-world.gltf";
  options.asset_index_json = root / "asset_index.json";
  options.godot_root = root;
  Scene scene;
  const SeWorldFlattenStats stats = bakeSeWorldFlattenScene(options, scene);
  expect_true("2.1 bake ok", stats.success);
  expect_true("2.1 one layout", stats.layout_instances == 1);
  expect_true("2.1 Ground grouping exists", findEntity(scene, "Ground") != nullptr);
  expect_true("2.1 Bush layout exists", findEntity(scene, "Bush") != nullptr);
  expect_true("2.1 COL-ground not spawned", findEntity(scene, "COL-ground") == nullptr);
  expect_true("2.1 COL-layout not spawned", findEntity(scene, "COL-layout") == nullptr);
  const SceneEntityDefinition* geo_ground = findEntity(scene, "GEO-ground");
  const SceneEntityDefinition* geo_water = findEntity(scene, "GEO-water");
  expect_true("2.1 GEO-ground spawned", geo_ground != nullptr);
  expect_true("2.1 GEO-water spawned", geo_water != nullptr);
  expect_true("2.1 two set GEO refs", stats.set_geo_mesh_refs == 2);
  expect_true("2.1 Ground grouping has no mesh",
              findEntity(scene, "Ground") != nullptr &&
                  findEntity(scene, "Ground")->mesh_virtual_path.empty());
  expect_true("2.1 GEO-ground has mesh",
              geo_ground != nullptr && !geo_ground->mesh_virtual_path.empty());
  expect_true("2.1 GEO-water has mesh",
              geo_water != nullptr && !geo_water->mesh_virtual_path.empty());
  expect_true("2.1 GEO share set mesh guid",
              geo_ground != nullptr && geo_water != nullptr &&
                  geo_ground->mesh_virtual_path == geo_water->mesh_virtual_path);
  expect_true("2.1 GEO-ground parent Ground",
              geo_ground != nullptr && geo_ground->parent_name == "Ground");
  if (geo_ground != nullptr) {
    expect_true("2.1 GEO-ground x metres",
                std::fabs(geo_ground->position.x - 1.0f) < 1e-4f);
  }
  if (geo_water != nullptr) {
    expect_true("2.1 GEO-water engine Y from glTF Z",
                std::fabs(geo_water->position.y + 2.0f) < 1e-4f);
  }
  bool any_col = false;
  bool any_inactive = false;
  for (const SceneEntityDefinition& entity : scene.getEntities()) {
    if (entity.name.find("COL-") == 0) {
      any_col = true;
    }
    if (!entity.active) {
      any_inactive = true;
    }
  }
  expect_true("2.1 no COL-* entities", !any_col);
  expect_true("2.1 no active:false placeholders", !any_inactive);
  eastl::string json;
  expect_true("2.1 serialize", SceneSerializer::serialize(scene, json));
  expect_true("2.1 json has no active false",
              json.find("\"active\": false") == eastl::string::npos);
  fs::remove_all(root);
}

void testColSkipAndExtrasIgnoredOnImport() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempRoot("import");
  fs::create_directories(project / "Assets" / "Meshes");
  fs::create_directories(project / "Resources" / "Models");

  writeTextFile(project / "Resources" / "Models" / "set.gltf",
                triangleGltfNodes(
                    R"([
                      { "name": "COL-ground", "mesh": 0 },
                      { "name": "GEO-ground", "mesh": 0, "translation": [1, 0, 0] }
                    ])",
                    "set", "[0, 1]"));
  writeTextFile(project / "Assets" / "Meshes" / "set.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-111111111111\n"
                "source: resources/Models/set.gltf\n"
                "import:\n  materials: true\n  animations: false\n  scale: 1\n");

  writeTextFile(project / "Resources" / "Models" / "extras.gltf",
                triangleGltfNodes(
                    R"([
                      { "name": "GEO-box", "mesh": 0 },
                      { "name": "LI-bush",
                        "extras": { "instance_asset_id": "9c53197a476fa552" } }
                    ])",
                    "extras", "[0, 1]"));
  writeTextFile(project / "Assets" / "Meshes" / "extras.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-222222222222\n"
                "source: resources/Models/extras.gltf\n"
                "import:\n  materials: true\n  animations: false\n  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);
  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  SceneInstance col_scene;
  const EntityId col_parent =
      col_scene.createEntity("Set", Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
  const GltfSceneImporter::ImportResult col_result =
      GltfSceneImporter::importUnderEntity(
          &manager, eastl::string("assets/Meshes/set.mesh.yaml"), col_scene,
          col_parent);
  expect_true("2.1 attach GEO", col_result.success);
  expect_true("2.1 GEO entity spawned",
              isValid(col_scene.findEntityByName("GEO-ground")));
  const EntityId geo_prim = col_scene.findEntityByName("GEO-ground_prim0");
  expect_true("2.1 GEO primitive has MeshRenderer",
              isValid(geo_prim) && col_scene.getMeshRenderer(geo_prim) != nullptr);
  expect_true("2.1 COL entity not spawned",
              !isValid(col_scene.findEntityByName("COL-ground")));
  expect_true("2.1 at least one renderer", liveMeshRendererCount(col_scene) >= 1u);

  SceneInstance extras_scene;
  const EntityId extras_parent = extras_scene.createEntity(
      "Root", Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
  const GltfSceneImporter::ImportResult extras_result =
      GltfSceneImporter::importUnderEntity(
          &manager, eastl::string("assets/Meshes/extras.mesh.yaml"), extras_scene,
          extras_parent);
  expect_true("2.2 import extras does not fail", extras_result.success);
  expect_true("2.2 GEO kept", isValid(extras_scene.findEntityByName("GEO-box")));
  expect_true("2.2 instance extras not spawned",
              !isValid(extras_scene.findEntityByName("LI-bush")));

  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void testAttachMeshAssetsBindWithoutGraphImport() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempRoot("attachguid");
  fs::create_directories(project / "Assets" / "Meshes");
  fs::create_directories(project / "Resources" / "Models");

  writeTextFile(project / "Resources" / "Models" / "box.gltf",
                triangleGltf("[{ \"name\": \"GEO-box\", \"mesh\": 0 }]", "box"));
  writeTextFile(project / "Assets" / "Meshes" / "box.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-333333333333\n"
                "source: resources/Models/box.gltf\n"
                "import:\n  materials: true\n  animations: false\n  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);
  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  Scene scene;
  SceneEntityDefinition bush;
  bush.name = "Bush";
  bush.position = Vec3(1.0f, 0.0f, 0.0f);
  bush.mesh_virtual_path = "assets/Meshes/box.mesh.yaml";
  SceneEntityDefinition bush_1;
  bush_1.name = "Bush_1";
  bush_1.position = Vec3(2.0f, 0.0f, 0.0f);
  bush_1.mesh_virtual_path = "assets/Meshes/box.mesh.yaml";
  scene.getEntities().push_back(eastl::move(bush));
  scene.getEntities().push_back(eastl::move(bush_1));

  SceneInstance instance;
  instance.instantiate(scene);
  GltfSceneImporter::attachEntityMeshes(&manager, instance, scene);

  expect_true("4.2 entity count stays two", instance.getEntityCount() == 2u);
  expect_true("4.2 no glTF child explosion",
              !isValid(instance.findEntityByName("GEO-box")) &&
                  !isValid(instance.findEntityByName("GEO-box_prim0")));
  const EntityId a = instance.findEntityByName("Bush");
  const EntityId b = instance.findEntityByName("Bush_1");
  expect_true("4.2 Bush MeshRenderer",
              isValid(a) && instance.getMeshRenderer(a) != nullptr &&
                  instance.getMeshRenderer(a)->mesh);
  expect_true("4.2 Bush_1 MeshRenderer",
              isValid(b) && instance.getMeshRenderer(b) != nullptr &&
                  instance.getMeshRenderer(b)->mesh);
  expect_true("4.2 shared Mesh Asset pointer",
              isValid(a) && isValid(b) &&
                  instance.getMeshRenderer(a)->mesh ==
                      instance.getMeshRenderer(b)->mesh);
  expect_true("4.2 two live renderers", liveMeshRendererCount(instance) == 2u);

  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void testSeWorldOpenPath() {
  using namespace Blunder;
  expect_true(
      "4.1 se-world after env empty",
      resolveWindowedLiveScenePath("", "", "", "", "assets/Scenes/pick_test.scene.asset",
                                   "assets/Scenes/se-world.scene.asset") ==
          "assets/Scenes/se-world.scene.asset");
  expect_true("4.1 pick_test when se-world absent",
              resolveWindowedLiveScenePath("", "", "", "",
                                           "assets/Scenes/pick_test.scene.asset",
                                           "") == "assets/Scenes/pick_test.scene.asset");
  expect_true(
      "4.1 cli still wins",
      resolveWindowedLiveScenePath("assets/Scenes/cli.scene.asset", "", "", "",
                                   "assets/Scenes/pick_test.scene.asset",
                                   "assets/Scenes/se-world.scene.asset") ==
          "assets/Scenes/cli.scene.asset");

  const fs::path project = makeTempRoot("open");
  fs::create_directories(project / "Assets" / "Scenes");
  FileSystem missing;
  FileSystemInitInfo missing_init;
  missing_init.project_root = project;
  missing.initialize(missing_init);
  expect_true("4.1 missing file returns empty",
              projectSeWorldScenePathIfExists(missing).empty());
  writeTextFile(project / "Assets" / "Scenes" / "se-world.scene.asset", "{}\n");
  expect_true("4.1 existing file returns virtual path",
              projectSeWorldScenePathIfExists(missing) ==
                  k_se_world_scene_virtual_path);
  missing.shutdown();
  fs::remove_all(project);
}

void testDogWalkSeWorldOpenTiming() {
  using namespace Blunder;
  ensureLogger();

  fs::path product("E:/Blunder Projects/DogWalk");
  if (const char* env = std::getenv("BLUNDER_PRODUCT_ROOT");
      env != nullptr && env[0] != '\0') {
    product = env;
  }
  const fs::path scene_file =
      product / "Assets" / "Scenes" / "se-world.scene.asset";
  if (!fs::exists(scene_file)) {
    std::fprintf(stdout, "skip DogWalk se-world timing (missing %s)\n",
                 scene_file.string().c_str());
    return;
  }

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = product;
  file_system.initialize(fs_init);

  auto registry = eastl::make_shared<AssetRegistry>();
  registry->initialize(&file_system);
  g_runtime_global_context.m_asset_registry = registry;

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  SceneSystem scenes;
  scenes.initialize(SceneSystemInitInfo{&manager});

  const auto begin = std::chrono::steady_clock::now();
  const eastl::shared_ptr<SceneInstance> instance =
      scenes.loadScene(eastl::string("assets/Scenes/se-world.scene.asset"));
  const double open_ms = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
  std::fprintf(stdout,
               "DogWalk se-world open: %.1f ms entities=%zu renderers=%zu "
               "gltf_opens=%zu\n",
               open_ms, instance ? instance->getEntityCount() : 0u,
               instance ? liveMeshRendererCount(*instance) : 0u,
               manager.gltfDocumentOpenCount());

  expect_true("se-world loadScene returned", instance != nullptr);
  expect_true("se-world entity count",
              instance && instance->getEntityCount() >= 10000u);
  expect_true("se-world mesh bind not glTF reimport",
              instance && liveMeshRendererCount(*instance) >= 10000u);
  expect_true("se-world attach not per-instance glTF import",
              manager.gltfDocumentOpenCount() <= 400u);
  expect_true("se-world open under 10s", open_ms < 10000.0);

  size_t textured_renderers = 0;
  if (instance) {
    instance->forEachMeshRenderer(
        [&](EntityId, const MeshRendererComponent& renderer) {
          const eastl::shared_ptr<MaterialAsset> material =
              renderer.material
                  ? renderer.material
                  : (renderer.mesh ? renderer.mesh->getMaterialAsset() : nullptr);
          if (material && material->hasBaseColorTexture()) {
            ++textured_renderers;
          }
        });
  }
  std::fprintf(stdout, "DogWalk se-world textured renderers: %zu / %zu\n",
               textured_renderers,
               instance ? liveMeshRendererCount(*instance) : 0u);
  expect_true("se-world trees/ground have albedo after attach",
              instance &&
                  textured_renderers * 10u >
                      liveMeshRendererCount(*instance) * 9u);

  auto expectAlbedo = [&](const char* label, const char* virtual_path) {
    const eastl::shared_ptr<MeshAsset> mesh =
        manager.loadMesh(eastl::string(virtual_path));
    const bool ok = mesh && mesh->getMaterialAsset() &&
                    mesh->getMaterialAsset()->hasBaseColorTexture();
    expect_true(label, ok);
  };
  expectAlbedo("pine albedo",
               "assets/Meshes/se-world/LI-pine_tree_alpha.mesh.yaml");
  expectAlbedo("ground albedo",
               "assets/Meshes/se-world/SL-hub-ground.mesh.yaml");
  expectAlbedo("fence albedo",
               "assets/Meshes/se-world/PR-fence_gate.mesh.yaml");

  const size_t opens_after_bind = manager.gltfDocumentOpenCount();
  manager.invalidateMeshCache(
      eastl::string("assets/Meshes/se-world/LI-pine_tree_alpha.mesh.yaml"));
  const eastl::shared_ptr<MeshAsset> pine_reload = manager.loadMesh(
      eastl::string("assets/Meshes/se-world/LI-pine_tree_alpha.mesh.yaml"));
  expect_true("pine sidecar reload has albedo",
              pine_reload && pine_reload->getMaterialAsset() &&
                  pine_reload->getMaterialAsset()->hasBaseColorTexture());
  expect_true("pine sidecar reload skips glTF",
              manager.gltfDocumentOpenCount() == opens_after_bind);

  const size_t hydrated = manager.tickDeferredGltfMaterials(~0u);
  std::fprintf(stdout,
               "DogWalk se-world deferred materials leftover=%zu hydrated=%zu\n",
               manager.pendingGltfMaterialCount(), hydrated);
  expect_true("se-world hydrates during attach, not 2/frame",
              manager.pendingGltfMaterialCount() == 0u);
  expect_true("se-world deferred drain empty", hydrated == 0u);

  scenes.shutdown();
  manager.shutdown();
  g_runtime_global_context.m_asset_registry.reset();
  registry->shutdown();
  file_system.shutdown();
}

void testPromotePondIceFilmToOpaque() {
  using namespace Blunder;
  Asset::Meta tex_meta;
  tex_meta.virtual_path =
      "resources/se-world/assets/textures/ice_surface_squiggles-albedo.png";
  auto tex = eastl::make_shared<Texture2DAsset>(
      eastl::move(tex_meta), 1u, 1u, 4u, eastl::vector<uint8_t>{0, 0, 0, 0});
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "pond_water_surface";
  auto ice = eastl::make_shared<MaterialAsset>(
      eastl::move(mat_meta), glm::vec4(1.0f, 1.0f, 1.0f, 0.2f), AssetHandle{},
      tex, nullptr, nullptr, nullptr, glm::vec3(0.15f), glm::vec3(1.0f),
      glm::vec3(0.9f), 8.0f, 0.9f, 1.0f, cgltf_alpha_mode_blend, 0.5f, true,
      false);
  expect_true("ice film starts transparent", ice->usesForwardTransparentPass());
  ice->promoteWaterSurfaceFilmToOpaque();
  expect_true("ice film becomes opaque", !ice->usesForwardTransparentPass());
  expect_true("ice film alpha 1", ice->getBaseColorFactor().a >= 0.999f);
  expect_true("ice film dielectric", ice->getMetallicFactor() < 0.01f);

  Asset::Meta bubble_tex_meta;
  bubble_tex_meta.virtual_path =
      "resources/se-world/assets/textures/pond_underwater_bubbles.png";
  auto bubble_tex = eastl::make_shared<Texture2DAsset>(
      eastl::move(bubble_tex_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{0, 0, 0, 0});
  Asset::Meta bubble_meta;
  bubble_meta.virtual_path = "pond_underwater_bubbles";
  auto bubbles = eastl::make_shared<MaterialAsset>(
      eastl::move(bubble_meta), glm::vec4(0.68f, 0.77f, 1.0f, 0.2f),
      AssetHandle{}, bubble_tex, nullptr, nullptr, nullptr, glm::vec3(0.15f),
      glm::vec3(1.0f), glm::vec3(0.4f), 32.0f, 0.0f, 0.9f,
      cgltf_alpha_mode_blend, 0.5f, true, false);
  bubbles->promoteWaterSurfaceFilmToOpaque();
  expect_true("bubbles stay transparent",
              bubbles->usesForwardTransparentPass());
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();
  testBakerTwoInstancesUniqueNames();
  testSkipMissingId();
  testNestedLibraryAndMissingFile();
  testMetresAndNegativeScale();
  testBakerOmitsColEntities();
  testColSkipAndExtrasIgnoredOnImport();
  testAttachMeshAssetsBindWithoutGraphImport();
  testSeWorldOpenPath();
  testDogWalkSeWorldOpenTiming();
  testPromotePondIceFilmToOpaque();
  g_runtime_global_context.m_logger_system.reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "se_world_flatten_test: all passed\n");
  return 0;
}
