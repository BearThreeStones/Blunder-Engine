#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/asset_cook_types.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <chrono>
#include <cmath>
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

void expect_eq_u32(const char* label, uint32_t actual, uint32_t expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s (got %u want %u)\n", label, actual, expected);
    ++g_failures;
  }
}

void expect_near(const char* label, float actual, float expected,
                 float epsilon = 1e-4f) {
  if (std::fabs(actual - expected) > epsilon) {
    std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, actual, expected);
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
      ("blunder_mesh_preview_render_test_" +
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

bool pathContains(const fs::path& path, const char* needle) {
  const std::string s = path.generic_string();
  return s.find(needle) != std::string::npos;
}

struct CallbackState {
  bool success_called{false};
  bool failure_called{false};
  eastl::string last_error;
};

void onSuccess(const Blunder::MeshPreviewRenderResult&, void* user) {
  auto* state = static_cast<CallbackState*>(user);
  state->success_called = true;
}

void onFailure(const eastl::string& error, void* user) {
  auto* state = static_cast<CallbackState*>(user);
  state->failure_called = true;
  state->last_error = error;
}

void framingUsesAabbWithPadding() {
  using namespace Blunder;

  const AABB bounds = AABB::fromCenterExtents(glm::vec3(1.0f, 2.0f, 3.0f),
                                              glm::vec3(2.0f, 1.0f, 0.5f));

  MeshPreviewFramingParams tight{};
  tight.local_bounds = bounds;
  tight.padding = 1.0f;
  tight.aspect = 1.0f;
  const MeshPreviewCameraFrame tight_frame = computeMeshPreviewCameraFrame(tight);

  MeshPreviewFramingParams padded{};
  padded.local_bounds = bounds;
  padded.padding = 1.5f;
  padded.aspect = 1.0f;
  const MeshPreviewCameraFrame padded_frame =
      computeMeshPreviewCameraFrame(padded);

  expect_true("framing ok", tight_frame.ok && padded_frame.ok);
  expect_near("framing target x", tight_frame.target.x, bounds.center().x);
  expect_near("framing target y", tight_frame.target.y, bounds.center().y);
  expect_near("framing target z", tight_frame.target.z, bounds.center().z);

  const float tight_dist =
      glm::length(tight_frame.eye - tight_frame.target);
  const float padded_dist =
      glm::length(padded_frame.eye - padded_frame.target);
  expect_true("padding increases camera distance", padded_dist > tight_dist);
}

void skinnedMeshUsesBindPoseIntent() {
  using namespace Blunder;

  eastl::vector<MeshVertex> vertices(1);
  eastl::vector<uint32_t> indices{0};
  MeshSkinData skin{};
  skin.influences.push_back(MeshSkinInfluence{});
  skin.joint_to_bone.push_back(0);

  Asset::Meta meta;
  meta.virtual_path = "test/skin.mesh.yaml";
  const eastl::shared_ptr<MeshAsset> mesh =
      eastl::make_shared<MeshAsset>(meta, vertices, indices, AssetHandle{},
                                    nullptr, skin, false);

  expect_true("mesh is skinned", mesh->isSkinned());
  expect_eq_u32("skinned pose mode",
                static_cast<uint32_t>(resolveMeshPreviewPoseMode(
                    *mesh, MeshPreviewPoseMode::RestPose)),
                static_cast<uint32_t>(MeshPreviewPoseMode::BindPose));
}

void renderFailureReturnsClearErrorAndHook() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  CallbackState callbacks{};
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit init;
  init.asset_manager = &manager;
  init.on_success = onSuccess;
  init.on_failure = onFailure;
  init.callback_user = &callbacks;
  service.initialize(init);

  const MeshPreviewRenderResult result =
      service.renderMeshAsset(eastl::string("assets/Meshes/missing.mesh.yaml"));
  expect_true("missing mesh !ok", !result.ok);
  expect_true("missing mesh error non-empty", !result.error.empty());
  expect_true("failure hook called", callbacks.failure_called);
  expect_true("success hook not called", !callbacks.success_called);
  expect_true("failure hook message non-empty", !callbacks.last_error.empty());

  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void renderPrefersFinalWhenAvailable() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "11111111-2222-4333-8444-555555555501";
  const char* kDescriptorPath = "assets/Meshes/final.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "final.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "final.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/final.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  expect_true("cook Final", compiler->cookAsset(eastl::string(kGuid)));

  CallbackState callbacks{};
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit init;
  init.asset_manager = &manager;
  init.on_success = onSuccess;
  init.on_failure = onFailure;
  init.callback_user = &callbacks;
  service.initialize(init);

  const MeshPreviewRenderResult result =
      service.renderMeshAsset(eastl::string(kDescriptorPath));
  expect_true("Final path ok", result.ok);
  expect_eq_u32("Final load source",
                static_cast<uint32_t>(result.load_source),
                static_cast<uint32_t>(MeshPreviewLoadSource::Final));
  expect_true("framing ok", result.framing.ok);
  expect_true("success hook called", callbacks.success_called);
  expect_true("failure hook not called", !callbacks.failure_called);

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("loaded mesh from Final",
              mesh && mesh->isFromCookedFinal());

  service.shutdown();
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void renderUsesIntermediateWhenFinalMissing() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "22222222-3333-4444-8555-666666666602";
  const char* kDescriptorPath = "assets/Meshes/inter.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "inter.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "inter.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/inter.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  expect_true(
      "precondition: Final missing",
      !file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));

  CallbackState callbacks{};
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit init;
  init.asset_manager = &manager;
  init.on_success = onSuccess;
  init.on_failure = onFailure;
  init.callback_user = &callbacks;
  service.initialize(init);

  const MeshPreviewRenderResult result =
      service.renderMeshAsset(eastl::string(kDescriptorPath));
  expect_true("Intermediate path ok", result.ok);
  expect_eq_u32("Intermediate load source",
                static_cast<uint32_t>(result.load_source),
                static_cast<uint32_t>(MeshPreviewLoadSource::Intermediate));
  expect_true("success hook called", callbacks.success_called);

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("loaded mesh from Intermediate",
              mesh && pathContains(mesh->getAbsolutePath(), ".gltf"));
  expect_true("Intermediate not cooked Final flag",
              mesh && !mesh->isFromCookedFinal());

  service.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  framingUsesAabbWithPadding();
  skinnedMeshUsesBindPoseIntent();
  renderFailureReturnsClearErrorAndHook();
  renderPrefersFinalWhenAvailable();
  renderUsesIntermediateWhenFinalMissing();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "mesh_preview_render_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
