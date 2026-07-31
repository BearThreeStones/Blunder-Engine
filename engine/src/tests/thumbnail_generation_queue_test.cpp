#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/thumbnail/thumbnail_generation_queue.h"
#include "runtime/resource/thumbnail/thumbnail_generator.h"

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

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_thumbnail_queue_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

constexpr char kMinimalTriangleGltf[] = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
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

class OrderedMeshPreviewBackend final : public Blunder::IMeshPreviewRenderBackend {
 public:
  eastl::vector<eastl::string> rendered_paths;

  bool renderMeshPreview(const Blunder::MeshAsset&,
                         const Blunder::MeshPreviewRenderRequest& request,
                         const Blunder::MeshPreviewCameraFrame&,
                         const Blunder::MeshPreviewStudioLights&,
                         Blunder::MeshPreviewPoseMode,
                         eastl::vector<uint8_t>& out_rgba) override {
    rendered_paths.push_back(request.mesh_virtual_path);
    out_rgba.assign(static_cast<size_t>(request.width) * request.height * 4u,
                    0u);
    for (size_t i = 0; i < out_rgba.size(); i += 4u) {
      out_rgba[i + 3] = 255u;
    }
    return true;
  }
};

void setupMeshDescriptor(const fs::path& project, const char* yaml_name,
                         const char* guid, const char* gltf_name,
                         const char* virtual_descriptor_path) {
  writeTextFile(project / "Resources" / "Models" / gltf_name, kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / yaml_name,
                std::string("type: Mesh\n") + "guid: " + guid + "\n" +
                    "source: resources/Models/" + gltf_name + "\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");
  (void)virtual_descriptor_path;
}

void queuePrioritizesVisibleMeshOverOffScreen() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  setupMeshDescriptor(project, "offscreen.mesh.yaml",
                      "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee01", "offscreen.gltf",
                      "assets/Meshes/offscreen.mesh.yaml");
  setupMeshDescriptor(project, "visible.mesh.yaml",
                      "bbbbbbbb-cccc-4ddd-8eee-ffffffffff02", "visible.gltf",
                      "assets/Meshes/visible.mesh.yaml");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  OrderedMeshPreviewBackend backend;
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

  ThumbnailGenerationQueue queue;
  queue.bind(&generator);

  ContentEntry offscreen{};
  offscreen.virtual_path = "assets/Meshes/offscreen.mesh.yaml";
  offscreen.is_directory = false;
  offscreen.modified_time = 1;

  ContentEntry visible{};
  visible.virtual_path = "assets/Meshes/visible.mesh.yaml";
  visible.is_directory = false;
  visible.modified_time = 2;

  queue.enqueue(offscreen, ThumbnailQueuePriority::Background);
  queue.enqueue(visible, ThumbnailQueuePriority::Background);
  queue.setPriority(visible.virtual_path, ThumbnailQueuePriority::Visible);

  const eastl::vector<ThumbnailQueueCompleted> first =
      queue.tick(/*max_items=*/1u);
  expect_true("first tick processes one item", first.size() == 1u);
  expect_true("visible mesh processed first",
              first[0].virtual_path == visible.virtual_path);
  expect_true("mesh preview invoked once", backend.rendered_paths.size() == 1u);
  expect_true("visible mesh rendered first",
              backend.rendered_paths[0] == "resources/Models/visible.gltf");

  const eastl::vector<ThumbnailQueueCompleted> second = queue.tick(1u);
  expect_true("second tick processes off-screen mesh", second.size() == 1u);
  expect_true("off-screen mesh processed second",
              second[0].virtual_path == offscreen.virtual_path);

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void texturePathUnchangedThroughQueue() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Assets" / "Textures");
  fs::create_directories(project / "Resources" / "Textures");

  constexpr unsigned char kMinimalPng[] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
      0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
      0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
      0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
      0x00, 0x03, 0x01, 0x01, 0x00, 0xC9, 0xFE, 0x92, 0xEF, 0x00, 0x00, 0x00,
      0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

  {
    std::ofstream out(project / "Resources" / "Textures" / "pixel.png",
                      std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(kMinimalPng), sizeof(kMinimalPng));
  }
  writeTextFile(project / "Assets" / "Textures" / "pixel.texture.yaml",
                "type: Texture2D\n"
                "guid: cccccccc-dddd-4eee-8fff-aaaaaaaaaa03\n"
                "source: resources/Textures/pixel.png\n"
                "import:\n  srgb: true\n  generate_mips: false\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init{};
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  OrderedMeshPreviewBackend backend;
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

  ThumbnailGenerationQueue queue;
  queue.bind(&generator);

  ContentEntry texture{};
  texture.virtual_path = "assets/Textures/pixel.texture.yaml";
  texture.is_directory = false;
  texture.modified_time = 1;

  queue.enqueue(texture, ThumbnailQueuePriority::Visible);
  const eastl::vector<ThumbnailQueueCompleted> completed = queue.tick(1u);
  expect_true("texture queued item completes", completed.size() == 1u);
  expect_true("texture generation succeeded",
              completed[0].result.status == ThumbnailStatus::Generated);
  expect_true("mesh preview not used for texture thumbnail",
              backend.rendered_paths.empty());

  generator.shutdown();
  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  queuePrioritizesVisibleMeshOverOffScreen();
  texturePathUnchangedThroughQueue();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    Blunder::g_runtime_global_context.m_logger_system.reset();
    return 1;
  }
  std::printf("thumbnail_generation_queue_test: all passed\n");
  Blunder::g_runtime_global_context.m_logger_system.reset();
  return 0;
}
