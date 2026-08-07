#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_import/companion_animation_gltf.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
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

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_asset_import_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Animations");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Animations");
  fs::create_directories(root / "Resources" / "Textures");
  fs::create_directories(root / "Resources" / "Source");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

void writeBinaryFile(const fs::path& path, const char* bytes, size_t size) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes, static_cast<std::streamsize>(size));
}

std::string readTextFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

bool startsWith(const eastl::string& value, const char* prefix) {
  const size_t n = std::strlen(prefix);
  return value.size() >= n && value.compare(0, n, prefix) == 0;
}

bool containsIgnoreCase(const eastl::string& value, const char* needle) {
  std::string lower(value.c_str());
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower.find(needle) != std::string::npos;
}

bool looksLikeGltfJson(const std::string& body) {
  return body.find("\"asset\"") != std::string::npos &&
         body.find("\"version\"") != std::string::npos;
}

bool looksLikeGlb(const std::string& body) {
  return body.size() >= 4 && body[0] == 'g' && body[1] == 'l' &&
         body[2] == 'T' && body[3] == 'F';
}

bool looksLikeGltfIntermediate(const std::string& body) {
  return looksLikeGltfJson(body) || looksLikeGlb(body);
}

fs::path normalizePathForCompare(const fs::path& path) {
  std::error_code ec;
  const fs::path absolute = fs::absolute(path, ec);
  if (ec) {
    return path.lexically_normal();
  }
  const fs::path canonical = fs::weakly_canonical(absolute, ec);
  return ec ? absolute.lexically_normal() : canonical;
}

bool pathSetsEqual(const std::vector<fs::path>& actual,
                   const std::vector<fs::path>& expected) {
  std::set<std::string> actual_set;
  std::set<std::string> expected_set;
  for (const fs::path& path : actual) {
    actual_set.insert(normalizePathForCompare(path).generic_string());
  }
  for (const fs::path& path : expected) {
    expected_set.insert(normalizePathForCompare(path).generic_string());
  }
  return actual_set == expected_set;
}

// Minimal glTF 2.0 triangle (Intermediate-direct import fixture).
constexpr const char* kTriangleGltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 0 },
      "indices": 1
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
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
  ],
  "buffers": [{
    "byteLength": 42,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"
  }]
})";

void writeDualAnimationGltfFixture(const fs::path& gltf_path) {
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
  "animations": [
    {
      "name": "idle",
      "channels": [{
        "sampler": 0,
        "target": { "node": 0, "path": "translation" }
      }],
      "samplers": [{
        "input": 2,
        "interpolation": "STEP",
        "output": 3
      }]
    },
    {
      "name": "walk",
      "channels": [{
        "sampler": 0,
        "target": { "node": 0, "path": "translation" }
      }],
      "samplers": [{
        "input": 4,
        "interpolation": "LINEAR",
        "output": 5
      }]
    }
  ],
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
    },
    {
      "bufferView": 4,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR",
      "max": [0.5],
      "min": [0.0]
    },
    {
      "bufferView": 5,
      "componentType": 5126,
      "count": 2,
      "type": "VEC3"
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 42, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 50, "byteLength": 24 },
    { "buffer": 0, "byteOffset": 74, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 82, "byteLength": 24 }
  ],
  "buffers": [{
    "byteLength": 106,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAA=="
  }]
})";
  writeTextFile(gltf_path, gltf);
}

void writeMethodKeysAnimationGltfFixture(const fs::path& gltf_path) {
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

// Skinned mesh with geometry and animations>=1 - full skinned character (reject as companion).
void writeSkinnedMeshWithGeometryGltfFixture(const fs::path& gltf_path) {
  const std::string gltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "Hips", "mesh": 0, "skin": 0 }],
  "skins": [{
    "joints": [0],
    "skeleton": 0
  }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 0 },
      "indices": 1
    }]
  }],
  "animations": [{
    "name": "idle",
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

// skins>=1, meshes>=1, animations=0 - Chocomel-shaped mesh host (no embedded anims).
void writeSkinnedMeshHostGltfFixture(const fs::path& gltf_path) {
  const std::string gltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "Hips", "mesh": 0, "skin": 0 }],
  "skins": [{
    "joints": [0],
    "skeleton": 0
  }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 0 },
      "indices": 1
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
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
  ],
  "buffers": [{
    "byteLength": 42,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"
  }]
})";
  writeTextFile(gltf_path, gltf);
}

// Task 1.1 (ADR 0021): Companion Animation glTF acceptance fixtures.
constexpr const char* kCompanionLoopGltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "Hips" }],
  "skins": [{
    "joints": [0]
  }],
  "animations": [{
    "name": "LOOP",
    "channels": [{
      "sampler": 0,
      "target": { "node": 0, "path": "translation" }
    }],
    "samplers": [{
      "input": 0,
      "interpolation": "LINEAR",
      "output": 1
    }]
  }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR"
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 2,
      "type": "VEC3"
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 24 }
  ],
  "buffers": [{
    "byteLength": 32,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
  }]
})";

constexpr const char* kExternalBufferCompanionLoopGltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "name": "Hips" }],
  "animations": [{
    "name": "LOOP",
    "channels": [{
      "sampler": 0,
      "target": { "node": 0, "path": "translation" }
    }],
    "samplers": [{
      "input": 0,
      "interpolation": "LINEAR",
      "output": 1
    }]
  }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR"
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 2,
      "type": "VEC3"
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 24 }
  ],
  "buffers": [{
    "byteLength": 32,
    "uri": "LOOP-idle.bin"
  }]
})";

constexpr const char* kCompanionAnimOnlyGltf = R"({
  "asset": { "version": "2.0" },
  "animations": [
    {
      "name": "walk_take_a",
      "channels": [{
        "sampler": 0,
        "target": { "node": 0, "path": "translation" }
      }],
      "samplers": [{
        "input": 0,
        "interpolation": "LINEAR",
        "output": 1
      }]
    },
    {
      "name": "walk_take_b",
      "channels": [{
        "sampler": 0,
        "target": { "node": 0, "path": "translation" }
      }],
      "samplers": [{
        "input": 0,
        "interpolation": "LINEAR",
        "output": 1
      }]
    }
  ],
  "nodes": [{ "name": "Tail" }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 2,
      "type": "SCALAR"
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 2,
      "type": "VEC3"
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 8 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 24 }
  ],
  "buffers": [{
    "byteLength": 32,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
  }]
})";

constexpr const char* kEmptyGltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [] }]
})";

void writeEmptyGltfStub(const fs::path& path) {
  writeTextFile(path, kEmptyGltf);
}

// Task 1.2 (ADR 0021): near-disk companion candidate enumeration.
void nearDiskCompanionGltfCandidateEnumeration() {
  using namespace Blunder;

  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_companion_near_disk_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));

  const fs::path mesh_path =
      root / "assets" / "char" / "chocomel" / "Chocomel.gltf";
  const fs::path co_located =
      root / "assets" / "char" / "chocomel" / "co_located.gltf";
  const fs::path sibling_idle =
      root / "assets" / "char" / "animations" / "LOOP-idle.gltf";
  const fs::path sibling_walk =
      root / "assets" / "char" / "animations" / "LOOP-walk.gltf";
  const fs::path nested_deep =
      root / "assets" / "char" / "animations" / "world" / "deep.gltf";
  const fs::path unrelated_far = root / "unrelated" / "far.gltf";

  writeEmptyGltfStub(mesh_path);
  writeEmptyGltfStub(co_located);
  writeEmptyGltfStub(sibling_idle);
  writeEmptyGltfStub(sibling_walk);
  writeEmptyGltfStub(nested_deep);
  writeEmptyGltfStub(unrelated_far);

  const std::vector<fs::path> candidates =
      enumerateNearDiskCompanionGltfCandidates(mesh_path);

  expect_true("near-disk finds mesh-dir and parent child-dir glTFs",
              pathSetsEqual(candidates,
                            {co_located, sibling_idle, sibling_walk}));
  expect_true("near-disk excludes mesh file itself",
              !pathSetsEqual(candidates, {mesh_path}));
  expect_true("near-disk does not recurse into nested child folders",
              !pathSetsEqual(candidates, {nested_deep}));
  expect_true("near-disk does not scan unrelated trees",
              !pathSetsEqual(candidates, {unrelated_far}));

  fs::remove_all(root);

  const fs::path pack_root =
      fs::temp_directory_path() /
      ("blunder_companion_near_disk_pack_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));

  const fs::path pack_mesh = pack_root / "pack" / "mesh.gltf";
  const fs::path pack_companion = pack_root / "pack" / "companion.gltf";
  const fs::path sibling_pack_other = pack_root / "sibling_pack" / "other.gltf";
  writeEmptyGltfStub(pack_mesh);
  writeEmptyGltfStub(pack_companion);
  writeEmptyGltfStub(sibling_pack_other);

  const std::vector<fs::path> pack_candidates =
      enumerateNearDiskCompanionGltfCandidates(pack_mesh);
  expect_true("near-disk mesh dir plus sibling child dirs",
              pathSetsEqual(pack_candidates,
                            {pack_companion, sibling_pack_other}));
  expect_true("near-disk excludes host mesh in pack layout",
              !pathSetsEqual(pack_candidates, {pack_mesh}));

  fs::remove_all(pack_root);
}

// Task 1.3 (ADR 0021): multi-select batch host/companion pairing.
void companionAnimationGltfMultiSelectBatchPairing() {
  using namespace Blunder;

  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_companion_batch_pair_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));

  const fs::path host_path = root / "Chocomel.gltf";
  const fs::path idle_path = root / "LOOP-idle.gltf";
  const fs::path walk_path = root / "LOOP-walk.gltf";
  const fs::path mesh_only_path = root / "triangle.gltf";

  writeSkinnedMeshHostGltfFixture(host_path);
  writeTextFile(idle_path, kCompanionLoopGltf);
  writeTextFile(walk_path, kCompanionAnimOnlyGltf);
  writeTextFile(mesh_only_path, kTriangleGltf);

  const CompanionGltfMultiSelectBatchPairingResult one_host =
      pairCompanionAnimationGltfMultiSelectBatch(
          {host_path, idle_path, walk_path, mesh_only_path});

  expect_true("one host: single pairing",
              one_host.host_pairings.size() == 1);
  expect_true("one host: correct host path",
              normalizePathForCompare(one_host.host_pairings[0].host_path) ==
                  normalizePathForCompare(host_path));
  expect_true("one host: companions attach",
              pathSetsEqual(one_host.host_pairings[0].companion_paths,
                            {idle_path, walk_path}));
  expect_true("one host: no orphan companions",
              one_host.orphan_companion_paths.empty());

  const fs::path host_a = root / "host_a.gltf";
  const fs::path host_b = root / "host_b.gltf";
  writeSkinnedMeshHostGltfFixture(host_a);
  writeSkinnedMeshHostGltfFixture(host_b);

  const CompanionGltfMultiSelectBatchPairingResult multi_host =
      pairCompanionAnimationGltfMultiSelectBatch(
          {host_a, host_b, idle_path, walk_path});

  expect_true("multi host: split into two pairings",
              multi_host.host_pairings.size() == 2);
  expect_true("multi host: each pairing has empty companions",
              multi_host.host_pairings[0].companion_paths.empty() &&
                  multi_host.host_pairings[1].companion_paths.empty());
  expect_true("multi host: companions become orphans",
              pathSetsEqual(multi_host.orphan_companion_paths,
                            {idle_path, walk_path}));

  const CompanionGltfMultiSelectBatchPairingResult orphans_only =
      pairCompanionAnimationGltfMultiSelectBatch({idle_path, walk_path});

  expect_true("orphans only: no invented host",
              orphans_only.host_pairings.empty());
  expect_true("orphans only: companions reported as orphans",
              pathSetsEqual(orphans_only.orphan_companion_paths,
                            {idle_path, walk_path}));

  const fs::path texture_path = root / "albedo.png";
  writeBinaryFile(texture_path, "PNG", 3);

  const CompanionGltfMultiSelectBatchPairingResult ignores_non_gltf =
      pairCompanionAnimationGltfMultiSelectBatch(
          {host_path, idle_path, texture_path});

  expect_true("non-glTF paths ignored for pairing",
              ignores_non_gltf.host_pairings.size() == 1);
  expect_true("non-glTF batch still pairs host + companion",
              pathSetsEqual(ignores_non_gltf.host_pairings[0].companion_paths,
                            {idle_path}));

  fs::remove_all(root);
}

// Tasks 2.1-2.3 (ADR 0021): batch pairing routes accepted companions through
// Mesh Import, persists them as Resources Intermediate bodies, and extracts
// companion clips using the companion file stem as the logical name.
void importExternalFilesPairsCompanionsIntoMeshImport() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external_root =
      fs::temp_directory_path() /
      ("blunder_companion_import_integration_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path host_path = external_root / "Chocomel.gltf";
  const fs::path idle_path = external_root / "LOOP-idle.gltf";
  const fs::path walk_path = external_root / "LOOP-walk.gltf";
  writeSkinnedMeshHostGltfFixture(host_path);
  writeTextFile(idle_path, kCompanionLoopGltf);
  writeTextFile(walk_path, kCompanionAnimOnlyGltf);

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

  const eastl::vector<eastl::string> paths = {
      eastl::string(host_path.generic_string().c_str()),
      eastl::string(idle_path.generic_string().c_str()),
      eastl::string(walk_path.generic_string().c_str())};

  MeshImportSettings enabled{};
  enabled.animations = true;
  const eastl::vector<ImportResult> enabled_results =
      import_service.importExternalFiles(paths, "assets/Meshes", enabled);

  expect_true("companion-aware batch imports one Mesh and three clips",
              enabled_results.size() == 4);
  expect_true("companion-aware batch returns successful host",
              !enabled_results.empty() && enabled_results[0].success);
  expect_true("companion-aware host merges all companion clips",
              !enabled_results.empty() &&
                  enabled_results[0].animation_clips.size() == 3);
  expect_true("single-animation companion prefers file stem",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-idle.animation.yaml"));
  expect_true("multi-animation companion uses file stem",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-walk.animation.yaml"));
  expect_true("multi-animation companion disambiguates with suffix",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-walk_1.animation.yaml"));
  const fs::path idle_intermediate =
      project / "Resources" / "Animations" / "LOOP-idle" /
      "LOOP-idle.gltf";
  const fs::path walk_intermediate =
      project / "Resources" / "Animations" / "LOOP-walk" /
      "LOOP-walk.gltf";
  expect_true("companion-aware mesh returns persisted Intermediate paths",
              !enabled_results.empty() &&
                  pathSetsEqual(enabled_results[0].companion_animation_paths,
                                {idle_intermediate, walk_intermediate}));
  expect_true("idle companion copied under Animations Intermediate",
              fs::exists(idle_intermediate) &&
                  readTextFile(idle_intermediate) == kCompanionLoopGltf);
  expect_true("walk companion copied under Animations Intermediate",
              fs::exists(walk_intermediate) &&
                  readTextFile(walk_intermediate) == kCompanionAnimOnlyGltf);

  eastl::string mesh_yaml;
  const fs::path mesh_descriptor_path =
      project / "Assets" / "Meshes" / "Chocomel.mesh.yaml";
  expect_true("companion-aware mesh descriptor is readable",
              file_system.readText(mesh_descriptor_path, mesh_yaml));
  MeshAssetDescriptor mesh_descriptor{};
  expect_true("companion-aware mesh descriptor parses",
              AssetYaml::parseMeshDescriptor(mesh_yaml, mesh_descriptor));
  expect_true("mesh descriptor has no companion packaging list",
              mesh_descriptor.companion_animation_sources.empty());
  expect_true(
      "mesh descriptor packaging list stays empty after idle import",
      mesh_descriptor.companion_animation_sources.empty());
  expect_true(
      "mesh descriptor packaging list stays empty after walk import",
      mesh_descriptor.companion_animation_sources.empty());
  expect_true("companion-only glTFs are not Mesh descriptors",
              !fs::exists(project / "Assets" / "Meshes" /
                          "LOOP-idle.mesh.yaml") &&
                  !fs::exists(project / "Assets" / "Meshes" /
                              "LOOP-walk.mesh.yaml"));

  eastl::string mismatch_descriptor_yaml;
  expect_true(
      "bone-mismatched companion clip descriptor remains registered",
      file_system.readText(project / "Assets" / "Animations" /
                               "LOOP-walk.animation.yaml",
                           mismatch_descriptor_yaml));
  AnimationClipAssetDescriptor mismatch_descriptor{};
  expect_true(
      "bone-mismatched companion clip descriptor parses",
      AssetYaml::parseAnimationClipDescriptor(mismatch_descriptor_yaml,
                                              mismatch_descriptor));
  eastl::string mismatch_source = mismatch_descriptor.source;
  if (startsWith(mismatch_source, "resources/")) {
    mismatch_source.erase(0, 10);
  }
  eastl::string mismatch_clip_yaml;
  expect_true(
      "bone-mismatched companion clip Intermediate is readable",
      file_system.readText(
          file_system.resolveResource(fs::path(mismatch_source.c_str())),
          mismatch_clip_yaml));
  AnimationClipData mismatch_clip{};
  expect_true("bone-mismatched companion clip data parses",
              AssetYaml::parseAnimationClipData(mismatch_clip_yaml,
                                                mismatch_clip));
  expect_true("bone-mismatched companion clip keeps Tail track",
              !mismatch_clip.tracks.empty() &&
                  mismatch_clip.tracks[0].bone == "Tail");

  const fs::path disabled_project = makeTempProject();
  FileSystem disabled_file_system;
  FileSystemInitInfo disabled_fs_init{};
  disabled_fs_init.project_root = disabled_project;
  disabled_file_system.initialize(disabled_fs_init);

  AssetRegistry disabled_registry;
  disabled_registry.initialize(&disabled_file_system);

  AssetImportService disabled_import_service;
  AssetImportServiceInit disabled_import_init{};
  disabled_import_init.file_system = &disabled_file_system;
  disabled_import_init.asset_registry = &disabled_registry;
  disabled_import_service.initialize(disabled_import_init);

  MeshImportSettings disabled{};
  disabled.animations = false;
  const eastl::vector<ImportResult> disabled_results =
      disabled_import_service.importExternalFiles(paths, "assets/Meshes",
                                                  disabled);

  expect_true("animations disabled still imports only host Mesh",
              disabled_results.size() == 1);
  expect_true("animations disabled does not attach companions",
              !disabled_results.empty() &&
                  disabled_results[0].companion_animation_paths.empty());
  expect_true("animations disabled skips companion Mesh registration",
              !fs::exists(disabled_project / "Assets" / "Meshes" /
                          "LOOP-idle.mesh.yaml") &&
                  !fs::exists(disabled_project / "Assets" / "Meshes" /
                              "LOOP-walk.mesh.yaml"));

  disabled_import_service.shutdown();
  disabled_registry.shutdown();
  disabled_file_system.shutdown();
  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(disabled_project);
  fs::remove_all(external_root);
}

void importExternalBufferCompanionPersistsSidecarAndExtractsClip() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external_root =
      fs::temp_directory_path() /
      ("blunder_companion_external_buffer_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path host_path = external_root / "Chocomel.gltf";
  const fs::path companion_path = external_root / "LOOP-idle.gltf";
  const fs::path companion_buffer_path = external_root / "LOOP-idle.bin";
  writeSkinnedMeshHostGltfFixture(host_path);
  writeTextFile(companion_path, kExternalBufferCompanionLoopGltf);
  constexpr char kCompanionBuffer[32] = {};
  writeBinaryFile(companion_buffer_path, kCompanionBuffer,
                  sizeof(kCompanionBuffer));

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
  const eastl::vector<eastl::string> paths = {
      eastl::string(host_path.generic_string().c_str()),
      eastl::string(companion_path.generic_string().c_str())};
  const eastl::vector<ImportResult> results =
      import_service.importExternalFiles(paths, "assets/Meshes", settings);

  const fs::path persisted_buffer =
      project / "Resources" / "Animations" / "LOOP-idle" /
      "LOOP-idle.bin";
  expect_true("external-buffer companion imports mesh plus clip",
              results.size() == 2);
  expect_true("external-buffer companion clip extracted after persistence",
              !results.empty() && results[0].animation_clips.size() == 1);
  expect_true("external-buffer companion sidecar persisted beside glTF",
              fs::exists(persisted_buffer));
  expect_true("external-buffer companion sidecar body preserved",
              fs::exists(persisted_buffer) &&
                  fs::file_size(persisted_buffer) ==
                      sizeof(kCompanionBuffer));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external_root);
}

// Tasks 5.1/5.2 Import gate: exercise the real DogWalk Chocomel source pack
// when it is available locally. Interactive Content Browser and Edit/Play
// checks remain in the human checklist.
void importRealDogWalkChocomelSources() {
  using namespace Blunder;

  const char* game_root_override = std::getenv("BLUNDER_DOGWALK_GAME_ROOT");
  const fs::path game_root =
      game_root_override != nullptr && game_root_override[0] != '\0'
          ? fs::path(game_root_override)
          : fs::path("E:/Godot Projects/dogwalk-repo/pro/game");
  const fs::path host =
      game_root / "assets" / "char" / "chocomel" / "Chocomel.gltf";
  const fs::path host_bin = host.parent_path() / "Chocomel.bin";
  const fs::path host_normal =
      host.parent_path() / "textures" / "chocomel_normal.png";
  const fs::path host_albedo =
      host.parent_path() / "textures" / "chocomel-albedo.png";
  const fs::path idle = game_root / "animations" / "world" /
                        "LOOP-chocomel-idle" / "LOOP-chocomel-idle.gltf";
  const fs::path idle_bin =
      idle.parent_path() / "LOOP-chocomel-idle.bin";
  const fs::path walk = game_root / "animations" / "world" /
                        "LOOP-chocomel-walk" / "LOOP-chocomel-walk.gltf";
  const fs::path walk_bin =
      walk.parent_path() / "LOOP-chocomel-walk.bin";

  const std::vector<fs::path> required = {
      host,        host_bin, host_normal, host_albedo,
      idle,        idle_bin, walk,        walk_bin};
  for (const fs::path& path : required) {
    if (!fs::is_regular_file(path)) {
      std::fprintf(stdout,
                   "SKIP real Chocomel Import: missing %s "
                   "(set BLUNDER_DOGWALK_GAME_ROOT)\n",
                   path.generic_string().c_str());
      return;
    }
  }

  std::fprintf(stdout, "RUN real Chocomel Import from %s\n",
               game_root.generic_string().c_str());
  ensureLogger();
  MeshImportSettings settings{};
  settings.animations = true;

  // A: the disconnected DogWalk files attach only through multi-select.
  const fs::path batch_project = makeTempProject();
  FileSystem batch_file_system;
  FileSystemInitInfo batch_fs_init{};
  batch_fs_init.project_root = batch_project;
  batch_file_system.initialize(batch_fs_init);
  AssetRegistry batch_registry;
  batch_registry.initialize(&batch_file_system);
  AssetImportService batch_import_service;
  AssetImportServiceInit batch_import_init{};
  batch_import_init.file_system = &batch_file_system;
  batch_import_init.asset_registry = &batch_registry;
  batch_import_service.initialize(batch_import_init);

  const eastl::vector<eastl::string> batch_paths = {
      eastl::string(host.generic_string().c_str()),
      eastl::string(idle.generic_string().c_str()),
      eastl::string(walk.generic_string().c_str())};
  const eastl::vector<ImportResult> batch_results =
      batch_import_service.importExternalFiles(batch_paths, "assets/Meshes",
                                               settings);

  expect_true("real Chocomel batch returns successful host Mesh",
              !batch_results.empty() && batch_results[0].success);
  size_t mesh_descriptor_count = 0;
  for (const auto& entry :
       fs::recursive_directory_iterator(batch_project / "Assets")) {
    if (entry.is_regular_file() &&
        entry.path().filename().generic_string().find(".mesh.yaml") !=
            std::string::npos) {
      ++mesh_descriptor_count;
    }
  }
  expect_true("real Chocomel batch creates exactly one Mesh descriptor",
              mesh_descriptor_count == 1);
  expect_true("real Chocomel batch creates no LOOP Mesh descriptors",
              !fs::exists(batch_project / "Assets" / "Meshes" /
                          "LOOP-chocomel-idle.mesh.yaml") &&
                  !fs::exists(batch_project / "Assets" / "Meshes" /
                              "LOOP-chocomel-walk.mesh.yaml"));
  expect_true("real Chocomel host Mesh persists its bin sidecar",
              fs::exists(batch_project / "Resources" / "Models" /
                         "Chocomel" / host_bin.filename()));
  expect_true("real Chocomel batch registers idle clip by companion stem",
              fs::exists(batch_project / "Assets" / "Animations" /
                         "LOOP-chocomel-idle.animation.yaml"));
  expect_true("real Chocomel batch registers walk clip by companion stem",
              fs::exists(batch_project / "Assets" / "Animations" /
                         "LOOP-chocomel-walk.animation.yaml"));
  const auto expect_registered_clip =
      [&](const fs::path& descriptor_path, const char* read_label,
          const char* parse_label, const char* registry_label) {
        eastl::string yaml;
        expect_true(read_label,
                    batch_file_system.readText(descriptor_path, yaml));
        AnimationClipAssetDescriptor descriptor{};
        const bool parsed =
            AssetYaml::parseAnimationClipDescriptor(yaml, descriptor);
        expect_true(parse_label, parsed);
        if (parsed) {
          eastl::string relative(
              fs::relative(descriptor_path, batch_project / "Assets")
                  .generic_string()
                  .c_str());
          eastl::string virtual_path("assets/");
          virtual_path.append(relative);
          expect_true(registry_label,
                      batch_registry.resolveGuid(descriptor.guid) ==
                          virtual_path);
        }
      };
  expect_registered_clip(
      batch_project / "Assets" / "Animations" /
          "LOOP-chocomel-idle.animation.yaml",
      "real Chocomel idle clip descriptor is readable",
      "real Chocomel idle clip descriptor parses",
      "real Chocomel idle clip is registered by GUID");
  expect_registered_clip(
      batch_project / "Assets" / "Animations" /
          "LOOP-chocomel-walk.animation.yaml",
      "real Chocomel walk clip descriptor is readable",
      "real Chocomel walk clip descriptor parses",
      "real Chocomel walk clip is registered by GUID");

  const fs::path idle_anim_dir =
      batch_project / "Resources" / "Animations" / idle.stem().generic_string();
  const fs::path walk_anim_dir =
      batch_project / "Resources" / "Animations" / walk.stem().generic_string();
  expect_true("real Chocomel batch persists idle glTF and bin under Animations",
              fs::exists(idle_anim_dir / idle.filename()) &&
                  fs::exists(idle_anim_dir / idle_bin.filename()) &&
                  fs::file_size(idle_anim_dir / idle_bin.filename()) ==
                      fs::file_size(idle_bin));
  expect_true("real Chocomel batch persists walk glTF and bin under Animations",
              fs::exists(walk_anim_dir / walk.filename()) &&
                  fs::exists(walk_anim_dir / walk_bin.filename()) &&
                  fs::file_size(walk_anim_dir / walk_bin.filename()) ==
                      fs::file_size(walk_bin));

  eastl::string batch_mesh_yaml;
  expect_true(
      "real Chocomel batch Mesh descriptor is readable",
      batch_file_system.readText(batch_project / "Assets" / "Meshes" /
                                     "Chocomel.mesh.yaml",
                                 batch_mesh_yaml));
  MeshAssetDescriptor batch_mesh_descriptor{};
  expect_true("real Chocomel batch Mesh descriptor parses",
              AssetYaml::parseMeshDescriptor(batch_mesh_yaml,
                                             batch_mesh_descriptor));
  expect_true("real Chocomel batch writes no companion packaging list",
              batch_mesh_descriptor.companion_animation_sources.empty());

  // B: importing only the disconnected host must not discover world animations.
  const fs::path disconnected_project = makeTempProject();
  FileSystem disconnected_file_system;
  FileSystemInitInfo disconnected_fs_init{};
  disconnected_fs_init.project_root = disconnected_project;
  disconnected_file_system.initialize(disconnected_fs_init);
  AssetRegistry disconnected_registry;
  disconnected_registry.initialize(&disconnected_file_system);
  AssetImportService disconnected_import_service;
  AssetImportServiceInit disconnected_import_init{};
  disconnected_import_init.file_system = &disconnected_file_system;
  disconnected_import_init.asset_registry = &disconnected_registry;
  disconnected_import_service.initialize(disconnected_import_init);

  const eastl::vector<eastl::string> host_only = {
      eastl::string(host.generic_string().c_str())};
  const eastl::vector<ImportResult> disconnected_results =
      disconnected_import_service.importExternalFiles(
          host_only, "assets/Meshes", settings);
  expect_true("real disconnected Chocomel host Import succeeds",
              !disconnected_results.empty() &&
                  disconnected_results[0].success);
  expect_true("real disconnected Chocomel host attaches no companions",
              !disconnected_results.empty() &&
                  disconnected_results[0].companion_animation_paths.empty());
  expect_true("real disconnected Chocomel host registers no LOOP clips",
              !fs::exists(disconnected_project / "Assets" / "Animations" /
                          "LOOP-chocomel-idle.animation.yaml") &&
                  !fs::exists(disconnected_project / "Assets" / "Animations" /
                              "LOOP-chocomel-walk.animation.yaml"));

  eastl::string disconnected_mesh_yaml;
  expect_true(
      "real disconnected Chocomel Mesh descriptor is readable",
      disconnected_file_system.readText(
          disconnected_project / "Assets" / "Meshes" / "Chocomel.mesh.yaml",
          disconnected_mesh_yaml));
  MeshAssetDescriptor disconnected_mesh_descriptor{};
  expect_true("real disconnected Chocomel Mesh descriptor parses",
              AssetYaml::parseMeshDescriptor(disconnected_mesh_yaml,
                                             disconnected_mesh_descriptor));
  expect_true("real disconnected Chocomel descriptor has no companions",
              disconnected_mesh_descriptor.companion_animation_sources.empty());

  // C: copy the same real files into the supported near-disk layout and verify
  // that a single host selection discovers the two sibling child folders.
  const fs::path co_located_root =
      fs::temp_directory_path() /
      ("blunder_real_chocomel_near_disk_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path co_located_host =
      co_located_root / "chocomel" / host.filename();
  const fs::path co_located_idle =
      co_located_root / "idle" / idle.filename();
  const fs::path co_located_walk =
      co_located_root / "walk" / walk.filename();
  fs::create_directories(co_located_host.parent_path());
  fs::create_directories(co_located_idle.parent_path());
  fs::create_directories(co_located_walk.parent_path());
  fs::copy_file(host, co_located_host);
  fs::copy_file(host_bin, co_located_host.parent_path() / host_bin.filename());
  fs::create_directories(co_located_host.parent_path() / "textures");
  fs::copy_file(host_normal, co_located_host.parent_path() / "textures" /
                                    host_normal.filename());
  fs::copy_file(host_albedo, co_located_host.parent_path() / "textures" /
                                    host_albedo.filename());
  fs::copy_file(idle, co_located_idle);
  fs::copy_file(idle_bin, co_located_idle.parent_path() / idle_bin.filename());
  fs::copy_file(walk, co_located_walk);
  fs::copy_file(walk_bin, co_located_walk.parent_path() / walk_bin.filename());

  const fs::path co_located_project = makeTempProject();
  FileSystem co_located_file_system;
  FileSystemInitInfo co_located_fs_init{};
  co_located_fs_init.project_root = co_located_project;
  co_located_file_system.initialize(co_located_fs_init);
  AssetRegistry co_located_registry;
  co_located_registry.initialize(&co_located_file_system);
  AssetImportService co_located_import_service;
  AssetImportServiceInit co_located_import_init{};
  co_located_import_init.file_system = &co_located_file_system;
  co_located_import_init.asset_registry = &co_located_registry;
  co_located_import_service.initialize(co_located_import_init);

  const eastl::vector<eastl::string> co_located_host_only = {
      eastl::string(co_located_host.generic_string().c_str())};
  const eastl::vector<ImportResult> co_located_results =
      co_located_import_service.importExternalFiles(
          co_located_host_only, "assets/Meshes", settings);
  expect_true("real co-located Chocomel host Import succeeds",
              !co_located_results.empty() && co_located_results[0].success);
  expect_true("real co-located Chocomel host attaches two companions",
              !co_located_results.empty() &&
                  co_located_results[0].companion_animation_paths.size() == 2);
  expect_true("real co-located Chocomel registers both LOOP clips",
              fs::exists(co_located_project / "Assets" / "Animations" /
                         "LOOP-chocomel-idle.animation.yaml") &&
                  fs::exists(co_located_project / "Assets" / "Animations" /
                             "LOOP-chocomel-walk.animation.yaml"));
  eastl::string co_located_mesh_yaml;
  expect_true(
      "real co-located Chocomel Mesh descriptor is readable",
      co_located_file_system.readText(
          co_located_project / "Assets" / "Meshes" / "Chocomel.mesh.yaml",
          co_located_mesh_yaml));
  MeshAssetDescriptor co_located_mesh_descriptor{};
  expect_true("real co-located Chocomel Mesh descriptor parses",
              AssetYaml::parseMeshDescriptor(co_located_mesh_yaml,
                                             co_located_mesh_descriptor));
  expect_true("real co-located Chocomel descriptor has no packaging list",
              co_located_mesh_descriptor.companion_animation_sources.empty());

  co_located_import_service.shutdown();
  co_located_registry.shutdown();
  co_located_file_system.shutdown();
  disconnected_import_service.shutdown();
  disconnected_registry.shutdown();
  disconnected_file_system.shutdown();
  batch_import_service.shutdown();
  batch_registry.shutdown();
  batch_file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(co_located_project);
  fs::remove_all(co_located_root);
  fs::remove_all(disconnected_project);
  fs::remove_all(batch_project);
}

void multiHostBatchDoesNotRediscoverOrphanCompanions() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external_root =
      fs::temp_directory_path() /
      ("blunder_companion_multi_host_import_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path host_a = external_root / "host_a.gltf";
  const fs::path host_b = external_root / "host_b.gltf";
  const fs::path orphan = external_root / "LOOP-idle.gltf";
  writeSkinnedMeshHostGltfFixture(host_a);
  writeSkinnedMeshHostGltfFixture(host_b);
  writeTextFile(orphan, kCompanionLoopGltf);

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
  const eastl::vector<eastl::string> paths = {
      eastl::string(host_a.generic_string().c_str()),
      eastl::string(host_b.generic_string().c_str()),
      eastl::string(orphan.generic_string().c_str())};
  const eastl::vector<ImportResult> results =
      import_service.importExternalFiles(paths, "assets/Meshes", settings);

  size_t host_count = 0;
  size_t orphan_clip_count = 0;
  for (const ImportResult& result : results) {
    if (result.success &&
        result.descriptor_virtual_path.find(".mesh.yaml") !=
            eastl::string::npos) {
      ++host_count;
      expect_true("multi-host batch leaves orphan companion unattached",
                  result.companion_animation_paths.empty() &&
                      result.animation_clips.empty());
    } else if (result.success &&
               result.descriptor_virtual_path.find(".animation.yaml") !=
                   eastl::string::npos) {
      ++orphan_clip_count;
    }
  }
  expect_true("multi-host batch imports exactly two Mesh hosts",
              host_count == 2);
  expect_true("multi-host orphan companion Imports as standalone clip",
              orphan_clip_count == 1 &&
                  fs::exists(project / "Assets" / "Meshes" /
                             "LOOP-idle.animation.yaml"));
  expect_true("orphan companion does not invent a Mesh descriptor",
              !fs::exists(project / "Assets" / "Meshes" /
                          "LOOP-idle.mesh.yaml"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external_root);
}

void standaloneCompanionOnlyImportRegistersClips() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external_root =
      fs::temp_directory_path() /
      ("blunder_standalone_companion_import_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path idle = external_root / "LOOP-chocomel-idle.gltf";
  const fs::path walk = external_root / "LOOP-chocomel-walk.gltf";
  writeTextFile(idle, kCompanionLoopGltf);
  writeTextFile(walk, kCompanionAnimOnlyGltf);

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
  const eastl::vector<eastl::string> paths = {
      eastl::string(idle.generic_string().c_str()),
      eastl::string(walk.generic_string().c_str())};
  const eastl::vector<ImportResult> results =
      import_service.importExternalFiles(paths, "assets/Meshes", settings);

  expect_true("standalone companions return successful clip results",
              results.size() >= 2);
  expect_true("standalone idle clip descriptor exists in selected folder",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-chocomel-idle.animation.yaml"));
  expect_true("standalone walk clip descriptor exists in selected folder",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-chocomel-walk.animation.yaml"));
  expect_true("standalone companions create no Mesh descriptors",
              !fs::exists(project / "Assets" / "Meshes" /
                          "LOOP-chocomel-idle.mesh.yaml") &&
                  !fs::exists(project / "Assets" / "Meshes" /
                              "LOOP-chocomel-walk.mesh.yaml"));
  expect_true("standalone idle Intermediate under Animations",
              fs::exists(project / "Resources" / "Animations" /
                         "LOOP-chocomel-idle" / "LOOP-chocomel-idle.gltf"));
  expect_true("standalone walk Intermediate under Animations",
              fs::exists(project / "Resources" / "Animations" /
                         "LOOP-chocomel-walk" / "LOOP-chocomel-walk.gltf"));
  expect_true("standalone does not use _standalone_companions folder",
              !fs::exists(project / "Resources" / "Models" /
                          "_standalone_companions"));

  MeshImportSettings disabled{};
  disabled.animations = false;
  const fs::path disabled_project = makeTempProject();
  FileSystem disabled_fs;
  FileSystemInitInfo disabled_fs_init{};
  disabled_fs_init.project_root = disabled_project;
  disabled_fs.initialize(disabled_fs_init);
  AssetRegistry disabled_registry;
  disabled_registry.initialize(&disabled_fs);
  AssetImportService disabled_import;
  AssetImportServiceInit disabled_init{};
  disabled_init.file_system = &disabled_fs;
  disabled_init.asset_registry = &disabled_registry;
  disabled_import.initialize(disabled_init);
  const eastl::vector<ImportResult> disabled_results =
      disabled_import.importExternalFiles(paths, "assets/Meshes", disabled);
  expect_true("animations=false skips orphan companion clips",
              disabled_results.empty() &&
                  !fs::exists(disabled_project / "Assets" / "Meshes" /
                              "LOOP-chocomel-idle.animation.yaml"));

  disabled_import.shutdown();
  disabled_registry.shutdown();
  disabled_fs.shutdown();
  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(disabled_project);
  fs::remove_all(project);
  fs::remove_all(external_root);
}

void deleteAssetRemovesMeshWithoutDependents() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_delete_asset_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path host = external / "Solo.gltf";
  writeSkinnedMeshHostGltfFixture(host);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, nullptr, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = false;
  const eastl::vector<ImportResult> imported = import_service.importExternalFiles(
      {eastl::string(host.generic_string().c_str())}, "assets/Meshes", settings);
  expect_true("delete test imported mesh",
              !imported.empty() && imported[0].success);
  const eastl::string descriptor = imported[0].descriptor_virtual_path;
  const eastl::string guid = imported[0].guid;
  expect_true("delete test mesh descriptor on disk",
              fs::exists(project / "Assets" / "Meshes" / "Solo.mesh.yaml"));

  eastl::string error;
  expect_true("deleteAsset succeeds without dependents",
              import_service.deleteAsset(descriptor, &error));
  expect_true("deleteAsset clears error on success", error.empty());
  expect_true("descriptor file removed",
              !fs::exists(project / "Assets" / "Meshes" / "Solo.mesh.yaml"));
  expect_true("GUID unregistered", registry.resolveGuid(guid).empty());
  expect_true("Intermediate mesh body removed",
              !fs::exists(project / "Resources" / "Models" / "Solo" /
                          "Solo.gltf"));

  import_service.shutdown();
  compiler.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external);
}

void deleteAssetRemovesAnimationClipWithoutDependents() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_delete_clip_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path companion = external / "LOOP-delete-me.gltf";
  writeTextFile(companion, kCompanionLoopGltf);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, nullptr, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = true;
  const eastl::vector<ImportResult> imported = import_service.importExternalFiles(
      {eastl::string(companion.generic_string().c_str())}, "assets/Animations",
      settings);
  expect_true("delete-clip test imported clip",
              !imported.empty() && imported[0].success);
  const eastl::string descriptor = imported[0].descriptor_virtual_path;
  const eastl::string guid = imported[0].guid;
  expect_true("delete-clip descriptor ends with .animation.yaml",
              descriptor.size() >= 15 &&
                  descriptor.compare(descriptor.size() - 15, 15,
                                     ".animation.yaml") == 0);
  expect_true("delete-clip descriptor on disk",
              fs::exists(project / "Assets" / "Animations" /
                         "LOOP-delete-me.animation.yaml"));

  eastl::string error;
  expect_true("deleteAsset removes animation clip",
              import_service.deleteAsset(descriptor, &error));
  expect_true("delete-clip clears error", error.empty());
  expect_true("clip descriptor removed",
              !fs::exists(project / "Assets" / "Animations" /
                          "LOOP-delete-me.animation.yaml"));
  expect_true("clip GUID unregistered", registry.resolveGuid(guid).empty());

  import_service.shutdown();
  compiler.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external);
}

void deleteAssetDetachesAnimationPlayerClipThenDeletes() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_delete_player_clip_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path companion = external / "LOOP-player-map.gltf";
  writeTextFile(companion, kCompanionLoopGltf);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, nullptr, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = true;
  const eastl::vector<ImportResult> imported = import_service.importExternalFiles(
      {eastl::string(companion.generic_string().c_str())}, "assets/Animations",
      settings);
  expect_true("player-map delete imported clip",
              !imported.empty() && imported[0].success);
  const eastl::string clip_descriptor = imported[0].descriptor_virtual_path;
  const eastl::string clip_guid = imported[0].guid;

  Scene scene;
  SceneEntityDefinition entity{};
  entity.name = "Animated";
  SceneEntityDefinition::AnimationClipBinding binding{};
  binding.name = "idle";
  binding.guid = clip_guid;
  entity.animation_player_clips.push_back(binding);
  entity.animation_player_slot0 = "idle";
  scene.getEntities().push_back(entity);

  const eastl::string scene_guid = registry.allocateGuid();
  scene.setGuid(scene_guid);
  eastl::string scene_json;
  expect_true("player-map serialize scene",
              SceneSerializer::serialize(scene, scene_json, &registry));
  const fs::path scene_absolute =
      project / "Assets" / "Scenes" / "uses_clip.scene.asset";
  fs::create_directories(scene_absolute.parent_path());
  writeTextFile(scene_absolute, scene_json.c_str());
  expect_true("player-map write scene", fs::exists(scene_absolute));
  registry.registerAsset(scene_guid, "assets/Scenes/uses_clip.scene.asset");

  eastl::string error;
  expect_true("deleteAsset detaches animationPlayer clip then deletes",
              import_service.deleteAsset(clip_descriptor, &error));
  expect_true("player-map delete clears error", error.empty());
  expect_true("clip descriptor removed after player-map detach",
              !fs::exists(project / "Assets" / "Animations" /
                          "LOOP-player-map.animation.yaml"));

  eastl::string updated_scene;
  expect_true("scene still on disk after clip detach",
              file_system.readText(scene_absolute, updated_scene));
  Scene reloaded;
  expect_true("reload scene after clip detach",
              SceneSerializer::deserialize(updated_scene, reloaded, &registry));
  expect_true("animationPlayer clip binding removed",
              !reloaded.getEntities().empty() &&
                  reloaded.getEntities()[0].animation_player_clips.empty());
  expect_true("animation_clip_guids cleared after player-map detach",
              !reloaded.getEntities().empty() &&
                  reloaded.getEntities()[0].animation_clip_guids.empty());
  expect_true("slot0 cleared when binding removed",
              !reloaded.getEntities().empty() &&
                  reloaded.getEntities()[0].animation_player_slot0.empty());

  import_service.shutdown();
  compiler.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external);
}

void deleteAssetRemovesSceneWithoutDependents() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
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

  Scene scene;
  scene.setName("Temp");
  const eastl::string scene_guid = registry.allocateGuid();
  scene.setGuid(scene_guid);
  eastl::string scene_json;
  expect_true("delete-scene serialize",
              SceneSerializer::serialize(scene, scene_json, &registry));
  const fs::path scene_absolute =
      project / "Assets" / "Scenes" / "temp_delete.scene.asset";
  fs::create_directories(scene_absolute.parent_path());
  writeTextFile(scene_absolute, scene_json.c_str());
  registry.registerAsset(scene_guid, "assets/Scenes/temp_delete.scene.asset");

  eastl::string error;
  expect_true("deleteAsset removes scene",
              import_service.deleteAsset("assets/Scenes/temp_delete.scene.asset",
                                         &error));
  expect_true("delete-scene clears error", error.empty());
  expect_true("scene file removed", !fs::exists(scene_absolute));
  expect_true("scene GUID unregistered",
              registry.resolveGuid(scene_guid).empty());

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void deleteAssetDetachesSceneDependentsThenDeletes() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_delete_detach_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path host = external / "DetachHost.gltf";
  writeSkinnedMeshHostGltfFixture(host);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, nullptr, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = false;
  const eastl::vector<ImportResult> imported = import_service.importExternalFiles(
      {eastl::string(host.generic_string().c_str())}, "assets/Meshes", settings);
  expect_true("detach-delete imported mesh",
              !imported.empty() && imported[0].success);
  const eastl::string mesh_descriptor = imported[0].descriptor_virtual_path;
  const eastl::string mesh_guid = imported[0].guid;

  Scene scene;
  SceneEntityDefinition entity{};
  entity.name = "Rig";
  entity.mesh_virtual_path = mesh_guid;
  scene.getEntities().push_back(entity);

  const eastl::string scene_guid = registry.allocateGuid();
  scene.setGuid(scene_guid);
  eastl::string scene_json;
  expect_true("detach-delete serialize scene",
              SceneSerializer::serialize(scene, scene_json, &registry));
  const fs::path scene_absolute =
      project / "Assets" / "Scenes" / "uses_mesh.scene.asset";
  fs::create_directories(scene_absolute.parent_path());
  writeTextFile(scene_absolute, scene_json.c_str());
  expect_true("detach-delete write scene", fs::exists(scene_absolute));
  registry.registerAsset(scene_guid, "assets/Scenes/uses_mesh.scene.asset");

  eastl::string error;
  expect_true("deleteAsset detaches scene then deletes mesh",
              import_service.deleteAsset(mesh_descriptor, &error));
  expect_true("detach-delete clears error", error.empty());
  expect_true("mesh descriptor removed after detach",
              !fs::exists(project / "Assets" / "Meshes" /
                          "DetachHost.mesh.yaml"));
  expect_true("mesh GUID unregistered after detach",
              registry.resolveGuid(mesh_guid).empty());

  eastl::string updated_scene;
  expect_true("scene still on disk after detach",
              file_system.readText(scene_absolute, updated_scene));
  Scene reloaded;
  expect_true("reload scene after detach",
              SceneSerializer::deserialize(updated_scene, reloaded, &registry));
  expect_true("scene entity mesh ref cleared",
              !reloaded.getEntities().empty() &&
                  reloaded.getEntities()[0].mesh_virtual_path.empty());

  import_service.shutdown();
  compiler.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external);
}

// Task 2.4 (ADR 0021): single-mesh Import discovers near-disk companions,
// filters acceptance, and routes through the same copy+extract path as
// multi-select. Disconnected deep trees remain out of scope.
void singleMeshImportDiscoversNearDiskCompanions() {
  using namespace Blunder;
  ensureLogger();

  const fs::path external_root =
      fs::temp_directory_path() /
      ("blunder_companion_near_disk_import_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));

  const fs::path host_path =
      external_root / "assets" / "char" / "chocomel" / "Chocomel.gltf";
  const fs::path idle_path =
      external_root / "assets" / "char" / "animations" / "LOOP-idle.gltf";
  const fs::path walk_path =
      external_root / "assets" / "char" / "animations" / "LOOP-walk.gltf";
  const fs::path deep_disconnected =
      external_root / "assets" / "char" / "animations" / "world" / "deep.gltf";
  const fs::path co_located_stub =
      external_root / "assets" / "char" / "chocomel" / "not_a_companion.gltf";

  writeSkinnedMeshHostGltfFixture(host_path);
  writeTextFile(idle_path, kCompanionLoopGltf);
  writeTextFile(walk_path, kCompanionAnimOnlyGltf);
  writeEmptyGltfStub(deep_disconnected);
  writeEmptyGltfStub(co_located_stub);

  const std::vector<fs::path> accepted =
      discoverAcceptedNearDiskCompanionAnimationGltfs(host_path);
  expect_true("near-disk filter accepts sibling child companions",
              pathSetsEqual(accepted, {idle_path, walk_path}));
  expect_true("near-disk filter rejects empty stub in mesh dir",
              !pathSetsEqual(accepted, {co_located_stub}));
  expect_true("near-disk filter excludes disconnected deep tree",
              !pathSetsEqual(accepted, {deep_disconnected}));

  const fs::path project = makeTempProject();
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

  const eastl::vector<eastl::string> single_host = {
      eastl::string(host_path.generic_string().c_str())};

  MeshImportSettings enabled{};
  enabled.animations = true;
  const eastl::vector<ImportResult> enabled_results =
      import_service.importExternalFiles(single_host, "assets/Meshes", enabled);

  expect_true("single-mesh near-disk imports one Mesh and three clips",
              enabled_results.size() == 4);
  expect_true("single-mesh near-disk returns successful host",
              !enabled_results.empty() && enabled_results[0].success);
  expect_true("single-mesh near-disk merges discovered companion clips",
              !enabled_results.empty() &&
                  enabled_results[0].animation_clips.size() == 3);
  expect_true("single-mesh near-disk registers idle clip by stem",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-idle.animation.yaml"));
  expect_true("single-mesh near-disk registers walk clip by stem",
              fs::exists(project / "Assets" / "Meshes" /
                         "LOOP-walk.animation.yaml"));

  const fs::path idle_intermediate =
      project / "Resources" / "Animations" / "LOOP-idle" /
      "LOOP-idle.gltf";
  const fs::path walk_intermediate =
      project / "Resources" / "Animations" / "LOOP-walk" /
      "LOOP-walk.gltf";
  expect_true("single-mesh near-disk persists companion Intermediates",
              !enabled_results.empty() &&
                  pathSetsEqual(enabled_results[0].companion_animation_paths,
                                {idle_intermediate, walk_intermediate}));
  expect_true("single-mesh near-disk copies idle companion body",
              fs::exists(idle_intermediate) &&
                  readTextFile(idle_intermediate) == kCompanionLoopGltf);
  expect_true("single-mesh near-disk copies walk companion body",
              fs::exists(walk_intermediate) &&
                  readTextFile(walk_intermediate) == kCompanionAnimOnlyGltf);
  expect_true("single-mesh near-disk does not register disconnected deep clip",
              !fs::exists(project / "Assets" / "Animations" /
                          "deep.animation.yaml"));

  eastl::string mesh_yaml;
  expect_true("single-mesh near-disk mesh descriptor is readable",
              file_system.readText(project / "Assets" / "Meshes" /
                                       "Chocomel.mesh.yaml",
                                   mesh_yaml));
  MeshAssetDescriptor mesh_descriptor{};
  expect_true("single-mesh near-disk mesh descriptor parses",
              AssetYaml::parseMeshDescriptor(mesh_yaml, mesh_descriptor));
  expect_true("single-mesh near-disk writes no companion packaging list",
              mesh_descriptor.companion_animation_sources.empty());

  const fs::path disabled_project = makeTempProject();
  FileSystem disabled_file_system;
  FileSystemInitInfo disabled_fs_init{};
  disabled_fs_init.project_root = disabled_project;
  disabled_file_system.initialize(disabled_fs_init);

  AssetRegistry disabled_registry;
  disabled_registry.initialize(&disabled_file_system);

  AssetImportService disabled_import_service;
  AssetImportServiceInit disabled_import_init{};
  disabled_import_init.file_system = &disabled_file_system;
  disabled_import_init.asset_registry = &disabled_registry;
  disabled_import_service.initialize(disabled_import_init);

  MeshImportSettings disabled{};
  disabled.animations = false;
  const eastl::vector<ImportResult> disabled_results =
      disabled_import_service.importExternalFiles(single_host, "assets/Meshes",
                                                  disabled);

  expect_true("single-mesh animations disabled imports only host Mesh",
              disabled_results.size() == 1);
  expect_true("single-mesh animations disabled skips near-disk companions",
              !disabled_results.empty() &&
                  disabled_results[0].companion_animation_paths.empty());

  disabled_import_service.shutdown();
  disabled_registry.shutdown();
  disabled_file_system.shutdown();
  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(disabled_project);
  fs::remove_all(external_root);
}

void companionAnimationGltfAcceptance() {
  using namespace Blunder;

  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_companion_accept_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));

  const fs::path loop_path = root / "loop.gltf";
  const fs::path anim_only_path = root / "idle.gltf";
  const fs::path mesh_with_anim_path = root / "mesh_anim.gltf";
  const fs::path skinned_mesh_path = root / "skinned_rig.gltf";
  const fs::path mesh_only_path = root / "triangle.gltf";
  const fs::path empty_path = root / "empty.gltf";

  writeTextFile(loop_path, kCompanionLoopGltf);
  writeTextFile(anim_only_path, kCompanionAnimOnlyGltf);
  writeDualAnimationGltfFixture(mesh_with_anim_path);
  writeSkinnedMeshWithGeometryGltfFixture(skinned_mesh_path);
  writeTextFile(mesh_only_path, kTriangleGltf);
  writeTextFile(empty_path, kEmptyGltf);

  expect_true("LOOP companion accepted (anim, meshes=0, skins=1)",
              isCompanionAnimationGltf(loop_path));
  expect_true("animations-only companion accepted (meshes=0)",
              isCompanionAnimationGltf(anim_only_path));
  expect_true("mesh-with-geometry and animations rejected (no skins)",
              !isCompanionAnimationGltf(mesh_with_anim_path));
  expect_true("skinned mesh-with-geometry rejected (skins>=1, meshes>=1, anims>=1)",
              !isCompanionAnimationGltf(skinned_mesh_path));
  expect_true("mesh without animations rejected",
              !isCompanionAnimationGltf(mesh_only_path));
  expect_true("empty glTF rejected",
              !isCompanionAnimationGltf(empty_path));
  expect_true("missing file rejected",
              !isCompanionAnimationGltf(root / "missing.gltf"));

  fs::remove_all(root);
}

// Task 2.2: multi-animation glTF Import registers mesh + clip Assets.
void importGltfWithTwoAnimationsRegistersMeshAndClips() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_dual_anim_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "rig.gltf";
  writeDualAnimationGltfFixture(external);

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

  expect_true("dual anim import mesh succeeds", result.success);
  expect_true("dual anim import returns mesh guid", !result.guid.empty());
  expect_true("dual anim import extracts two clips",
              result.animation_clips.size() == 2);
  expect_true("dual anim clip guids distinct",
              !result.animation_clips[0].guid.empty() &&
                  !result.animation_clips[1].guid.empty() &&
                  result.animation_clips[0].guid != result.animation_clips[1].guid);

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  bool saw_idle = false;
  bool saw_walk = false;
  for (const ImportResult& clip : result.animation_clips) {
    expect_true("clip import succeeds", clip.success);
    expect_true("clip descriptor under selected folder",
                startsWith(clip.descriptor_virtual_path, "assets/Meshes/"));
    expect_true("clip descriptor ends with .animation.yaml",
                clip.descriptor_virtual_path.find(".animation.yaml") !=
                    eastl::string::npos);

    eastl::string desc_rel = clip.descriptor_virtual_path;
    if (startsWith(desc_rel, "assets/")) {
      desc_rel.erase(0, 7);
    }
    const fs::path descriptor_absolute =
        file_system.resolveAsset(fs::path(desc_rel.c_str()));
    eastl::string descriptor_yaml;
    expect_true("read clip descriptor yaml",
                file_system.readText(descriptor_absolute, descriptor_yaml));

    AnimationClipAssetDescriptor clip_descriptor{};
    expect_true("parse clip descriptor",
                AssetYaml::parseAnimationClipDescriptor(descriptor_yaml,
                                                        clip_descriptor));
    expect_true("clip descriptor guid matches",
                clip_descriptor.guid == clip.guid);
    expect_true("clip source is Intermediate resources/ path",
                startsWith(clip_descriptor.source, "resources/Animations/"));
    expect_true("clip Intermediate ends with .anim.yaml",
                clip_descriptor.source.find(".anim.yaml") !=
                    eastl::string::npos);

    const fs::path intermediate_absolute =
        resolveResourcesVirtual(clip_descriptor.source);
    eastl::string intermediate_yaml;
    expect_true("read clip Intermediate yaml",
                file_system.readText(intermediate_absolute, intermediate_yaml));

    AnimationClipData clip_data{};
    expect_true("parse clip Intermediate yaml",
                AssetYaml::parseAnimationClipData(intermediate_yaml, clip_data));
    expect_true("clip has at least one track", !clip_data.tracks.empty());
    expect_true("clip track targets Hips bone",
                clip_data.tracks[0].bone == "Hips");
    expect_true("clip track is translation",
                clip_data.tracks[0].channel == AnimationChannel::Translation);

    if (clip_data.name == "idle") {
      saw_idle = true;
      expect_true("idle clip uses Constant interpolation",
                  clip_data.tracks[0].interpolation ==
                      AnimationInterpolation::Constant);
      expect_true("idle clip duration is 1.0",
                  clip_data.duration == 1.0f);
    } else if (clip_data.name == "walk") {
      saw_walk = true;
      expect_true("walk clip uses Linear interpolation",
                  clip_data.tracks[0].interpolation ==
                      AnimationInterpolation::Linear);
      expect_true("walk clip duration is 0.5",
                  clip_data.duration == 0.5f);
    }

    expect_true("registry maps clip guid",
                registry.resolveGuid(clip.guid) == clip.descriptor_virtual_path);
  }

  expect_true("dual anim import registered idle clip", saw_idle);
  expect_true("dual anim import registered walk clip", saw_walk);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Phase 5 task 2.2: glTF animation extras method_keys preserved in Intermediate YAML.
void importMethodKeysFromGltfExtrasPreservesYaml() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_method_keys_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "events_rig.gltf";
  writeMethodKeysAnimationGltfFixture(external);

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

  expect_true("method keys import mesh succeeds", result.success);
  expect_true("method keys clip extracted", result.animation_clips.size() == 1);
  if (result.animation_clips.empty()) {
    import_service.shutdown();
    registry.shutdown();
    file_system.shutdown();
    g_runtime_global_context.m_logger_system.reset();
    fs::remove_all(project);
    fs::remove_all(external.parent_path());
    return;
  }

  const ImportResult& clip = result.animation_clips[0];
  eastl::string desc_rel = clip.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string descriptor_yaml;
  expect_true("read method keys clip descriptor",
              file_system.readText(descriptor_absolute, descriptor_yaml));

  AnimationClipAssetDescriptor clip_descriptor{};
  expect_true("parse method keys clip descriptor",
              AssetYaml::parseAnimationClipDescriptor(descriptor_yaml,
                                                      clip_descriptor));

  eastl::string intermediate_rel = clip_descriptor.source;
  if (startsWith(intermediate_rel, "resources/")) {
    intermediate_rel.erase(0, 10);
  }
  const fs::path intermediate_absolute =
      file_system.resolveResource(fs::path(intermediate_rel.c_str()));
  eastl::string intermediate_yaml;
  expect_true("read method keys intermediate yaml",
              file_system.readText(intermediate_absolute, intermediate_yaml));
  expect_true("intermediate yaml contains method_keys",
              intermediate_yaml.find("method_keys") != eastl::string::npos);
  expect_true("intermediate yaml contains Footstep",
              intermediate_yaml.find("Footstep") != eastl::string::npos);

  AnimationClipData clip_data{};
  expect_true("parse method keys intermediate yaml",
              AssetYaml::parseAnimationClipData(intermediate_yaml, clip_data));
  expect_true("imported method_keys size", clip_data.method_keys.size() == 1);
  expect_true("imported method key name",
              clip_data.method_keys[0].name == "Footstep");
  expect_true("imported method key time", clip_data.method_keys[0].time == 0.5f);
  expect_true("imported method key arg",
              clip_data.method_keys[0].args.size() == 1 &&
                  clip_data.method_keys[0].args[0] == 1.5f);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 1.1 (ADR 0019): mesh Intermediate = glTF/GLB; COLLADA removed.
// FBX/OBJ are Source Export inputs. Images remain Intermediate-direct.
void meshExtensionRoutingTables() {
  using namespace Blunder;

  expect_true("gltf is mesh Intermediate extension",
              AssetImportService::isMeshIntermediateExtension(".gltf"));
  expect_true("glb is mesh Intermediate extension",
              AssetImportService::isMeshIntermediateExtension(".glb"));
  expect_true("dae is not mesh Intermediate extension",
              !AssetImportService::isMeshIntermediateExtension(".dae"));
  expect_true("gltf is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".gltf"));
  expect_true("glb is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".glb"));
  expect_true("obj is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".obj"));
  expect_true("fbx is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".fbx"));
  expect_true("blend is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".blend"));
  expect_true("blend is not mesh Intermediate extension",
              !AssetImportService::isMeshIntermediateExtension(".blend"));
  expect_true("png is texture Intermediate extension",
              AssetImportService::isTextureIntermediateExtension(".png"));
}

// Task 1.1 (ADR 0019): COLLADA `.dae` is not mesh Intermediate — Import rejects.
void importColladaMeshRejected() {
  using namespace Blunder;
  ensureLogger();

  expect_true("dae is not mesh Intermediate extension",
              !AssetImportService::isMeshIntermediateExtension(".dae"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_dae_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.dae";
  writeTextFile(external, "dae-not-intermediate");

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
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("dae mesh Import rejected", !result.success);
  expect_true("dae reject leaves guid empty", result.guid.empty());
  expect_true("dae reject writes no Assets descriptor",
              !fs::exists(project / "Assets" / "Meshes" / "cube.mesh.yaml"));
  expect_true("dae reject writes no Intermediate under Models",
              !fs::exists(project / "Resources" / "Models" / "cube.dae"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 5.1 / 1.1: Import registers Intermediate (not Source) — copy body under
// Resources (non-Source) + write Assets descriptor with Intermediate `source`.
// Mesh Intermediate-direct input is glTF/GLB (ADR 0019).
void importMeshWritesIntermediateAndDescriptor() {
  using namespace Blunder;
  ensureLogger();

  expect_true("gltf is mesh Intermediate extension",
              AssetImportService::isMeshIntermediateExtension(".gltf"));
  expect_true("png is texture Intermediate extension",
              AssetImportService::isTextureIntermediateExtension(".png"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_ext_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.gltf";
  writeTextFile(external, kTriangleGltf);

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
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("importMesh succeeds", result.success);
  expect_true("importMesh returns guid", !result.guid.empty());
  expect_true("descriptor virtual path under assets/",
              startsWith(result.descriptor_virtual_path, "assets/"));
  expect_true("descriptor ends with .mesh.yaml",
              result.descriptor_virtual_path.find(".mesh.yaml") !=
                  eastl::string::npos);

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  expect_true("Assets descriptor file exists",
              file_system.exists(descriptor_absolute));

  eastl::string yaml;
  expect_true("read descriptor yaml",
              file_system.readText(descriptor_absolute, yaml));

  MeshAssetDescriptor parsed{};
  expect_true("parse mesh descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("descriptor guid matches result", parsed.guid == result.guid);
  expect_true("descriptor source is Intermediate resources/ path",
              startsWith(parsed.source, "resources/"));
  expect_true("descriptor source is not Source archive",
              !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("archived_source empty for glTF Intermediate Import",
              parsed.archived_source.empty());

  eastl::string intermediate_rel = parsed.source;
  if (startsWith(intermediate_rel, "resources/")) {
    intermediate_rel.erase(0, 10);
  }
  const fs::path intermediate_absolute =
      file_system.resolveResource(fs::path(intermediate_rel.c_str()));
  expect_true("Intermediate body exists under Resources",
              file_system.exists(intermediate_absolute));
  expect_true("Intermediate body not under Resources/Source",
              intermediate_absolute.generic_string().find("/Source/") ==
                      std::string::npos &&
                  intermediate_absolute.generic_string().find("\\Source\\") ==
                      std::string::npos);
  expect_true("Intermediate body content copied",
              readTextFile(intermediate_absolute) == kTriangleGltf);
  expect_true("Intermediate body is glTF",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("registry maps guid to descriptor",
              registry.resolveGuid(result.guid) ==
                  result.descriptor_virtual_path);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Minimal Wavefront OBJ triangle (Source Export fixture).
constexpr const char* kTriangleObj = R"(# blunder Source Export fixture
o Triangle
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0
f 1 2 3
)";

// Task 1.2 (ADR 0019): Source Export dual-writes Source archive + Intermediate glTF.
void importObjSourceExportDualWritesArchiveAndIntermediate() {
  using namespace Blunder;
  ensureLogger();

  expect_true("obj is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".obj"));
  expect_true("fbx is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".fbx"));
  expect_true("gltf is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".gltf"));
  expect_true("glb is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".glb"));
  expect_true("blend is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".blend"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_obj_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.obj";
  writeTextFile(external, kTriangleObj);

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
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("OBJ Source Export succeeds", result.success);
  expect_true("OBJ Source Export returns guid", !result.guid.empty());
  expect_true("OBJ descriptor under assets/",
              startsWith(result.descriptor_virtual_path, "assets/"));
  expect_true("OBJ descriptor ends with .mesh.yaml",
              result.descriptor_virtual_path.find(".mesh.yaml") !=
                  eastl::string::npos);

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  expect_true("OBJ Assets descriptor exists",
              file_system.exists(descriptor_absolute));

  eastl::string yaml;
  expect_true("read OBJ descriptor yaml",
              file_system.readText(descriptor_absolute, yaml));

  MeshAssetDescriptor parsed{};
  expect_true("parse OBJ mesh descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("OBJ descriptor guid matches result", parsed.guid == result.guid);
  expect_true("OBJ source is Intermediate resources/ path",
              startsWith(parsed.source, "resources/"));
  expect_true("OBJ source is Intermediate glTF",
              containsIgnoreCase(parsed.source, ".gltf") ||
                  containsIgnoreCase(parsed.source, ".glb"));
  expect_true("OBJ source is not COLLADA .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("OBJ source is not under Source archive",
              !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("OBJ archived_source set", !parsed.archived_source.empty());
  expect_true("OBJ archived_source under Source",
              containsIgnoreCase(parsed.archived_source, "source/"));
  expect_true("OBJ archived_source keeps .obj",
              containsIgnoreCase(parsed.archived_source, ".obj"));

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path intermediate_absolute = resolveResourcesVirtual(parsed.source);
  expect_true("Intermediate glTF exists under Resources",
              file_system.exists(intermediate_absolute));
  expect_true("Intermediate glTF not under Resources/Source",
              intermediate_absolute.generic_string().find("/Source/") ==
                      std::string::npos &&
                  intermediate_absolute.generic_string().find("\\Source\\") ==
                      std::string::npos);
  expect_true("Intermediate lives under Models",
              containsIgnoreCase(eastl::string(intermediate_absolute.generic_string().c_str()),
                                 "/models/") ||
                  containsIgnoreCase(
                      eastl::string(intermediate_absolute.generic_string().c_str()),
                      "\\models\\"));
  const std::string intermediate_body = readTextFile(intermediate_absolute);
  expect_true("Intermediate body looks like glTF",
              looksLikeGltfIntermediate(intermediate_body));

  eastl::string archived_rel = parsed.archived_source;
  if (startsWith(archived_rel, "resources/")) {
    archived_rel.erase(0, 10);
  }
  const fs::path archived_absolute =
      file_system.resolveResource(fs::path(archived_rel.c_str()));
  expect_true("archived Source file exists under Resources/Source",
              file_system.exists(archived_absolute));
  expect_true("archived Source path contains Source",
              archived_absolute.generic_string().find("/Source/") !=
                      std::string::npos ||
                  archived_absolute.generic_string().find("\\Source\\") !=
                      std::string::npos);
  expect_true("archived Source content matches OBJ",
              readTextFile(archived_absolute) == kTriangleObj);

  expect_true("registry maps OBJ guid to descriptor",
              registry.resolveGuid(result.guid) ==
                  result.descriptor_virtual_path);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 1.1 (ADR 0019): glTF Import is Intermediate-direct, not Source Export.
void importGltfIntermediateDirectNotSourceExport() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_gltf_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.gltf";
  writeTextFile(external, kTriangleGltf);

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
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("glTF Intermediate Import succeeds", result.success);
  expect_true("glTF Intermediate Import returns guid", !result.guid.empty());

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("read glTF descriptor yaml",
              file_system.readText(descriptor_absolute, yaml));

  MeshAssetDescriptor parsed{};
  expect_true("parse glTF mesh descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("glTF source is Intermediate .gltf",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("glTF source is not COLLADA .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("glTF archived_source empty (no Source Export)",
              parsed.archived_source.empty());

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path intermediate_absolute = resolveResourcesVirtual(parsed.source);
  expect_true("glTF Intermediate file exists",
              file_system.exists(intermediate_absolute));
  expect_true("glTF Intermediate body preserved",
              readTextFile(intermediate_absolute) == kTriangleGltf);
  expect_true("glTF Import did not archive under Source",
              !fs::exists(project / "Resources" / "Source" / "Models"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Minimal Wavefront OBJ quad (distinct from triangle for Reimport refresh).
constexpr const char* kQuadObj = R"(# blunder Reimport fixture
o Quad
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 1.0 1.0 0.0
v 0.0 1.0 0.0
f 1 2 3
f 1 3 4
)";

// Task 5.3: Reimport from archived Source preserves GUID and refreshes Intermediate.
void reimportObjPreservesGuidAndRefreshesIntermediate() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_obj_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.obj";
  writeTextFile(external, kTriangleObj);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("Reimport fixture: OBJ import succeeds", imported.success);
  expect_true("Reimport fixture: GUID allocated", !imported.guid.empty());
  const eastl::string original_guid = imported.guid;

  eastl::string desc_rel = imported.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("Reimport fixture: read descriptor",
              file_system.readText(descriptor_absolute, yaml));
  MeshAssetDescriptor parsed{};
  expect_true("Reimport fixture: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path intermediate_absolute =
      resolveResourcesVirtual(parsed.source);
  const fs::path archived_absolute =
      resolveResourcesVirtual(parsed.archived_source);
  expect_true("Reimport fixture: Intermediate exists",
              file_system.exists(intermediate_absolute));
  expect_true("Reimport fixture: archived Source exists",
              file_system.exists(archived_absolute));

  // Stamp Intermediate so we can detect a real Assimp re-export overwrite.
  writeTextFile(intermediate_absolute, "STALE_INTERMEDIATE_MARKER");
  expect_true("Reimport fixture: Intermediate stamped stale",
              readTextFile(intermediate_absolute) ==
                  "STALE_INTERMEDIATE_MARKER");

  // Plant a cooked Final so Reimport must invalidate it.
  const fs::path mesh_cooked = cookedMeshPath(file_system, original_guid);
  const fs::path mesh_meta = cookedMeshMetaPath(file_system, original_guid);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");
  expect_true("Reimport fixture: planted cooked Final",
              file_system.exists(mesh_cooked));

  // Modify archived Source (simulates artist edit / watch trigger input).
  writeTextFile(archived_absolute, kQuadObj);

  expect_true("requestReimport succeeds",
              import_service.requestReimport(original_guid));

  expect_true("Reimport preserves GUID in registry",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);

  eastl::string yaml_after;
  expect_true("Reimport: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed_after{};
  expect_true("Reimport: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed_after));
  expect_true("Reimport preserves descriptor GUID",
              parsed_after.guid == original_guid);
  expect_true("Reimport keeps Intermediate source path",
              parsed_after.source == parsed.source);
  expect_true("Reimport keeps archived_source path",
              parsed_after.archived_source == parsed.archived_source);

  const std::string intermediate_after =
      readTextFile(intermediate_absolute);
  expect_true("Reimport refreshes Intermediate (not stale marker)",
              intermediate_after != "STALE_INTERMEDIATE_MARKER");
  expect_true("Reimport Intermediate still looks like glTF",
              looksLikeGltfIntermediate(intermediate_after));

  expect_true("Reimport invalidates cooked Final",
              !file_system.exists(mesh_cooked));
  expect_true("Reimport invalidates cooked meta",
              !file_system.exists(mesh_meta));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 5.3: Intermediate-only Reimport preserves GUID and invalidates Finals.
void reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_gltf_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.gltf";
  writeTextFile(external, kTriangleGltf);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("glTF Intermediate Reimport fixture import succeeds", imported.success);
  const eastl::string original_guid = imported.guid;

  const fs::path mesh_cooked = cookedMeshPath(file_system, original_guid);
  const fs::path mesh_meta = cookedMeshMetaPath(file_system, original_guid);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  expect_true("Intermediate-only requestReimport succeeds",
              import_service.requestReimport(original_guid));
  expect_true("Intermediate-only Reimport preserves GUID",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);
  expect_true("Intermediate-only Reimport invalidates Final",
              !file_system.exists(mesh_cooked));
  expect_true("Intermediate-only Reimport invalidates meta",
              !file_system.exists(mesh_meta));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

void importUnsupportedSourceExportRejected() {
  using namespace Blunder;
  ensureLogger();

  expect_true("blend is not Source Export whitelist",
              !AssetImportService::isMeshSourceExportExtension(".blend"));
  expect_true("blend is not Intermediate mesh extension",
              !AssetImportService::isMeshIntermediateExtension(".blend"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_blend_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.blend";
  writeTextFile(external, "not-a-real-blend");

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
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);
  // Task 5.4: .blend is not Assimp Source Export whitelist — clear failure,
  // not silent success or copy-to-Source-only.
  expect_true("blend Source Export rejected", !result.success);
  expect_true("blend reject leaves guid empty", result.guid.empty());
  expect_true("blend reject leaves descriptor path empty",
              result.descriptor_virtual_path.empty());
  expect_true("blend reject writes no Assets descriptor",
              !fs::exists(project / "Assets" / "Meshes" / "cube.mesh.yaml"));
  expect_true("blend reject writes no Source archive",
              !fs::exists(project / "Resources" / "Source" / "Models" /
                          "cube"));
  expect_true("blend reject writes no Intermediate under Models",
              !fs::exists(project / "Resources" / "Models" / "cube.dae") &&
                  !fs::exists(project / "Resources" / "Models" / "cube.gltf") &&
                  !fs::exists(project / "Resources" / "Models" / "cube.glb"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

void importTextureWritesIntermediateAndDescriptor() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_tex_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "albedo.png";
  writeTextFile(external, "png-intermediate-body");

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

  TextureImportSettings settings{};
  const ImportResult result =
      import_service.importTexture(external, "assets/Textures", settings);

  expect_true("importTexture succeeds", result.success);
  expect_true("texture descriptor under assets/",
              startsWith(result.descriptor_virtual_path, "assets/"));

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("read texture descriptor",
              file_system.readText(descriptor_absolute, yaml));

  TextureAssetDescriptor parsed{};
  expect_true("parse texture descriptor",
              AssetYaml::parseTextureDescriptor(yaml, parsed));
  expect_true("texture source is Intermediate resources/ path",
              startsWith(parsed.source, "resources/"));
  expect_true("texture source is not Source archive",
              !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("texture archived_source empty",
              parsed.archived_source.empty());

  eastl::string intermediate_rel = parsed.source;
  if (startsWith(intermediate_rel, "resources/")) {
    intermediate_rel.erase(0, 10);
  }
  const fs::path intermediate_absolute =
      file_system.resolveResource(fs::path(intermediate_rel.c_str()));
  expect_true("texture Intermediate body exists under Resources",
              file_system.exists(intermediate_absolute));
  expect_true("texture Intermediate body content copied",
              readTextFile(intermediate_absolute) == "png-intermediate-body");

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 1.2 (ADR 0019): registry scan must not convert glTF Intermediate → COLLADA.
void registryScanDoesNotUpgradeGltfToDae() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  fs::create_directories(project / "Resources" / "Models" / "legacy");
  fs::create_directories(project / "Resources" / "Source");

  const fs::path legacy_gltf =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  writeTextFile(legacy_gltf, kTriangleGltf);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.gltf\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  const uint32_t upgraded = import_service.scanAndUpgradeLegacyIntermediates();
  expect_true("registry scan reports zero glTF→dae upgrades", upgraded == 0);

  eastl::string yaml_after;
  expect_true("no-upgrade: read descriptor after scan",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("no-upgrade: parse descriptor after scan",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("no-upgrade preserves descriptor GUID", parsed.guid == kGuid);
  expect_true("no-upgrade keeps glTF source",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("no-upgrade source does not point at .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("no-upgrade does not invent archived_source",
              parsed.archived_source.empty());

  const fs::path sibling_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  expect_true("no-upgrade does not create sibling .dae",
              !file_system.exists(sibling_dae));
  expect_true("no-upgrade preserves legacy glTF Intermediate",
              file_system.exists(legacy_gltf));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.2: upgradeLegacyMeshIntermediates is a no-op (glTF stays glTF).
void upgradeLegacyMeshIntermediatesIsNoOp() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_gltf =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  writeTextFile(legacy_gltf, kTriangleGltf);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  const std::string descriptor_before =
      std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
      "source: resources/Models/legacy/hero.gltf\n" +
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1\n";
  writeTextFile(descriptor_absolute, descriptor_before);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  const uint32_t upgraded = import_service.upgradeLegacyMeshIntermediates();
  expect_true("upgradeLegacyMeshIntermediates returns zero", upgraded == 0);

  eastl::string yaml_after;
  expect_true("no-op upgrade: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  expect_true("no-op upgrade: descriptor body unchanged",
              std::string(yaml_after.c_str()) == descriptor_before);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Legacy migration: minimal COLLADA Intermediate (Assimp-readable).
constexpr char kMinimalTriangleDae[] = R"(<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset>
    <up_axis>Y_UP</up_axis>
  </asset>
  <library_geometries>
    <geometry id="tri-mesh" name="tri">
      <mesh>
        <source id="tri-positions">
          <float_array id="tri-positions-array" count="9">0 0 0 1 0 0 0 1 0</float_array>
          <technique_common>
            <accessor source="#tri-positions-array" count="3" stride="3">
              <param name="X" type="float"/>
              <param name="Y" type="float"/>
              <param name="Z" type="float"/>
            </accessor>
          </technique_common>
        </source>
        <vertices id="tri-vertices">
          <input semantic="POSITION" source="#tri-positions"/>
        </vertices>
        <triangles count="1">
          <input semantic="VERTEX" source="#tri-vertices" offset="0"/>
          <p>0 1 2</p>
        </triangles>
      </mesh>
    </geometry>
  </library_geometries>
  <library_visual_scenes>
    <visual_scene id="Scene" name="Scene">
      <node id="tri-node" name="tri">
        <instance_geometry url="#tri-mesh"/>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#Scene"/>
  </scene>
</COLLADA>
)";

// Task 1.4 (ADR 0019): legacy `.dae` Intermediate migrates GUID-preserving to glTF.
void upgradeLegacyDaeIntermediateToGltfPreservesGuid() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "dddddddd-eeee-4fff-8aaa-bbbbbbbbbbbb";

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.dae\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  const fs::path mesh_cooked = cookedMeshPath(file_system, kGuid);
  writeBinaryFile(mesh_cooked, "MESH", 4);

  const uint32_t migrated = import_service.upgradeLegacyMeshIntermediates();
  expect_true("dae migration reports one Asset", migrated == 1);

  eastl::string yaml_after;
  expect_true("dae migration: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("dae migration: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("dae migration preserves descriptor GUID", parsed.guid == kGuid);
  expect_true("dae migration rewrites source to glTF",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("dae migration source does not point at .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("dae migration archives former .dae under Source",
              !parsed.archived_source.empty());
  expect_true("dae migration archived_source under Source",
              containsIgnoreCase(parsed.archived_source, "source/"));
  expect_true("dae migration archived_source keeps .dae",
              containsIgnoreCase(parsed.archived_source, ".dae"));

  const fs::path gltf_path =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  expect_true("dae migration creates sibling glTF Intermediate",
              file_system.exists(gltf_path));
  expect_true("dae migration glTF body looks valid",
              looksLikeGltfIntermediate(readTextFile(gltf_path)));
  expect_true("dae migration removes legacy .dae Intermediate",
              !file_system.exists(legacy_dae));
  expect_true("dae migration invalidates cooked Final",
              !file_system.exists(mesh_cooked));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.4: registry scan path migrates legacy `.dae` Intermediate.
void registryScanUpgradesDaeToGltf() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "eeeeeeee-ffff-4aaa-8bbb-cccccccccccc";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.dae\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

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

  const uint32_t migrated = import_service.scanAndUpgradeLegacyIntermediates();
  expect_true("scan migrates legacy dae", migrated == 1);

  eastl::string yaml_after;
  expect_true("scan dae migration: read descriptor",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("scan dae migration: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("scan dae migration preserves GUID", parsed.guid == kGuid);
  expect_true("scan dae migration rewrites source to glTF",
              containsIgnoreCase(parsed.source, ".gltf"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.4: fail-soft migration leaves `.dae` source when convert fails.
void upgradeLegacyDaeFailSoftLeavesDaeSource() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "ffffff00-1111-4222-8333-444444444444";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path partial_gltf =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  writeTextFile(partial_gltf, "PARTIAL_GLTF_NOT_VALID");

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  const std::string descriptor_before =
      std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
      "source: resources/Models/legacy/hero.dae\n" +
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1\n";
  writeTextFile(descriptor_absolute, descriptor_before);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  AssetImportService::setForceUpgradeConvertFailureForTest(true);
  const uint32_t migrated = import_service.upgradeLegacyMeshIntermediates();
  AssetImportService::setForceUpgradeConvertFailureForTest(false);

  expect_true("fail-soft dae migration reports zero", migrated == 0);

  eastl::string yaml_after;
  expect_true("fail-soft dae: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  expect_true("fail-soft dae: descriptor body unchanged",
              std::string(yaml_after.c_str()) == descriptor_before);

  MeshAssetDescriptor parsed{};
  expect_true("fail-soft dae: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("fail-soft dae: source still .dae",
              containsIgnoreCase(parsed.source, ".dae"));
  expect_true("fail-soft dae: archived_source not invented",
              parsed.archived_source.empty());
  expect_true("fail-soft dae: cleans partial sibling glTF",
              !file_system.exists(partial_gltf));
  expect_true("fail-soft dae: legacy Intermediate preserved",
              file_system.exists(legacy_dae));

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string("assets/Meshes/hero.mesh.yaml"));
  expect_true("fail-soft dae: loadMesh legacy Fast Path returns mesh",
              mesh != nullptr);
  expect_true("fail-soft dae: loadMesh legacy Fast Path has vertices",
              mesh && mesh->getVertexCount() == 3);

  import_service.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.4: when archived OBJ exists, migration Reimports glTF from Source.
void upgradeLegacyDaeWithArchivedObjReimportsGltf() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "aaaa1111-bbbb-4222-8ccc-dddddddddddd";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");
  fs::create_directories(project / "Resources" / "Source" / "Models" / "hero");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path archived_obj =
      project / "Resources" / "Source" / "Models" / "hero" / "triangle.obj";
  writeTextFile(archived_obj, kTriangleObj);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.dae\n" +
                    "archived_source: resources/Source/Models/hero/triangle.obj\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  const uint32_t migrated = import_service.upgradeLegacyMeshIntermediates();
  expect_true("archived OBJ dae migration reports one", migrated == 1);

  eastl::string yaml_after;
  expect_true("archived OBJ migration: read descriptor",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("archived OBJ migration: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("archived OBJ migration preserves GUID", parsed.guid == kGuid);
  expect_true("archived OBJ migration rewrites source to glTF",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("archived OBJ migration keeps archived_source",
              parsed.archived_source ==
                  "resources/Source/Models/hero/triangle.obj");

  const fs::path gltf_path =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  expect_true("archived OBJ migration creates glTF Intermediate",
              file_system.exists(gltf_path));
  expect_true("archived OBJ migration glTF from OBJ reexport",
              looksLikeGltfIntermediate(readTextFile(gltf_path)));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.2: Reimport from archived OBJ regenerates Intermediate glTF.
void reimportFromArchivedObjRegeneratesGltfIntermediate() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_gltf_obj_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.obj";
  writeTextFile(external, kTriangleObj);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("gltf reimport fixture: OBJ import succeeds", imported.success);
  const eastl::string original_guid = imported.guid;

  eastl::string desc_rel = imported.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("gltf reimport fixture: read descriptor",
              file_system.readText(descriptor_absolute, yaml));
  MeshAssetDescriptor parsed{};
  expect_true("gltf reimport fixture: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("gltf reimport fixture: source is glTF",
              containsIgnoreCase(parsed.source, ".gltf") ||
                  containsIgnoreCase(parsed.source, ".glb"));

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path intermediate_absolute =
      resolveResourcesVirtual(parsed.source);
  const fs::path archived_absolute =
      resolveResourcesVirtual(parsed.archived_source);
  expect_true("gltf reimport fixture: Intermediate exists",
              file_system.exists(intermediate_absolute));
  expect_true("gltf reimport fixture: archived Source exists",
              file_system.exists(archived_absolute));

  writeTextFile(intermediate_absolute, "STALE_GLTF_MARKER");
  expect_true("gltf reimport fixture: Intermediate stamped stale",
              readTextFile(intermediate_absolute) == "STALE_GLTF_MARKER");

  const fs::path mesh_cooked = cookedMeshPath(file_system, original_guid);
  const fs::path mesh_meta = cookedMeshMetaPath(file_system, original_guid);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  writeTextFile(archived_absolute, kQuadObj);

  expect_true("gltf reimport: requestReimport succeeds",
              import_service.requestReimport(original_guid));

  expect_true("gltf reimport preserves GUID",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);

  eastl::string yaml_after;
  expect_true("gltf reimport: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed_after{};
  expect_true("gltf reimport: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed_after));
  expect_true("gltf reimport keeps Intermediate source path",
              parsed_after.source == parsed.source);

  const std::string intermediate_after = readTextFile(intermediate_absolute);
  expect_true("gltf reimport refreshes Intermediate (not stale marker)",
              intermediate_after != "STALE_GLTF_MARKER");
  expect_true("gltf reimport regenerated body is glTF",
              looksLikeGltfIntermediate(intermediate_after));
  expect_true("gltf reimport invalidates cooked Final",
              !file_system.exists(mesh_cooked));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Tasks 3.1-3.2 (ADR 0021): Mesh Reimport uses persisted companion
// Intermediates to refresh derived clips without changing their Asset GUIDs.
void reimportCompanionClipPreservesGuidAndRefreshesYaml() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external_root =
      fs::temp_directory_path() /
      ("blunder_reimport_companion_anim_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  const fs::path host_path = external_root / "Chocomel.gltf";
  const fs::path companion_path = external_root / "LOOP-idle.gltf";
  writeSkinnedMeshHostGltfFixture(host_path);
  writeTextFile(companion_path, kCompanionLoopGltf);

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
  const eastl::vector<eastl::string> paths = {
      eastl::string(host_path.generic_string().c_str()),
      eastl::string(companion_path.generic_string().c_str())};
  const eastl::vector<ImportResult> imported =
      import_service.importExternalFiles(paths, "assets/Meshes", settings);

  expect_true("companion reimport fixture: mesh plus clip imported",
              imported.size() == 2);
  expect_true("companion reimport fixture: host has one clip",
              !imported.empty() && imported[0].animation_clips.size() == 1);
  if (imported.empty() || imported[0].animation_clips.empty()) {
    import_service.shutdown();
    registry.shutdown();
    file_system.shutdown();
    g_runtime_global_context.m_logger_system.reset();
    fs::remove_all(project);
    fs::remove_all(external_root);
    return;
  }
  const eastl::string mesh_guid = imported[0].guid;
  const ImportResult companion_clip = imported[0].animation_clips[0];
  expect_true("companion reimport fixture: clip GUID captured",
              !companion_clip.guid.empty());

  eastl::string mesh_yaml;
  expect_true("companion reimport fixture: read mesh descriptor",
              file_system.readText(
                  project / "Assets" / "Meshes" / "Chocomel.mesh.yaml",
                  mesh_yaml));
  MeshAssetDescriptor mesh_descriptor{};
  expect_true("companion reimport fixture: parse mesh descriptor",
              AssetYaml::parseMeshDescriptor(mesh_yaml, mesh_descriptor));
  expect_true("companion reimport fixture: no Mesh packaging list",
              mesh_descriptor.companion_animation_sources.empty());

  eastl::string clip_desc_rel = companion_clip.descriptor_virtual_path;
  if (startsWith(clip_desc_rel, "assets/")) {
    clip_desc_rel.erase(0, 7);
  }
  const fs::path clip_descriptor_absolute =
      file_system.resolveAsset(fs::path(clip_desc_rel.c_str()));
  eastl::string clip_descriptor_yaml;
  expect_true("companion reimport fixture: read clip descriptor",
              file_system.readText(clip_descriptor_absolute,
                                   clip_descriptor_yaml));
  AnimationClipAssetDescriptor clip_descriptor{};
  expect_true("companion reimport fixture: parse clip descriptor",
              AssetYaml::parseAnimationClipDescriptor(clip_descriptor_yaml,
                                                      clip_descriptor));

  eastl::string clip_source_rel = clip_descriptor.source;
  if (startsWith(clip_source_rel, "resources/")) {
    clip_source_rel.erase(0, 10);
  }
  const fs::path clip_intermediate_absolute =
      file_system.resolveResource(fs::path(clip_source_rel.c_str()));
  writeTextFile(clip_intermediate_absolute, "STALE_COMPANION_CLIP_MARKER");

  // ADR 0028: Mesh Reimport must not refresh companion Clip YAML.
  fs::remove_all(external_root);
  expect_true("mesh reimport request succeeds",
              import_service.requestReimport(mesh_guid));
  expect_true("mesh reimport leaves companion clip GUID mapped",
              registry.resolveGuid(companion_clip.guid) ==
                  companion_clip.descriptor_virtual_path);
  expect_true("mesh reimport does not refresh companion clip YAML",
              readTextFile(clip_intermediate_absolute) ==
                  "STALE_COMPANION_CLIP_MARKER");

  expect_true("clip reimport request succeeds",
              import_service.requestReimport(companion_clip.guid));
  const std::string clip_yaml_after =
      readTextFile(clip_intermediate_absolute);
  expect_true("clip reimport refreshes stale YAML from Animations glTF",
              clip_yaml_after != "STALE_COMPANION_CLIP_MARKER");
  AnimationClipData clip_data_after{};
  expect_true("clip reimport writes valid clip YAML",
              AssetYaml::parseAnimationClipData(
                  eastl::string(clip_yaml_after.c_str()), clip_data_after));
  expect_true("clip reimport keeps companion stem identity",
              clip_data_after.name == "LOOP-idle");

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 2.3: mesh Reimport refreshes clip YAML while preserving clip GUIDs.
void reimportPreservesAnimationClipGuidsAndRefreshesYaml() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_dual_anim_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "rig.gltf";
  writeDualAnimationGltfFixture(external);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, nullptr, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = true;
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("clip reimport fixture: mesh import succeeds", imported.success);
  expect_true("clip reimport fixture: two clips extracted",
              imported.animation_clips.size() == 2);

  const eastl::string mesh_guid = imported.guid;
  const eastl::string idle_guid = imported.animation_clips[0].guid;
  const eastl::string walk_guid = imported.animation_clips[1].guid;
  expect_true("clip reimport fixture: clip guids captured",
              !idle_guid.empty() && !walk_guid.empty() &&
                  idle_guid != walk_guid);

  eastl::string mesh_desc_rel = imported.descriptor_virtual_path;
  if (startsWith(mesh_desc_rel, "assets/")) {
    mesh_desc_rel.erase(0, 7);
  }
  const fs::path mesh_descriptor_absolute =
      file_system.resolveAsset(fs::path(mesh_desc_rel.c_str()));
  eastl::string mesh_yaml;
  expect_true("clip reimport fixture: read mesh descriptor",
              file_system.readText(mesh_descriptor_absolute, mesh_yaml));
  MeshAssetDescriptor mesh_descriptor{};
  expect_true("clip reimport fixture: parse mesh descriptor",
              AssetYaml::parseMeshDescriptor(mesh_yaml, mesh_descriptor));

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path mesh_gltf_absolute =
      resolveResourcesVirtual(mesh_descriptor.source);
  expect_true("clip reimport fixture: mesh glTF exists",
              file_system.exists(mesh_gltf_absolute));

  for (const ImportResult& clip : imported.animation_clips) {
    eastl::string desc_rel = clip.descriptor_virtual_path;
    if (startsWith(desc_rel, "assets/")) {
      desc_rel.erase(0, 7);
    }
    const fs::path descriptor_absolute =
        file_system.resolveAsset(fs::path(desc_rel.c_str()));
    eastl::string descriptor_yaml;
    expect_true("clip reimport fixture: read clip descriptor",
                file_system.readText(descriptor_absolute, descriptor_yaml));
    AnimationClipAssetDescriptor clip_descriptor{};
    expect_true("clip reimport fixture: parse clip descriptor",
                AssetYaml::parseAnimationClipDescriptor(descriptor_yaml,
                                                        clip_descriptor));

    const fs::path clip_intermediate_absolute =
        resolveResourcesVirtual(clip_descriptor.source);
    writeTextFile(clip_intermediate_absolute, "STALE_CLIP_MARKER");
    expect_true("clip reimport fixture: stamped stale clip yaml",
                readTextFile(clip_intermediate_absolute) == "STALE_CLIP_MARKER");
  }

  expect_true("clip reimport: requestReimport succeeds",
              import_service.requestReimport(mesh_guid));

  expect_true("clip reimport preserves mesh GUID",
              registry.resolveGuid(mesh_guid) ==
                  imported.descriptor_virtual_path);
  expect_true("clip reimport preserves idle clip GUID",
              registry.resolveGuid(idle_guid) ==
                  imported.animation_clips[0].descriptor_virtual_path);
  expect_true("clip reimport preserves walk clip GUID",
              registry.resolveGuid(walk_guid) ==
                  imported.animation_clips[1].descriptor_virtual_path);

  bool refreshed_idle = false;
  bool refreshed_walk = false;
  for (const ImportResult& clip : imported.animation_clips) {
    eastl::string desc_rel = clip.descriptor_virtual_path;
    if (startsWith(desc_rel, "assets/")) {
      desc_rel.erase(0, 7);
    }
    const fs::path descriptor_absolute =
        file_system.resolveAsset(fs::path(desc_rel.c_str()));
    eastl::string descriptor_yaml;
    expect_true("clip reimport: read clip descriptor after",
                file_system.readText(descriptor_absolute, descriptor_yaml));
    AnimationClipAssetDescriptor clip_descriptor{};
    expect_true("clip reimport: parse clip descriptor after",
                AssetYaml::parseAnimationClipDescriptor(descriptor_yaml,
                                                        clip_descriptor));
    expect_true("clip reimport: clip descriptor guid unchanged",
                clip_descriptor.guid == clip.guid);

    const fs::path clip_intermediate_absolute =
        resolveResourcesVirtual(clip_descriptor.source);
    const std::string clip_yaml_after =
        readTextFile(clip_intermediate_absolute);
    expect_true("clip reimport: clip yaml refreshed (not stale marker)",
                clip_yaml_after != "STALE_CLIP_MARKER");

    AnimationClipData clip_data{};
    expect_true("clip reimport: parse refreshed clip yaml",
                AssetYaml::parseAnimationClipData(
                    eastl::string(clip_yaml_after.c_str()), clip_data));
    if (clip_data.name == "idle") {
      refreshed_idle = true;
      expect_true("clip reimport: idle duration from glTF",
                  clip_data.duration == 1.0f);
    } else if (clip_data.name == "walk") {
      refreshed_walk = true;
      expect_true("clip reimport: walk duration from glTF",
                  clip_data.duration == 0.5f);
    }
  }

  expect_true("clip reimport refreshed idle clip", refreshed_idle);
  expect_true("clip reimport refreshed walk clip", refreshed_walk);

  import_service.shutdown();
  compiler.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 2.3: clip GUID preservation when mesh descriptor stem differs from
// Intermediate source stem (e.g. rig_1.mesh.yaml → resources/Models/rig/rig.gltf).
void reimportPreservesClipGuidsWhenMeshDescriptorStemDiffers() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_stem_mismatch_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "rig.gltf";
  writeDualAnimationGltfFixture(external);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, nullptr, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = true;
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("stem mismatch fixture: mesh import succeeds", imported.success);
  expect_true("stem mismatch fixture: two clips extracted",
              imported.animation_clips.size() == 2);

  const eastl::string mesh_guid = imported.guid;
  const eastl::string idle_guid = imported.animation_clips[0].guid;
  const eastl::string walk_guid = imported.animation_clips[1].guid;

  eastl::string old_desc_rel = imported.descriptor_virtual_path;
  if (startsWith(old_desc_rel, "assets/")) {
    old_desc_rel.erase(0, 7);
  }
  const fs::path old_descriptor_absolute =
      file_system.resolveAsset(fs::path(old_desc_rel.c_str()));
  const fs::path new_descriptor_absolute =
      file_system.resolveAsset(fs::path("Meshes/rig_1.mesh.yaml"));
  fs::rename(old_descriptor_absolute, new_descriptor_absolute);
  const eastl::string renamed_descriptor_virtual =
      eastl::string("assets/Meshes/rig_1.mesh.yaml");
  expect_true("stem mismatch fixture: registry updated for renamed descriptor",
              registry.registerAsset(mesh_guid, renamed_descriptor_virtual));
  expect_true("stem mismatch fixture: resolveGuid returns renamed path",
              registry.resolveGuid(mesh_guid) == renamed_descriptor_virtual);

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  for (const ImportResult& clip : imported.animation_clips) {
    eastl::string desc_rel = clip.descriptor_virtual_path;
    if (startsWith(desc_rel, "assets/")) {
      desc_rel.erase(0, 7);
    }
    const fs::path descriptor_absolute =
        file_system.resolveAsset(fs::path(desc_rel.c_str()));
    eastl::string descriptor_yaml;
    expect_true("stem mismatch fixture: read clip descriptor",
                file_system.readText(descriptor_absolute, descriptor_yaml));
    AnimationClipAssetDescriptor clip_descriptor{};
    expect_true("stem mismatch fixture: parse clip descriptor",
                AssetYaml::parseAnimationClipDescriptor(descriptor_yaml,
                                                        clip_descriptor));
    expect_true("stem mismatch fixture: clips still under Animations/rig/",
                clip_descriptor.source.find("resources/Animations/rig/") == 0);

    const fs::path clip_intermediate_absolute =
        resolveResourcesVirtual(clip_descriptor.source);
    writeTextFile(clip_intermediate_absolute, "STALE_CLIP_MARKER");
  }

  expect_true("stem mismatch reimport: requestReimport succeeds",
              import_service.requestReimport(mesh_guid));

  expect_true("stem mismatch reimport preserves mesh GUID",
              registry.resolveGuid(mesh_guid) == renamed_descriptor_virtual);
  expect_true("stem mismatch reimport preserves idle clip GUID",
              registry.resolveGuid(idle_guid) ==
                  imported.animation_clips[0].descriptor_virtual_path);
  expect_true("stem mismatch reimport preserves walk clip GUID",
              registry.resolveGuid(walk_guid) ==
                  imported.animation_clips[1].descriptor_virtual_path);

  for (const ImportResult& clip : imported.animation_clips) {
    eastl::string desc_rel = clip.descriptor_virtual_path;
    if (startsWith(desc_rel, "assets/")) {
      desc_rel.erase(0, 7);
    }
    const fs::path descriptor_absolute =
        file_system.resolveAsset(fs::path(desc_rel.c_str()));
    eastl::string descriptor_yaml;
    expect_true("stem mismatch reimport: read clip descriptor after",
                file_system.readText(descriptor_absolute, descriptor_yaml));
    AnimationClipAssetDescriptor clip_descriptor{};
    expect_true("stem mismatch reimport: parse clip descriptor after",
                AssetYaml::parseAnimationClipDescriptor(descriptor_yaml,
                                                        clip_descriptor));
    expect_true("stem mismatch reimport: clip guid unchanged",
                clip_descriptor.guid == clip.guid);

    const fs::path clip_intermediate_absolute =
        resolveResourcesVirtual(clip_descriptor.source);
    expect_true("stem mismatch reimport: clip yaml refreshed",
                readTextFile(clip_intermediate_absolute) != "STALE_CLIP_MARKER");
  }

  import_service.shutdown();
  compiler.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

}  // namespace

int main() {
  companionAnimationGltfAcceptance();
  nearDiskCompanionGltfCandidateEnumeration();
  companionAnimationGltfMultiSelectBatchPairing();
  importExternalFilesPairsCompanionsIntoMeshImport();
  importExternalBufferCompanionPersistsSidecarAndExtractsClip();
  importRealDogWalkChocomelSources();
  multiHostBatchDoesNotRediscoverOrphanCompanions();
  standaloneCompanionOnlyImportRegistersClips();
  deleteAssetRemovesMeshWithoutDependents();
  deleteAssetRemovesAnimationClipWithoutDependents();
  deleteAssetDetachesAnimationPlayerClipThenDeletes();
  deleteAssetRemovesSceneWithoutDependents();
  deleteAssetDetachesSceneDependentsThenDeletes();
  singleMeshImportDiscoversNearDiskCompanions();
  meshExtensionRoutingTables();
  importColladaMeshRejected();
  importMeshWritesIntermediateAndDescriptor();
  importGltfWithTwoAnimationsRegistersMeshAndClips();
  importMethodKeysFromGltfExtrasPreservesYaml();
  reimportCompanionClipPreservesGuidAndRefreshesYaml();
  reimportPreservesAnimationClipGuidsAndRefreshesYaml();
  reimportPreservesClipGuidsWhenMeshDescriptorStemDiffers();
  importTextureWritesIntermediateAndDescriptor();
  importObjSourceExportDualWritesArchiveAndIntermediate();
  importGltfIntermediateDirectNotSourceExport();
  importUnsupportedSourceExportRejected();
  reimportObjPreservesGuidAndRefreshesIntermediate();
  reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal();
  registryScanDoesNotUpgradeGltfToDae();
  upgradeLegacyMeshIntermediatesIsNoOp();
  upgradeLegacyDaeIntermediateToGltfPreservesGuid();
  registryScanUpgradesDaeToGltf();
  upgradeLegacyDaeFailSoftLeavesDaeSource();
  upgradeLegacyDaeWithArchivedObjReimportsGltf();
  reimportFromArchivedObjRegeneratesGltfIntermediate();
  if (g_failures != 0) {
    std::fprintf(stderr, "asset_import_test: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "asset_import_test: all passed\n");
  return 0;
}
