#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/thumbnail/thumbnail_cache.h"
#include "runtime/resource/thumbnail/thumbnail_generator.h"
#include "runtime/resource/thumbnail/thumbnail_placeholders.h"
#include "runtime/resource/thumbnail/thumbnail_resize.h"

#include "../../3rdparty/stb/stb_image.h"

#include <chrono>
#include <cstdio>
#include <cstring>
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

void expect_eq_u8(const char* label, uint8_t actual, uint8_t expected) {
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

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_thumbnail_generator_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Textures");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

void writeBinaryFile(const fs::path& path, const unsigned char* bytes,
                     size_t size) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes),
            static_cast<std::streamsize>(size));
}

// 1x1 RGB PNG (red pixel).
constexpr unsigned char kMinimalPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
    0x00, 0x03, 0x01, 0x01, 0x00, 0xC9, 0xFE, 0x92, 0xEF, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

constexpr char kTexturedTriangleGltf[] = R"({
  "asset": { "version": "2.0" },
  "images": [{ "uri": "albedo.png" }],
  "textures": [{ "source": 0 }],
  "materials": [{
    "pbrMetallicRoughness": {
      "baseColorTexture": { "index": 0 }
    }
  }],
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

bool loadRgbaFromPngFile(const fs::path& path, eastl::vector<uint8_t>& out_rgba,
                         int& out_w, int& out_h) {
  int comp = 0;
  unsigned char* pixels =
      stbi_load(path.string().c_str(), &out_w, &out_h, &comp, STBI_rgb_alpha);
  if (pixels == nullptr) {
    return false;
  }
  out_rgba.assign(pixels,
                  pixels + static_cast<size_t>(out_w) * static_cast<size_t>(out_h) * 4u);
  stbi_image_free(pixels);
  return true;
}

bool rgbaIsSolidColor(const eastl::vector<uint8_t>& rgba, uint8_t r, uint8_t g,
                      uint8_t b) {
  if (rgba.size() < 4u || (rgba.size() % 4u) != 0u) {
    return false;
  }
  for (size_t i = 0; i < rgba.size(); i += 4u) {
    if (rgba[i + 0] != r || rgba[i + 1] != g || rgba[i + 2] != b ||
        rgba[i + 3] != 255u) {
      return false;
    }
  }
  return true;
}

class TrackingMeshPreviewBackend final : public Blunder::IMeshPreviewRenderBackend {
 public:
  uint32_t call_count{0};

  bool renderMeshPreview(const Blunder::MeshAsset&,
                         const Blunder::MeshPreviewRenderRequest& request,
                         const Blunder::MeshPreviewCameraFrame&,
                         const Blunder::MeshPreviewStudioLights&,
                         Blunder::MeshPreviewPoseMode,
                         eastl::vector<uint8_t>& out_rgba) override {
    ++call_count;
    out_rgba.assign(static_cast<size_t>(request.width) * request.height * 4u,
                    0u);
    for (size_t i = 0; i < out_rgba.size(); i += 4u) {
      out_rgba[i + 0] = 42u;
      out_rgba[i + 1] = 43u;
      out_rgba[i + 2] = 44u;
      out_rgba[i + 3] = 255u;
    }
    return true;
  }
};

class FailingMeshPreviewBackend final : public Blunder::IMeshPreviewRenderBackend {
 public:
  bool renderMeshPreview(const Blunder::MeshAsset&,
                         const Blunder::MeshPreviewRenderRequest&,
                         const Blunder::MeshPreviewCameraFrame&,
                         const Blunder::MeshPreviewStudioLights&,
                         Blunder::MeshPreviewPoseMode,
                         eastl::vector<uint8_t>&) override {
    return false;
  }
};

void setupTexturedMeshProject(const fs::path& project, const char* kGuid,
                              const char* kDescriptorPath) {
  writeBinaryFile(project / "Resources" / "Models" / "albedo.png", kMinimalPng,
                  sizeof(kMinimalPng));
  writeTextFile(project / "Resources" / "Models" / "textured.gltf",
                kTexturedTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "textured.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/textured.gltf\n" +
                    "import:\n  materials: true\n  animations: false\n"
                    "  scale: 1\n");
}

void meshThumbnailUsesMeshPreviewRenderOnSuccess() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee01";
  const char* kDescriptorPath = "assets/Meshes/textured.mesh.yaml";
  setupTexturedMeshProject(project, kGuid, kDescriptorPath);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("textured mesh loads", mesh != nullptr);
  expect_true("textured mesh has base-color texture",
              mesh && mesh->getMaterialAsset() &&
                  mesh->getMaterialAsset()->hasBaseColorTexture());

  TrackingMeshPreviewBackend backend;
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit service_init{};
  service_init.asset_manager = &manager;
  service_init.backend = &backend;
  service.initialize(service_init);

  ThumbnailGenerator generator;
  ThumbnailGeneratorInit thumb_init{};
  thumb_init.file_system = &file_system;
  thumb_init.asset_manager = &manager;
  thumb_init.mesh_preview_service = &service;
  thumb_init.thumbnail_size = 32;
  generator.initialize(thumb_init);

  ContentEntry entry{};
  entry.virtual_path = kDescriptorPath;
  entry.is_directory = false;
  entry.modified_time = 1;

  eastl::vector<uint8_t> rgba;
  expect_true("mesh thumbnail generation succeeds",
              generator.generateThumbnailRgba(entry, rgba));
  expect_true("mesh preview backend invoked", backend.call_count == 1u);
  expect_eq_u8("preview still red channel", rgba[0], 42u);
  expect_eq_u8("preview still green channel", rgba[1], 43u);
  expect_eq_u8("preview still blue channel", rgba[2], 44u);
  expect_true("preview still is not base-color resize",
              !rgbaIsSolidColor(rgba, 255u, 0u, 0u));

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void meshThumbnailFailureUsesPlaceholderNotBaseColor() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "bbbbbbbb-cccc-4ddd-8eee-ffffffffff02";
  const char* kDescriptorPath = "assets/Meshes/textured.mesh.yaml";
  setupTexturedMeshProject(project, kGuid, kDescriptorPath);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("textured mesh loads for failure path", mesh != nullptr);

  FailingMeshPreviewBackend backend;
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit service_init{};
  service_init.asset_manager = &manager;
  service_init.backend = &backend;
  service.initialize(service_init);

  ThumbnailGenerator generator;
  ThumbnailGeneratorInit thumb_init{};
  thumb_init.file_system = &file_system;
  thumb_init.asset_manager = &manager;
  thumb_init.mesh_preview_service = &service;
  thumb_init.thumbnail_size = 32;
  generator.initialize(thumb_init);

  ContentEntry entry{};
  entry.virtual_path = kDescriptorPath;
  entry.is_directory = false;
  entry.modified_time = 1;

  eastl::vector<uint8_t> rgba;
  expect_true("mesh thumbnail fallback succeeds",
              generator.generateThumbnailRgba(entry, rgba));

  eastl::vector<uint8_t> expected_placeholder;
  fillThumbnailPlaceholder(ThumbnailPlaceholderKind::Mesh, 32, 32,
                           expected_placeholder);
  expect_true("failure uses mesh placeholder size",
              rgba.size() == expected_placeholder.size());
  expect_true("failure uses mesh placeholder pixels",
              std::memcmp(rgba.data(), expected_placeholder.data(),
                          rgba.size()) == 0);
  expect_true("failure is not base-color resize",
              !rgbaIsSolidColor(rgba, 255u, 0u, 0u));

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void meshThumbnailCacheStoresPreviewStillOnRegenerate() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "dddddddd-eeee-4fff-8aaa-bbbbbbbbbbb04";
  const char* kDescriptorPath = "assets/Meshes/textured.mesh.yaml";
  setupTexturedMeshProject(project, kGuid, kDescriptorPath);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  TrackingMeshPreviewBackend backend;
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit service_init{};
  service_init.asset_manager = &manager;
  service_init.backend = &backend;
  service.initialize(service_init);

  ThumbnailGenerator generator;
  ThumbnailGeneratorInit thumb_init{};
  thumb_init.file_system = &file_system;
  thumb_init.asset_manager = &manager;
  thumb_init.mesh_preview_service = &service;
  thumb_init.thumbnail_size = 32;
  generator.initialize(thumb_init);

  ContentEntry entry{};
  entry.virtual_path = kDescriptorPath;
  entry.is_directory = false;
  entry.modified_time = 42;

  const ThumbnailResult generated = generator.ensureThumbnail(entry);
  expect_true("ensureThumbnail reports generated",
              generated.status == ThumbnailStatus::Generated);
  expect_true("generated cache path set", !generated.cache_path.empty());

  const ThumbnailResult cached = generator.probeThumbnailStatus(entry);
  expect_true("probe reports cache hit after write",
              cached.status == ThumbnailStatus::CacheHit);
  expect_true("cache hit path matches generated path",
              cached.cache_path == generated.cache_path);
  expect_true("cache png exists on disk",
              fs::exists(fs::path(generated.cache_path.c_str())));

  eastl::vector<uint8_t> cached_rgba;
  int width = 0;
  int height = 0;
  expect_true("cached png decodes",
              loadRgbaFromPngFile(fs::path(generated.cache_path.c_str()),
                                  cached_rgba, width, height));
  expect_true("cached png dimensions match thumbnail size", width == 32 && height == 32);
  expect_eq_u8("cached still red channel", cached_rgba[0], 42u);
  expect_eq_u8("cached still green channel", cached_rgba[1], 43u);
  expect_eq_u8("cached still blue channel", cached_rgba[2], 44u);
  expect_true("cached still is not texture resize red",
              !rgbaIsSolidColor(cached_rgba, 255u, 0u, 0u));

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void meshThumbnailMtimeInvalidatesOldTextureCache() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "eeeeeeee-ffff-4aaa-8bbb-cccccccccc05";
  const char* kDescriptorPath = "assets/Meshes/textured.mesh.yaml";
  setupTexturedMeshProject(project, kGuid, kDescriptorPath);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  ContentEntry stale_entry{};
  stale_entry.virtual_path = kDescriptorPath;
  stale_entry.is_directory = false;
  stale_entry.modified_time = 1;

  ThumbnailCache legacy_cache;
  legacy_cache.bind(&file_system);
  const ThumbnailCachePaths stale_paths = legacy_cache.pathsForEntry(stale_entry);
  eastl::vector<uint8_t> legacy_texture_rgba(
      static_cast<size_t>(32) * 32 * 4u, 0u);
  for (size_t i = 0; i < legacy_texture_rgba.size(); i += 4u) {
    legacy_texture_rgba[i + 0] = 255u;
    legacy_texture_rgba[i + 1] = 0u;
    legacy_texture_rgba[i + 2] = 0u;
    legacy_texture_rgba[i + 3] = 255u;
  }
  expect_true("legacy texture cache write succeeds",
              legacy_cache.writePng(stale_paths, 32, 32, legacy_texture_rgba.data(),
                                    stale_entry.modified_time));
  expect_true("legacy cache valid for old mtime",
              legacy_cache.isCacheValid(stale_entry, stale_paths));

  TrackingMeshPreviewBackend backend;
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit service_init{};
  service_init.asset_manager = &manager;
  service_init.backend = &backend;
  service.initialize(service_init);

  ThumbnailGenerator generator;
  ThumbnailGeneratorInit thumb_init{};
  thumb_init.file_system = &file_system;
  thumb_init.asset_manager = &manager;
  thumb_init.mesh_preview_service = &service;
  thumb_init.thumbnail_size = 32;
  generator.initialize(thumb_init);

  ContentEntry fresh_entry = stale_entry;
  fresh_entry.modified_time = 2;

  const ThumbnailResult stale_probe = generator.probeThumbnailStatus(fresh_entry);
  expect_true("new mtime misses legacy texture cache",
              stale_probe.status == ThumbnailStatus::None);

  const ThumbnailResult regenerated = generator.ensureThumbnail(fresh_entry);
  expect_true("regenerate reports generated",
              regenerated.status == ThumbnailStatus::Generated);
  expect_true("regenerated path differs from legacy path",
              regenerated.cache_path != stale_paths.png_path);
  expect_true("mesh preview invoked for regenerate", backend.call_count == 1u);

  eastl::vector<uint8_t> cached_rgba;
  int width = 0;
  int height = 0;
  expect_true("regenerated png decodes",
              loadRgbaFromPngFile(fs::path(regenerated.cache_path.c_str()),
                                  cached_rgba, width, height));
  expect_eq_u8("regenerated still red channel", cached_rgba[0], 42u);
  expect_eq_u8("regenerated still green channel", cached_rgba[1], 43u);
  expect_eq_u8("regenerated still blue channel", cached_rgba[2], 44u);
  expect_true("regenerated still is not legacy texture red",
              !rgbaIsSolidColor(cached_rgba, 255u, 0u, 0u));

  const ThumbnailResult fresh_probe = generator.probeThumbnailStatus(fresh_entry);
  expect_true("fresh mtime probes regenerated cache",
              fresh_probe.status == ThumbnailStatus::CacheHit);
  expect_true("fresh probe path matches regenerated path",
              fresh_probe.cache_path == regenerated.cache_path);

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void textureThumbnailPathUnchanged() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "cccccccc-dddd-4eee-8fff-aaaaaaaaaa03";
  const char* kDescriptorPath = "assets/Textures/pixel.texture.yaml";

  writeBinaryFile(project / "Resources" / "Textures" / "pixel.png", kMinimalPng,
                  sizeof(kMinimalPng));
  writeTextFile(project / "Assets" / "Textures" / "pixel.texture.yaml",
                std::string("type: Texture2D\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Textures/pixel.png\n" +
                    "import:\n  srgb: true\n  generate_mips: false\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  TrackingMeshPreviewBackend backend;
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit service_init{};
  service_init.asset_manager = &manager;
  service_init.backend = &backend;
  service.initialize(service_init);

  ThumbnailGenerator generator;
  ThumbnailGeneratorInit thumb_init{};
  thumb_init.file_system = &file_system;
  thumb_init.asset_manager = &manager;
  thumb_init.mesh_preview_service = &service;
  thumb_init.thumbnail_size = 16;
  generator.initialize(thumb_init);

  ContentEntry entry{};
  entry.virtual_path = kDescriptorPath;
  entry.is_directory = false;
  entry.modified_time = 1;

  eastl::vector<uint8_t> rgba;
  expect_true("texture thumbnail generation succeeds",
              generator.generateThumbnailRgba(entry, rgba));
  expect_true("mesh preview not used for texture",
              backend.call_count == 0u);
  expect_true("texture thumbnail is solid red resize",
              rgbaIsSolidColor(rgba, 255u, 0u, 0u));

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  meshThumbnailUsesMeshPreviewRenderOnSuccess();
  meshThumbnailFailureUsesPlaceholderNotBaseColor();
  meshThumbnailCacheStoresPreviewStillOnRegenerate();
  meshThumbnailMtimeInvalidatesOldTextureCache();
  textureThumbnailPathUnchanged();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    Blunder::g_runtime_global_context.m_logger_system.reset();
    return 1;
  }
  std::printf("thumbnail_generator_test: all passed\n");
  Blunder::g_runtime_global_context.m_logger_system.reset();
  return 0;
}
