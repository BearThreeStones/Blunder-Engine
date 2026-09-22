#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/gltf_material_extras.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <cgltf.h>
#include <glm/vec4.hpp>

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

void writeBytes(const fs::path& path, const unsigned char* bytes, size_t size) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes),
            static_cast<std::streamsize>(size));
}

fs::path makeTempProject(const char* tag) {
  const fs::path root =
      fs::temp_directory_path() /
      (std::string("blunder_gltf_material_extras_") + tag + "_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Resources" / "Models");
  return root;
}

// 1x1 RGB PNG (red pixel).
constexpr unsigned char kMinimalPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
    0x00, 0x03, 0x01, 0x01, 0x00, 0xC9, 0xFE, 0x92, 0xEF, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

constexpr char kTriangleAccessors[] = R"(
  "accessors": [
    { "bufferView": 0, "componentType": 5123, "count": 3, "type": "SCALAR" },
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

std::string makeFoliageGltf(const char* extras_json, bool include_mr_texture,
                            const char* metallic_factor_json) {
  std::string pbr = "{";
  pbr += "\"baseColorTexture\": { \"index\": 0 }";
  if (include_mr_texture) {
    pbr += ", \"metallicRoughnessTexture\": { \"index\": 1 }";
  }
  if (metallic_factor_json != nullptr && metallic_factor_json[0] != '\0') {
    pbr += ", ";
    pbr += metallic_factor_json;
  }
  pbr += "}";

  std::string images =
      R"("images": [ { "uri": "pine_leaves_albedo_01.png" })";
  if (include_mr_texture) {
    images += R"(, { "uri": "pine_leaves_roughness_01.png" })";
  }
  images += "]";

  std::string textures = R"("textures": [ { "source": 0 })";
  if (include_mr_texture) {
    textures += ", { \"source\": 1 }";
  }
  textures += "]";

  std::string extras;
  if (extras_json != nullptr && extras_json[0] != '\0') {
    extras = std::string(", \"extras\": ") + extras_json;
  }

  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0,
      "material": 0
    }]
  }],
  "materials": [{
    "name": "pine_tree",
    "alphaMode": "BLEND",
    "pbrMetallicRoughness": )") +
         pbr + extras + R"(
  }],
  )" + images +
         ",\n  " + textures + ",\n" + kTriangleAccessors + "\n}\n";
}

eastl::shared_ptr<Blunder::MeshAsset> loadMeshFromProject(
    const fs::path& project, const char* descriptor) {
  using namespace Blunder;
  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  eastl::shared_ptr<MeshAsset> mesh = manager.loadMesh(eastl::string(descriptor));
  manager.shutdown();
  file_system.shutdown();
  return mesh;
}

void writeFoliageProject(const fs::path& project, const std::string& gltf) {
  writeTextFile(project / "Resources" / "Models" / "pine.gltf", gltf);
  writeBytes(project / "Resources" / "Models" / "pine_leaves_albedo_01.png",
             kMinimalPng, sizeof(kMinimalPng));
  writeBytes(project / "Resources" / "Models" / "pine_leaves_roughness_01.png",
             kMinimalPng, sizeof(kMinimalPng));
  writeTextFile(project / "Assets" / "Meshes" / "pine.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee70\n"
                "source: resources/Models/pine.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n");
}

void parserHonorsMaterialInfoMetallic() {
  using namespace Blunder;
  const char* json =
      "{\"material_info\":{\"metallic\":0.0,\"paper_color\":[0.65,0.69,0.57]}}";
  GltfMaterialExtras extras{};
  expect_true("parse material_info metallic",
              parseGltfMaterialExtrasJson(json, std::strlen(json), extras));
  expect_true("extras has metallic", extras.has_metallic);
  expect_true("extras metallic 0", extras.metallic < 0.01f);
  expect_true("paper_color not required", !extras.has_roughness);
}

void parserHonorsTopLevelMetallic() {
  using namespace Blunder;
  const char* json = "{\"metallic\":0,\"roughness\":0.8}";
  GltfMaterialExtras extras{};
  expect_true("parse top-level metallic",
              parseGltfMaterialExtrasJson(json, std::strlen(json), extras));
  expect_true("top metallic 0", extras.has_metallic && extras.metallic < 0.01f);
  expect_true("top roughness 0.8",
              extras.has_roughness && std::fabs(extras.roughness - 0.8f) < 1e-4f);
}

void parserIgnoresMetallicFactorKey() {
  using namespace Blunder;
  const char* json = "{\"metallicFactor\":1.0}";
  GltfMaterialExtras extras{};
  expect_true("metallicFactor is not extras metallic",
              !parseGltfMaterialExtrasJson(json, std::strlen(json), extras));
}

void resolvePolicy() {
  using namespace Blunder;
  GltfMaterialExtras extras{};
  extras.has_metallic = true;
  extras.metallic = 0.0f;
  expect_true(
      "extras 0 wins over default 1 and roughness atlas",
      resolveImportedMetallicFactor(1.0f, extras, "pine_leaves_roughness_01.png") <
          0.01f);

  GltfMaterialExtras metal{};
  metal.has_metallic = true;
  metal.metallic = 1.0f;
  expect_true("extras 1 keeps metal on roughness-named map",
              resolveImportedMetallicFactor(1.0f, metal,
                                            "pine_leaves_roughness_01.png") >
                  0.99f);

  GltfMaterialExtras none{};
  expect_true(
      "roughness-only uri forces dielectric when factor omitted",
      resolveImportedMetallicFactor(1.0f, none, "pine_leaves_roughness_01.png") <
          0.01f);
  expect_true(
      "packed metallic_roughness stays ORM",
      resolveImportedMetallicFactor(1.0f, none, "metallic_roughness.png") >
          0.99f);
  expect_true(
      "explicit factor 0 stays 0 without extras",
      resolveImportedMetallicFactor(0.0f, none, nullptr) < 0.01f);
  expect_true("roughness-only helper",
              metallicRoughnessUriIsRoughnessOnly(
                  "resources/se-world/pine_leaves_roughness_01.png"));
  expect_true("packed orm helper false",
              !metallicRoughnessUriIsRoughnessOnly("packed_orm.png"));
}

void maskPromotion() {
  using namespace Blunder;
  Asset::Meta tex_meta;
  tex_meta.virtual_path = "resources/se-world/pine_leaves_albedo_01.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(tex_meta), 1u, 1u, 4u, eastl::vector<uint8_t>{1, 1, 1, 1});
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "pine";
  MaterialAsset card(eastl::move(mat_meta), glm::vec4(1.0f), AssetHandle{},
                     albedo, nullptr, nullptr, nullptr, glm::vec3(0.15f),
                     glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 1.0f, 1.0f,
                     cgltf_alpha_mode_blend, 0.5f, true, false);
  card.promoteOpaqueTexturedBlendToMask();
  expect_true("foliage BLEND+albedo becomes MASK",
              card.getAlphaMode() == cgltf_alpha_mode_mask);
}

void importExtrasMetallicZero() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("extras");
  writeFoliageProject(
      project, makeFoliageGltf(
                   R"({"material_info":{"metallic":0.0,"paper_color":[0.65,0.69,0.57]}})",
                   true, nullptr));
  const eastl::shared_ptr<MeshAsset> mesh =
      loadMeshFromProject(project, "assets/Meshes/pine.mesh.yaml");
  expect_true("extras pine mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("extras pine has material", material != nullptr);
  if (material != nullptr) {
    expect_true("extras pine metallic 0", material->getMetallicFactor() < 0.01f);
    expect_true("extras pine keeps roughness atlas",
                material->hasMetallicRoughnessTexture());
    expect_true("extras pine MASK cutout",
                material->getAlphaMode() == cgltf_alpha_mode_mask);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void importRoughnessAtlasWithoutExtras() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("rough");
  writeFoliageProject(project, makeFoliageGltf(nullptr, true, nullptr));
  const eastl::shared_ptr<MeshAsset> mesh =
      loadMeshFromProject(project, "assets/Meshes/pine.mesh.yaml");
  expect_true("roughness-atlas mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("roughness-atlas material", material != nullptr);
  if (material != nullptr) {
    expect_true("roughness-atlas dielectric",
                material->getMetallicFactor() < 0.01f);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void importExplicitMetallicFactorZero() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("factor");
  writeFoliageProject(project,
                      makeFoliageGltf(nullptr, false, "\"metallicFactor\": 0.0"));
  const eastl::shared_ptr<MeshAsset> mesh =
      loadMeshFromProject(project, "assets/Meshes/pine.mesh.yaml");
  expect_true("factor mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("factor material", material != nullptr);
  if (material != nullptr) {
    expect_true("authored metallicFactor 0",
                material->getMetallicFactor() < 0.01f);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

}  // namespace

int main() {
  parserHonorsMaterialInfoMetallic();
  parserHonorsTopLevelMetallic();
  parserIgnoresMetallicFactorKey();
  resolvePolicy();
  maskPromotion();
  importExtrasMetallicZero();
  importRoughnessAtlasWithoutExtras();
  importExplicitMetallicFactorZero();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d gltf_material_extras_test failure(s)\n", g_failures);
    return 1;
  }
  std::printf("gltf_material_extras_test: all passed\n");
  return 0;
}
