#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/editor/inspector_add_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/scene/skeleton_from_gltf.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include "EASTL/shared_ptr.h"

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
      ("blunder_skeleton_hydration_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Resources" / "Models");
  return root;
}

// Triangle mesh plus a two-joint skin on the mesh node.
constexpr char kSkinnedGltf[] = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [
    { "name": "hips", "mesh": 0, "skin": 0, "children": [1] },
    { "name": "spine", "translation": [0, 0.5, 0] }
  ],
  "skins": [{ "joints": [0, 1] }],
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
}
)";

constexpr char kStaticGltf[] = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "static", "mesh": 0 }],
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
}
)";

constexpr char kLoadSceneJson[] = R"({
  "type": "Scene",
  "guid": "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff",
  "entities": [
    {
      "name": "ReloadDog",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "mesh": "resources/Models/skinned.gltf",
      "hasSkeleton": true,
      "animationPlayer": {
        "clips": { "idle": "00000000-0000-0000-0000-000000000001" }
      }
    },
    {
      "name": "GeoChild",
      "parent": "ReloadDog",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "hasSkeleton": true
    },
    {
      "name": "Cube",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "mesh": "resources/Models/static.gltf",
      "hasSkeleton": true
    }
  ]
}
)";

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();
  ObjectDB::clear();

  const fs::path project = makeTempProject();
  writeTextFile(project / "Resources" / "Models" / "skinned.gltf", kSkinnedGltf);
  writeTextFile(project / "Resources" / "Models" / "static.gltf", kStaticGltf);
  writeTextFile(project / "Assets" / "Scenes" / "hydrate_load.scene.asset",
                kLoadSceneJson);

  const char* kMeshGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee42";
  writeTextFile(project / "Assets" / "Meshes" / "skinned.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: resources/Models/skinned.gltf\n" +
                    "import:\n  materials: true\n  animations: false\n"
                    "  scale: 1\n");
  writeTextFile(project / "Assets" / "Scenes" / "hydrate_guid.scene.asset",
                std::string("{\n  \"type\": \"Scene\",\n"
                            "  \"guid\": \"cccccccc-dddd-4eee-8fff-000000000001\",\n"
                            "  \"entities\": [\n"
                            "    { \"name\": \"GuidDog\", \"position\": [0, 0, 0], "
                            "\"rotation\": [0, 0, 0], \"rotationMode\": \"euler_degrees\", "
                            "\"mesh\": \"") +
                    kMeshGuid +
                    "\", \"hasSkeleton\": true, "
                    "\"animationPlayer\": { \"clips\": { \"idle\": "
                    "\"00000000-0000-0000-0000-000000000001\" } } }\n"
                    "  ]\n}\n");

  FileSystem file_system;
  FileSystemInitInfo fs_info;
  fs_info.project_root = project;
  file_system.initialize(fs_info);

  AssetManager assets;
  AssetManagerInitInfo asset_info;
  asset_info.file_system = &file_system;
  assets.initialize(asset_info);

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Skinned", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.getEntity(id)->setMeshVirtualPath("resources/Models/skinned.gltf");
    const size_t entity_count_before = scene.getEntityCount();

    const InspectorUniqueAddResult result = applyInspectorUniqueAdd(
        &assets, scene, id, InspectorUniqueKind::AnimationPlayer);
    expect_true("skinned add created player", result.created_player);
    expect_true("skinned add created skeleton", result.created_skeleton);
    expect_true("add did not spawn glTF children",
                scene.getEntityCount() == entity_count_before);

    Object* object = scene.findBoundObject(id);
    expect_true("skinned bound object", object != nullptr);
    expect_true("skinned has skeleton", object != nullptr && object->hasSkeleton());
    const Skeleton* skeleton = object != nullptr ? object->getSkeleton() : nullptr;
    expect_true("skinned named hips",
                skeleton != nullptr && skeleton->findBoneIndex("hips") >= 0);
    expect_true("skinned named spine",
                skeleton != nullptr && skeleton->findBoneIndex("spine") >= 0);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Static", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.getEntity(id)->setMeshVirtualPath("resources/Models/static.gltf");

    const InspectorUniqueAddResult result =
        applyInspectorUniqueAdd(&assets, scene, id, InspectorUniqueKind::Skeleton);
    expect_true("static add succeeded", result.created_skeleton);
    Object* object = scene.findBoundObject(id);
    expect_true("static has skeleton", object != nullptr && object->hasSkeleton());
    expect_true("static skeleton empty",
                object != nullptr && object->getSkeleton() != nullptr &&
                    object->getSkeleton()->getBoneCount() == 0);
    expect_true("static no player",
                object != nullptr && !object->hasAnimationPlayer());
  }

  {
    Scene scene;
    SceneEntityDefinition dog;
    dog.name = "ReloadDog";
    dog.has_skeleton = true;
    dog.mesh_virtual_path = "resources/Models/skinned.gltf";
    dog.animation_player_clips.push_back(
        {"idle", "00000000-0000-0000-0000-000000000001"});
    SceneEntityDefinition geo;
    geo.name = "GeoChild";
    geo.parent_name = "ReloadDog";
    geo.has_skeleton = true;
    scene.getEntities().push_back(eastl::move(dog));
    scene.getEntities().push_back(eastl::move(geo));

    SceneInstance instance;
    instance.instantiate(scene);
    const EntityId id = instance.findEntityByName("ReloadDog");
    Object* object = instance.findBoundObject(id);
    expect_true("reload bound object", object != nullptr && object->hasSkeleton());
    expect_true(
        "reload empty before hydrate",
        object != nullptr && object->getSkeleton() != nullptr &&
            object->getSkeleton()->getBoneCount() == 0);
    expect_true("hydrate empty skeletons",
                hydrateEmptySkeletonsFromEntityMeshes(&assets, instance));
    const Skeleton* skeleton = object != nullptr ? object->getSkeleton() : nullptr;
    expect_true("reload named hips",
                skeleton != nullptr && skeleton->findBoneIndex("hips") >= 0);
    expect_true("reload named spine",
                skeleton != nullptr && skeleton->findBoneIndex("spine") >= 0);

    const EntityId child_id = instance.findEntityByName("GeoChild");
    Object* child = instance.findBoundObject(child_id);
    expect_true("child empty skeleton",
                child != nullptr && child->hasSkeleton() &&
                    child->getSkeleton()->getBoneCount() == 0);
    Skeleton* used = instance.findSkeletonForEntity(child_id);
    expect_true("child skins from parent bones",
                used != nullptr && used->findBoneIndex("hips") >= 0);
  }

  {
    SceneSystem scene_system;
    scene_system.initialize(SceneSystemInitInfo{&assets});
    const eastl::shared_ptr<SceneInstance> loaded = scene_system.loadScene(
        eastl::string("assets/Scenes/hydrate_load.scene.asset"));
    expect_true("loadScene returns instance", loaded != nullptr);
    if (loaded) {
      Object* dog = loaded->findBoundObject(loaded->findEntityByName("ReloadDog"));
      const Skeleton* dog_skeleton =
          dog != nullptr ? dog->getSkeleton() : nullptr;
      expect_true("loadScene named hips",
                  dog_skeleton != nullptr && dog_skeleton->findBoneIndex("hips") >= 0);
      expect_true(
          "loadScene named spine",
          dog_skeleton != nullptr && dog_skeleton->findBoneIndex("spine") >= 0);

      Object* child =
          loaded->findBoundObject(loaded->findEntityByName("GeoChild"));
      expect_true("loadScene child empty skeleton",
                  child != nullptr && child->hasSkeleton() &&
                      child->getSkeleton()->getBoneCount() == 0);
      Skeleton* used =
          loaded->findSkeletonForEntity(loaded->findEntityByName("GeoChild"));
      expect_true("loadScene child skins from parent",
                  used != nullptr && used->findBoneIndex("hips") >= 0);

      Object* cube = loaded->findBoundObject(loaded->findEntityByName("Cube"));
      expect_true("loadScene cube empty skeleton",
                  cube != nullptr && cube->hasSkeleton() &&
                      cube->getSkeleton() != nullptr &&
                      cube->getSkeleton()->getBoneCount() == 0);
      expect_true("loadScene cube has no player",
                  cube != nullptr && !cube->hasAnimationPlayer() &&
                      !cube->hasAnimationTree());
    }

    auto registry = eastl::make_shared<AssetRegistry>();
    registry->initialize(&file_system);
    expect_true("register skinned mesh guid",
                registry->registerAsset(eastl::string(kMeshGuid),
                                        eastl::string("assets/Meshes/skinned.mesh.yaml")));
    g_runtime_global_context.m_asset_registry = registry;

    const eastl::shared_ptr<SceneInstance> guid_loaded = scene_system.loadScene(
        eastl::string("assets/Scenes/hydrate_guid.scene.asset"));
    expect_true("guid loadScene returns instance", guid_loaded != nullptr);
    if (guid_loaded) {
      Object* dog =
          guid_loaded->findBoundObject(guid_loaded->findEntityByName("GuidDog"));
      const Skeleton* skeleton = dog != nullptr ? dog->getSkeleton() : nullptr;
      expect_true("guid loadScene named hips",
                  skeleton != nullptr && skeleton->findBoneIndex("hips") >= 0);
      expect_true("guid loadScene named spine",
                  skeleton != nullptr && skeleton->findBoneIndex("spine") >= 0);
    }

    scene_system.shutdown();
    g_runtime_global_context.m_asset_registry.reset();
    registry->shutdown();
  }

  assets.shutdown();
  file_system.shutdown();
  std::error_code error;
  fs::remove_all(project, error);
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d skeleton_hydration_test failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "skeleton_hydration_test: all passed\n");
  return 0;
}
