#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/editor/inspector_add_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/skeleton_from_gltf.h"
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
      ("blunder_skeleton_hydration_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
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

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();
  ObjectDB::clear();

  const fs::path project = makeTempProject();
  writeTextFile(project / "Resources" / "Models" / "skinned.gltf", kSkinnedGltf);
  writeTextFile(project / "Resources" / "Models" / "static.gltf", kStaticGltf);

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
