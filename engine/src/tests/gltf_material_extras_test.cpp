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
  expect_true("extras paper_color", extras.has_paper_color);
  expect_true("extras paper_color rgb",
              std::fabs(extras.paper_color[0] - 0.65f) < 1e-4f &&
                  std::fabs(extras.paper_color[1] - 0.69f) < 1e-4f &&
                  std::fabs(extras.paper_color[2] - 0.57f) < 1e-4f);
  expect_true("paper_color not roughness", !extras.has_roughness);
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
  expect_true("creek water roughness is not paper atlas",
              !metallicRoughnessUriIsRoughnessOnly(
                  "resources/se-world/assets/textures/"
                  "creek_water_surface-roughness.png"));
  expect_true("paper_rough is paper grain",
              textureUriIsPaperGrain(
                  "resources/se-world/assets/textures/paper_rough_256.png"));
  expect_true("albedo is not paper grain",
              !textureUriIsPaperGrain("pine_leaves_albedo_01.png"));
  expect_true("dummy path pond name",
              materialNameIsDummyPath("DUMMY-path-pond"));
  expect_true("pine is not dummy path", !materialNameIsDummyPath("pine_tree"));
  expect_true("pond albedo file",
              dummyPathAlbedoFileName("DUMMY-path-pond") != nullptr &&
                  std::strcmp(dummyPathAlbedoFileName("DUMMY-path-pond"),
                              "path_7_albedo.png") == 0);
  expect_true("fence albedo file",
              dummyPathAlbedoFileName("DUMMY-path-fence") != nullptr &&
                  std::strcmp(dummyPathAlbedoFileName("DUMMY-path-fence"),
                              "path_1_albedo.png") == 0);
  expect_true("path_7 uri is paper albedo",
              textureUriIsPathPaperAlbedo(
                  "resources/se-world/assets/lib/textures/path_7_albedo.png"));
  expect_true("pine albedo is not path paper",
              !textureUriIsPathPaperAlbedo("pine_leaves_albedo_01.png"));
  expect_true("fence-paths yaml is dummy set",
              meshSourceLooksLikeDummyPathSet(
                  "assets/Meshes/se-world/SL-fence-paths-m1p0.mesh.yaml"));
  expect_true("clearing bushes is not dummy set",
              !meshSourceLooksLikeDummyPathSet(
                  "assets/Meshes/se-world/SL-clearing-bushes-m0p0.mesh.yaml"));
  float dirt[3] = {0.0f, 0.0f, 0.0f};
  dummyPathFallbackAlbedoRgb(dirt);
  expect_true("dirt fallback red-brown",
              dirt[0] > dirt[1] && dirt[1] > dirt[2] && dirt[2] > 0.1f);
  expect_true("dummy snow 01 name",
              materialNameIsDummySnowPatch("DUMMY-snow_patch_01"));
  expect_true("dummy snow 02 name",
              materialNameIsDummySnowPatch("DUMMY-snow_patch_02"));
  expect_true("pine is not dummy snow",
              !materialNameIsDummySnowPatch("pine_tree"));
  expect_true("path is not dummy snow",
              !materialNameIsDummySnowPatch("DUMMY-path-pond"));
  expect_true("snow albedo file",
              dummySnowPatchAlbedoFileName("DUMMY-snow_patch_01") != nullptr &&
                  std::strcmp(dummySnowPatchAlbedoFileName("DUMMY-snow_patch_01"),
                              "snow_gen_albedo-01.png") == 0);
  expect_true("snow 02 uses same albedo file",
              dummySnowPatchAlbedoFileName("DUMMY-snow_patch_02") != nullptr &&
                  std::strcmp(dummySnowPatchAlbedoFileName("DUMMY-snow_patch_02"),
                              "snow_gen_albedo-01.png") == 0);
  expect_true("snow_gen uri is snow paper",
              textureUriIsSnowPatchAlbedo(
                  "resources/se-world/assets/textures/snow_gen_albedo-01.png"));
  expect_true("pine albedo is not snow paper",
              !textureUriIsSnowPatchAlbedo("pine_leaves_albedo_01.png"));
  expect_true("path albedo is not snow paper",
              !textureUriIsSnowPatchAlbedo(
                  "resources/se-world/assets/lib/textures/path_7_albedo.png"));
  expect_true("hub snow_patches yaml is dummy snow set",
              meshSourceLooksLikeDummySnowPatchSet(
                  "assets/Meshes/se-world/SL-hub-snow_patches-m26p0.mesh.yaml"));
  expect_true("fence snow_patches yaml is dummy snow set",
              meshSourceLooksLikeDummySnowPatchSet(
                  "assets/Meshes/se-world/SL-fence-snow_patches-m20p0.mesh.yaml"));
  expect_true("clearing snow yaml is dummy snow set",
              meshSourceLooksLikeDummySnowPatchSet(
                  "assets/Meshes/se-world/SL-clearing-snow-m1p0.mesh.yaml"));
  expect_true("clearing snow gltf is dummy snow set",
              meshSourceLooksLikeDummySnowPatchSet(
                  "resources/se-world/assets/sets/clearing/SL-clearing-snow.gltf"));
  expect_true("clearing bushes is not dummy snow set",
              !meshSourceLooksLikeDummySnowPatchSet(
                  "assets/Meshes/se-world/SL-clearing-bushes-m0p0.mesh.yaml"));
  expect_true("snow_edge_plateau name",
              materialNameIsSnowEdgePlateau("snow_edge_plateau"));
  expect_true("dummy snow is not plateau snow",
              !materialNameIsSnowEdgePlateau("DUMMY-snow_patch_01"));
  expect_true("snow_wall_top name", materialNameIsWallSnow("snow_wall_top"));
  expect_true("snow_edge_stone_wall name",
              materialNameIsWallSnow("snow_edge_stone_wall"));
  expect_true("plateau snow is not wall snow",
              !materialNameIsWallSnow("snow_edge_plateau"));
  expect_true("plateau albedo uri is paper snow",
              textureUriIsPlateauSnowAlbedo(
                  "resources/se-world/assets/lib/stone_plateau_albedo.png"));
  expect_true("snow_gen is not plateau albedo",
              !textureUriIsPlateauSnowAlbedo(
                  "resources/se-world/assets/textures/snow_gen_albedo-01.png"));
  expect_true("wall snow edge albedo uri is paper",
              textureUriIsWallSnowAlbedo(
                  "resources/se-world/assets/textures/"
                  "snow_edge_plateaus-albedo.png"));
  expect_true("snow_gen is not wall-edge albedo",
              !textureUriIsWallSnowAlbedo(
                  "resources/se-world/assets/textures/snow_gen_albedo-01.png"));
  expect_true("plateau albedo is not wall-edge albedo",
              !textureUriIsWallSnowAlbedo(
                  "resources/se-world/assets/lib/stone_plateau_albedo.png"));
  expect_true("creek-bed uri is creek paper",
              textureUriIsCreekPaperAlbedo(
                  "resources/se-world/assets/lib/textures/creek-bed.png"));
  expect_true("creek-snow_edge uri is creek paper",
              textureUriIsCreekPaperAlbedo(
                  "resources/se-world/assets/lib/textures/creek-snow_edge.png"));
  expect_true("creek water albedo is not paper card",
              !textureUriIsCreekPaperAlbedo(
                  "resources/se-world/assets/textures/"
                  "creek_water_surface-albedo.png"));
  expect_true("creek water albedo is water film",
              textureUriIsWaterSurfaceFilm(
                  "resources/se-world/assets/textures/"
                  "creek_water_surface-albedo.png"));
  expect_true("ice squiggles albedo is water film",
              textureUriIsWaterSurfaceFilm(
                  "resources/se-world/assets/textures/"
                  "ice_surface_squiggles-albedo.png"));
  expect_true("creek-bed is not water film",
              !textureUriIsWaterSurfaceFilm(
                  "resources/se-world/assets/lib/textures/creek-bed.png"));
  expect_true("path albedo is not creek paper",
              !textureUriIsCreekPaperAlbedo(
                  "resources/se-world/assets/lib/textures/path_7_albedo.png"));
  expect_true("world-creek yaml is creek set",
              meshSourceLooksLikeWorldCreek(
                  "assets/Meshes/se-world/SL-world-creek-m3p0.mesh.yaml"));
  expect_true("world-creek gltf is creek set",
              meshSourceLooksLikeWorldCreek(
                  "resources/se-world/assets/sets/world/SL-world-creek.gltf"));
  expect_true("clearing snow is not creek set",
              !meshSourceLooksLikeWorldCreek(
                  "assets/Meshes/se-world/SL-clearing-snow-m1p0.mesh.yaml"));
  float snow[3] = {0.0f, 0.0f, 0.0f};
  dummySnowPatchFallbackAlbedoRgb(snow);
  expect_true("snow fallback pale cool white",
              snow[2] >= snow[1] && snow[1] > snow[0] && snow[0] > 0.8f);
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
    expect_true("extras pine paper_color", material->hasPaperColor());
    expect_true("extras pine paper card", material->isPaperCard());
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
    expect_true("roughness-atlas paper card", material->isPaperCard());
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void writeDummyPathProject(const fs::path& project, const std::string& gltf,
                           bool write_albedo) {
  writeTextFile(project / "Resources" / "se-world" / "assets" / "sets" /
                    "fence" / "SL-fence-paths.gltf",
                gltf);
  if (write_albedo) {
    writeBytes(project / "Resources" / "se-world" / "assets" / "lib" /
                   "textures" / "path_7_albedo.png",
               kMinimalPng, sizeof(kMinimalPng));
  }
  writeTextFile(project / "Assets" / "Meshes" / "se-world" /
                    "SL-fence-paths-m0p0.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee91\n"
                "source: resources/se-world/assets/sets/fence/SL-fence-paths.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n");
}

std::string makeDummyPathGltf() {
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "name": "GEO-paths-pond",
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0,
      "material": 0
    }]
  }],
  "materials": [{
    "name": "DUMMY-path-pond",
    "doubleSided": true,
    "extras": { "asset_id": "9a9b2737a030d116" },
    "pbrMetallicRoughness": {
      "baseColorFactor": [0.8, 0.8, 0.8, 1],
      "metallicFactor": 0,
      "roughnessFactor": 0.4
    }
  }],
  )") + kTriangleAccessors + "\n}\n";
}

void importDummyPathBindsAlbedo() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("dummypath");
  writeDummyPathProject(project, makeDummyPathGltf(), true);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/SL-fence-paths-m0p0.mesh.yaml");
  expect_true("dummy path mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("dummy path has material", material != nullptr);
  if (material != nullptr) {
    expect_true("dummy path binds Godot albedo",
                material->hasBaseColorTexture());
    expect_true("dummy path paper card", material->isPaperCard());
    expect_true("dummy path MASK cutout",
                material->getAlphaMode() == cgltf_alpha_mode_mask);
    expect_true("dummy path white factor over texture",
                std::fabs(material->getBaseColorFactor().x - 1.0f) < 1e-4f &&
                    std::fabs(material->getBaseColorFactor().y - 1.0f) < 1e-4f &&
                    std::fabs(material->getBaseColorFactor().z - 1.0f) < 1e-4f);
    expect_true("dummy path dielectric",
                material->getMetallicFactor() < 0.01f);
    const eastl::shared_ptr<Texture2DAsset>& albedo =
        material->getBaseColorTextureAsset();
    expect_true(
        "dummy path albedo uri",
        albedo &&
            albedo->getVirtualPath().find("path_7_albedo.png") !=
                eastl::string::npos);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void importDummyPathFallbackDirtFactor() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("dummypath_factor");
  writeDummyPathProject(project, makeDummyPathGltf(), false);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/SL-fence-paths-m0p0.mesh.yaml");
  expect_true("fallback dummy path mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("fallback dummy path material", material != nullptr);
  if (material != nullptr) {
    expect_true("fallback has no albedo file",
                !material->hasBaseColorTexture());
    expect_true("fallback paper card", material->isPaperCard());
    const glm::vec4& factor = material->getBaseColorFactor();
    expect_true("fallback dirt not dummy gray",
                factor.x < 0.7f && factor.x > factor.y && factor.y > factor.z);
    expect_true("fallback not black", factor.z > 0.1f);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void writeDummySnowProject(const fs::path& project, const std::string& gltf,
                           bool write_albedo) {
  writeTextFile(project / "Resources" / "se-world" / "assets" / "sets" /
                    "hub" / "SL-hub-snow_patches.gltf",
                gltf);
  if (write_albedo) {
    writeBytes(project / "Resources" / "se-world" / "assets" / "textures" /
                   "snow_gen_albedo-01.png",
               kMinimalPng, sizeof(kMinimalPng));
  }
  writeTextFile(project / "Assets" / "Meshes" / "se-world" /
                    "SL-hub-snow_patches-m0p0.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee92\n"
                "source: resources/se-world/assets/sets/hub/SL-hub-snow_patches.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n");
}

std::string makeDummySnowGltf() {
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "name": "GEO-snow_patch",
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0,
      "material": 0
    }]
  }],
  "materials": [{
    "name": "DUMMY-snow_patch_01",
    "doubleSided": true,
    "extras": { "asset_id": "45903d90dac8635e" },
    "pbrMetallicRoughness": {
      "baseColorFactor": [0.8, 0.8, 0.8, 1],
      "metallicFactor": 0,
      "roughnessFactor": 0.4
    }
  }],
  )") + kTriangleAccessors + "\n}\n";
}

void importDummySnowBindsAlbedo() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("dummysnow");
  writeDummySnowProject(project, makeDummySnowGltf(), true);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/SL-hub-snow_patches-m0p0.mesh.yaml");
  expect_true("dummy snow mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("dummy snow has material", material != nullptr);
  if (material != nullptr) {
    expect_true("dummy snow binds Godot albedo",
                material->hasBaseColorTexture());
    expect_true("dummy snow paper card", material->isPaperCard());
    expect_true("dummy snow MASK cutout",
                material->getAlphaMode() == cgltf_alpha_mode_mask);
    const glm::vec4& factor = material->getBaseColorFactor();
    expect_true("dummy snow albedo_color over texture",
                std::fabs(factor.x - 0.92f) < 1e-4f &&
                    std::fabs(factor.y - 0.971f) < 1e-4f &&
                    std::fabs(factor.z - 1.0f) < 1e-4f);
    expect_true("dummy snow dielectric",
                material->getMetallicFactor() < 0.01f);
    const eastl::shared_ptr<Texture2DAsset>& albedo =
        material->getBaseColorTextureAsset();
    expect_true(
        "dummy snow albedo uri",
        albedo &&
            albedo->getVirtualPath().find("snow_gen_albedo-01.png") !=
                eastl::string::npos);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void importDummySnowFallbackSnowFactor() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("dummysnow_factor");
  writeDummySnowProject(project, makeDummySnowGltf(), false);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/SL-hub-snow_patches-m0p0.mesh.yaml");
  expect_true("fallback dummy snow mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("fallback dummy snow material", material != nullptr);
  if (material != nullptr) {
    expect_true("fallback has no albedo file",
                !material->hasBaseColorTexture());
    expect_true("fallback snow paper card", material->isPaperCard());
    const glm::vec4& factor = material->getBaseColorFactor();
    expect_true("fallback snow not dummy gray",
                factor.z >= factor.y && factor.y > factor.x && factor.x > 0.8f);
    expect_true("fallback snow not black", factor.x > 0.8f);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void writeClearingSnowProject(const fs::path& project, const std::string& gltf,
                              bool write_albedo) {
  writeTextFile(project / "Resources" / "se-world" / "assets" / "sets" /
                    "clearing" / "SL-clearing-snow.gltf",
                gltf);
  if (write_albedo) {
    writeBytes(project / "Resources" / "se-world" / "assets" / "textures" /
                   "snow_gen_albedo-01.png",
               kMinimalPng, sizeof(kMinimalPng));
  }
  writeTextFile(project / "Assets" / "Meshes" / "se-world" /
                    "SL-clearing-snow-m1p0.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee93\n"
                "source: resources/se-world/assets/sets/clearing/"
                "SL-clearing-snow.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n");
}

void importClearingSnowBindsAlbedo() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("clearingsnow");
  writeClearingSnowProject(project, makeDummySnowGltf(), true);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/SL-clearing-snow-m1p0.mesh.yaml");
  expect_true("clearing snow mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("clearing snow has material", material != nullptr);
  if (material != nullptr) {
    expect_true("clearing snow binds Godot albedo",
                material->hasBaseColorTexture());
    expect_true("clearing snow paper card", material->isPaperCard());
    expect_true("clearing snow MASK cutout",
                material->getAlphaMode() == cgltf_alpha_mode_mask);
    const glm::vec4& factor = material->getBaseColorFactor();
    expect_true("clearing snow albedo_color over texture",
                std::fabs(factor.x - 0.92f) < 1e-4f &&
                    std::fabs(factor.y - 0.971f) < 1e-4f &&
                    std::fabs(factor.z - 1.0f) < 1e-4f);
    const eastl::shared_ptr<Texture2DAsset>& albedo =
        material->getBaseColorTextureAsset();
    expect_true(
        "clearing snow albedo uri",
        albedo &&
            albedo->getVirtualPath().find("snow_gen_albedo-01.png") !=
                eastl::string::npos);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void writePlateauSnowProject(const fs::path& project, const std::string& gltf,
                             bool write_albedo) {
  writeTextFile(project / "Resources" / "se-world" / "assets" / "lib" /
                    "stone_plateaus" / "LI-stone_plateau_001.gltf",
                gltf);
  if (write_albedo) {
    writeBytes(project / "Resources" / "se-world" / "assets" / "lib" /
                   "stone_plateau_albedo.png",
               kMinimalPng, sizeof(kMinimalPng));
  }
  writeTextFile(project / "Assets" / "Meshes" / "se-world" /
                    "LI-stone_plateau_001-m1p0.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee94\n"
                "source: resources/se-world/assets/lib/stone_plateaus/"
                "LI-stone_plateau_001.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n  meshIndex: 0\n  primitiveIndex: 0\n");
}

std::string makePlateauSnowGltf() {
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "name": "GEO-stone_plateau_snow_001",
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0,
      "material": 0
    }]
  }],
  "materials": [{
    "name": "snow_edge_plateau",
    "alphaMode": "BLEND",
    "extras": {
      "material_info": {
        "metallic": 0.0,
        "paper_color": [1.0, 1.0, 1.0]
      }
    },
    "pbrMetallicRoughness": {
      "baseColorFactor": [1, 1, 1, 1],
      "metallicFactor": 0,
      "baseColorTexture": { "index": 0 }
    }
  }],
  "textures": [{ "source": 0 }],
  "images": [{ "uri": "../stone_plateau_albedo.png" }],
  )") + kTriangleAccessors + "\n}\n";
}

void importPlateauSnowMarksPaper() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("plateausnow");
  writePlateauSnowProject(project, makePlateauSnowGltf(), true);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/LI-stone_plateau_001-m1p0.mesh.yaml");
  expect_true("plateau snow mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("plateau snow has material", material != nullptr);
  if (material != nullptr) {
    expect_true("plateau snow binds albedo", material->hasBaseColorTexture());
    expect_true("plateau snow paper card", material->isPaperCard());
    expect_true("plateau snow two-sided", material->isDoubleSided());
    expect_true("plateau snow MASK cutout",
                material->getAlphaMode() == cgltf_alpha_mode_mask);
    expect_true("plateau snow dielectric",
                material->getMetallicFactor() < 0.01f);
    const eastl::shared_ptr<Texture2DAsset>& albedo =
        material->getBaseColorTextureAsset();
    expect_true(
        "plateau snow albedo uri",
        albedo &&
            albedo->getVirtualPath().find("stone_plateau_albedo.png") !=
                eastl::string::npos);
  }
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void writeWallSnowProject(const fs::path& project, const std::string& gltf,
                          bool write_albedo) {
  writeTextFile(project / "Resources" / "se-world" / "assets" / "lib" /
                    "stone_walls" / "stone_walls" / "LI-stone_wall_004.gltf",
                gltf);
  if (write_albedo) {
    writeBytes(project / "Resources" / "se-world" / "assets" / "textures" /
                   "snow_edge_plateaus-albedo.png",
               kMinimalPng, sizeof(kMinimalPng));
  }
  writeTextFile(project / "Assets" / "Meshes" / "se-world" /
                    "LI-stone_wall_004-m1p1.mesh.yaml",
                "type: Mesh\n"
                "guid: aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee95\n"
                "source: resources/se-world/assets/lib/stone_walls/stone_walls/"
                "LI-stone_wall_004.gltf\n"
                "import:\n  materials: true\n  animations: false\n"
                "  scale: 1\n  meshIndex: 1\n  primitiveIndex: 1\n");
}

std::string makeWallSnowGltf() {
  return std::string(R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 1 }],
  "meshes": [
    {
      "name": "GEO-stone_wall_004",
      "primitives": [{
        "attributes": { "POSITION": 1 },
        "indices": 0
      }]
    },
    {
      "name": "GEO-stone_wall_snow_004",
      "primitives": [
        {
          "attributes": { "POSITION": 1 },
          "indices": 0
        },
        {
          "attributes": { "POSITION": 1 },
          "indices": 0,
          "material": 0
        }
      ]
    }
  ],
  "materials": [{
    "name": "snow_edge_stone_wall",
    "alphaMode": "BLEND",
    "extras": {
      "material_info": {
        "metallic": 0.0,
        "paper_color": [1.0, 1.0, 1.0]
      }
    },
    "pbrMetallicRoughness": {
      "baseColorFactor": [1, 1, 1, 1],
      "metallicFactor": 0,
      "baseColorTexture": { "index": 0 }
    }
  }],
  "textures": [{ "source": 0 }],
  "images": [{ "uri": "../../../textures/snow_edge_plateaus-albedo.png" }],
  )") + kTriangleAccessors + "\n}\n";
}

void importWallSnowMarksPaper() {
  using namespace Blunder;
  ensureLogger();
  const fs::path project = makeTempProject("wallsnow");
  writeWallSnowProject(project, makeWallSnowGltf(), true);
  const eastl::shared_ptr<MeshAsset> mesh = loadMeshFromProject(
      project, "assets/Meshes/se-world/LI-stone_wall_004-m1p1.mesh.yaml");
  expect_true("wall snow mesh loads", mesh != nullptr);
  const MaterialAsset* material =
      mesh != nullptr ? mesh->getMaterialAsset().get() : nullptr;
  expect_true("wall snow has material", material != nullptr);
  if (material != nullptr) {
    expect_true("wall snow binds albedo", material->hasBaseColorTexture());
    expect_true("wall snow paper card", material->isPaperCard());
    expect_true("wall snow two-sided", material->isDoubleSided());
    expect_true("wall snow MASK cutout",
                material->getAlphaMode() == cgltf_alpha_mode_mask);
    expect_true("wall snow dielectric", material->getMetallicFactor() < 0.01f);
    const eastl::shared_ptr<Texture2DAsset>& albedo =
        material->getBaseColorTextureAsset();
    expect_true(
        "wall snow albedo uri",
        albedo && albedo->getVirtualPath().find("snow_edge_plateaus-albedo.png") !=
                      eastl::string::npos);
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
  importDummyPathBindsAlbedo();
  importDummyPathFallbackDirtFactor();
  importDummySnowBindsAlbedo();
  importDummySnowFallbackSnowFactor();
  importClearingSnowBindsAlbedo();
  importPlateauSnowMarksPaper();
  importWallSnowMarksPaper();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d gltf_material_extras_test failure(s)\n", g_failures);
    return 1;
  }
  std::printf("gltf_material_extras_test: all passed\n");
  return 0;
}
