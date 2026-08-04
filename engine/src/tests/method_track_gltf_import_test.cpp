#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_registry/asset_registry.h"

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
      ("blunder_method_track_import_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Animations");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Animations");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

void writeMethodKeysAnimationGltf(const fs::path& gltf_path) {
  const std::string gltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "Hips", "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 0 },
      "indices": 1
    }]
  }],
  "animations": [{
    "name": "events",
    "extras": {
      "method_keys": [
        { "name": "Footstep", "time": 0.5, "args": [1.5] }
      ]
    },
    "channels": [{
      "sampler": 0,
      "target": { "node": 0, "path": "translation" }
    }],
    "samplers": [{
      "input": 2,
      "interpolation": "STEP",
      "output": 3
    }]
  }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [1.0, 1.0, 0.0],
      "min": [0.0, 0.0, 0.0]
    },
    {
      "bufferView": 1,
      "componentType": 5123,
      "count": 3,
      "type": "SCALAR"
    },
    {
      "bufferView": 2,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR",
      "max": [1.0],
      "min": [0.0]
    },
    {
      "bufferView": 3,
      "componentType": 5126,
      "count": 2,
      "type": "VEC3"
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 42, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 50, "byteLength": 24 }
  ],
  "buffers": [{
    "byteLength": 74,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPw=="
  }]
})";
  writeTextFile(gltf_path, gltf);
}

bool startsWith(const eastl::string& value, const char* prefix) {
  const size_t n = std::strlen(prefix);
  return value.size() >= n && value.compare(0, n, prefix) == 0;
}

void importMethodKeysFromGltfExtrasPreservesYaml() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external = project / "external" / "events_rig.gltf";
  writeMethodKeysAnimationGltf(external);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = true;
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("import succeeds", result.success);
  expect_true("clip extracted", result.animation_clips.size() == 1);
  if (result.animation_clips.empty()) {
    import_service.shutdown();
    registry.shutdown();
    file_system.shutdown();
    g_runtime_global_context.m_logger_system.reset();
    fs::remove_all(project);
    return;
  }

  const ImportResult& clip = result.animation_clips[0];
  eastl::string desc_rel = clip.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string desc_yaml;
  expect_true("read clip descriptor yaml",
              file_system.readText(descriptor_absolute, desc_yaml));

  eastl::string desc_source;
  const size_t source_pos = desc_yaml.find("source:");
  expect_true("descriptor has source", source_pos != eastl::string::npos);
  if (source_pos != eastl::string::npos) {
    size_t start = source_pos + 7;
    while (start < desc_yaml.size() && desc_yaml[start] == ' ') {
      ++start;
    }
    const size_t end = desc_yaml.find('\n', start);
    desc_source = desc_yaml.substr(start, end - start);
  }

  eastl::string intermediate_rel = desc_source;
  if (startsWith(intermediate_rel, "resources/")) {
    intermediate_rel.erase(0, 10);
  }
  const fs::path intermediate_absolute =
      file_system.resolveResource(fs::path(intermediate_rel.c_str()));
  eastl::string intermediate_yaml;
  expect_true("read intermediate yaml",
              file_system.readText(intermediate_absolute, intermediate_yaml));
  expect_true("intermediate contains method_keys",
              intermediate_yaml.find("method_keys") != eastl::string::npos);
  expect_true("intermediate contains Footstep",
              intermediate_yaml.find("Footstep") != eastl::string::npos);
  expect_true("intermediate contains arg 1.5",
              intermediate_yaml.find("1.5") != eastl::string::npos);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

}  // namespace

int main() {
  importMethodKeysFromGltfExtrasPreservesYaml();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
