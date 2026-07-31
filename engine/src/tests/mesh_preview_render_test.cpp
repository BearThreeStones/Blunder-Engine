#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"
#include "runtime/function/render/mesh_preview/mesh_preview_offscreen_backend.h"
#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/function/render/mesh_preview/mesh_preview_studio_lights.h"
#include "runtime/function/render/overlay/camera_preview_rt_size.h"
#include "runtime/function/render/viewport_style.h"
#include "runtime/function/render/rhi/i_command_list.h"
#include "runtime/function/render/rhi/i_frame_sync.h"
#include "runtime/function/render/rhi/i_gpu_buffer.h"
#include "runtime/function/render/rhi/i_gpu_texture.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/i_render_device.h"
#include "runtime/function/render/rhi/i_shader_compiler.h"
#include "runtime/function/render/rhi/render_backend_factory.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/platform/window/window_system.h"

#include <vulkan/vulkan.h>
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
bool g_gpu_mesh_preview_readback_verified = false;

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

constexpr char kDualMaterialDualMeshGltf[] = R"({
  "asset": { "version": "2.0" },
  "materials": [
    { "pbrMetallicRoughness": { "baseColorFactor": [0.85, 0.12, 0.12, 1.0] } },
    { "pbrMetallicRoughness": { "baseColorFactor": [0.12, 0.12, 0.85, 1.0] } }
  ],
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [
      {
        "attributes": { "POSITION": 1 },
        "indices": 0,
        "material": 0
      },
      {
        "attributes": { "POSITION": 1 },
        "indices": 0,
        "material": 1
      }
    ]
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

bool stringContains(const eastl::string& haystack, const char* needle) {
  return haystack.find(needle) != eastl::string::npos;
}

bool vec3Near(const glm::vec3& actual, const glm::vec3& expected,
              float epsilon = 1e-4f) {
  return glm::length(actual - expected) <= epsilon;
}

void expectStudioLightsDefault(const char* label,
                               const Blunder::MeshPreviewStudioLights& lights) {
  const Blunder::MeshPreviewStudioLights defaults =
      Blunder::defaultMeshPreviewStudioLights();
  expect_true(label, vec3Near(lights.key_light_direction,
                              defaults.key_light_direction));
  expect_true(label, vec3Near(lights.key_light_color, defaults.key_light_color));
  expect_true(label, vec3Near(lights.fill_light_direction,
                              defaults.fill_light_direction));
  expect_true(label, vec3Near(lights.fill_light_color, defaults.fill_light_color));
  expect_true(label, vec3Near(lights.ambient_color, defaults.ambient_color));
  expect_true(label, lights.shadows_enabled == defaults.shadows_enabled);
}

class StubNonVulkanRenderBackend final : public Blunder::rhi::IRenderBackend {
  struct DummyDevice final : Blunder::rhi::IRenderDevice {
    void initialize(const Blunder::rhi::RenderDeviceDesc&) override {}
    void shutdown() override {}
    eastl::unique_ptr<Blunder::rhi::IOffscreenRenderTarget> createOffscreenTarget(
        const Blunder::rhi::OffscreenTargetDesc&) override {
      return nullptr;
    }
    eastl::unique_ptr<Blunder::rhi::IGpuBuffer> createBuffer(
        const Blunder::rhi::BufferDesc&) override {
      return nullptr;
    }
    eastl::unique_ptr<Blunder::rhi::IGpuTexture> createTextureFromAsset(
        const Blunder::Texture2DAsset*) override {
      return nullptr;
    }
    eastl::unique_ptr<Blunder::rhi::ICommandList> beginImmediateCommandList()
        override {
      return nullptr;
    }
  };
  struct DummyCompiler final : Blunder::rhi::IShaderCompiler {
    Blunder::rhi::ShaderBytecode compile(const char*, const char*,
                                         Blunder::rhi::ShaderStage) override {
      return {};
    }
  };
  struct DummyFrameSync final : Blunder::rhi::IFrameSync {
    uint32_t maxFramesInFlight() const override { return 1; }
    void waitForFrame(uint32_t) override {}
    void resetFrameFence(uint32_t) override {}
    void signalFrameSubmitted(uint32_t) override {}
  };

  DummyDevice m_device;
  DummyCompiler m_compiler;
  DummyFrameSync m_sync;

 public:
  Blunder::rhi::RenderBackendType type() const override {
    return Blunder::rhi::RenderBackendType::D3D12;
  }
  Blunder::rhi::IRenderDevice& device() override { return m_device; }
  const Blunder::rhi::IRenderDevice& device() const override {
    return m_device;
  }
  Blunder::rhi::IShaderCompiler& shaderCompiler() override {
    return m_compiler;
  }
  const Blunder::rhi::IShaderCompiler& shaderCompiler() const override {
    return m_compiler;
  }
  Blunder::rhi::IFrameSync& frameSync() override { return m_sync; }
  const Blunder::rhi::IFrameSync& frameSync() const override {
    return m_sync;
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

class ClearReadbackMeshPreviewBackend final
    : public Blunder::IMeshPreviewRenderBackend {
 public:
  bool renderMeshPreview(const Blunder::MeshAsset&,
                         const Blunder::MeshPreviewRenderRequest& request,
                         const Blunder::MeshPreviewCameraFrame&,
                         const Blunder::MeshPreviewStudioLights&,
                         Blunder::MeshPreviewPoseMode,
                         eastl::vector<uint8_t>& out_rgba) override {
    out_rgba.assign(static_cast<size_t>(request.width) * request.height * 4u,
                    0u);
    for (size_t i = 3; i < out_rgba.size(); i += 4u) {
      out_rgba[i] = 255u;
    }
    return true;
  }
};

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

void meshPreviewRenderTargetOwnershipIsDedicated() {
  using namespace Blunder;

  expect_true(
      "Mesh Preview owner differs from Camera Preview",
      MeshPreviewOffscreenBackend::k_render_target_owner !=
          kCameraPreviewRenderTargetOwner);
  expect_true(
      "Mesh Preview owner differs from main viewport",
      MeshPreviewOffscreenBackend::k_render_target_owner !=
          PreviewRenderTargetOwner::MainViewport);
  expect_eq_u32(
      "Mesh Preview owner tag",
      static_cast<uint32_t>(MeshPreviewOffscreenBackend::k_render_target_owner),
      static_cast<uint32_t>(PreviewRenderTargetOwner::MeshPreview));
}

void meshPreviewOffscreenBackendRejectsInvalidInit() {
  using namespace Blunder;

  auto* backend = new MeshPreviewOffscreenBackend();
  expect_true("offscreen target null before init",
              backend->offscreenTarget() == nullptr);
  expect_true("initialize rejects null backend",
              !backend->initialize(nullptr));

  StubNonVulkanRenderBackend non_vulkan;
  expect_true("initialize rejects non-Vulkan backend",
              !backend->initialize(&non_vulkan));
  expect_true("offscreen target still null after failed init",
              backend->offscreenTarget() == nullptr);

  Asset::Meta meta;
  meta.virtual_path = "test/uninit.mesh.yaml";
  const eastl::shared_ptr<MeshAsset> mesh =
      eastl::make_shared<MeshAsset>(meta, eastl::vector<MeshVertex>{},
                                    eastl::vector<uint32_t>{}, AssetHandle{},
                                    nullptr, MeshSkinData{}, false);
  MeshPreviewRenderRequest request{};
  request.width = 64;
  request.height = 64;
  MeshPreviewFramingParams framing_params{};
  framing_params.local_bounds =
      AABB::fromCenterExtents(glm::vec3(0.0f), glm::vec3(1.0f));
  framing_params.aspect = 1.0f;
  const MeshPreviewCameraFrame framing =
      computeMeshPreviewCameraFrame(framing_params);
  const MeshPreviewStudioLights lights = defaultMeshPreviewStudioLights();
  eastl::vector<uint8_t> rgba;
  expect_true("render without initialize fails",
              !backend->renderMeshPreview(*mesh, request, framing, lights,
                                          MeshPreviewPoseMode::RestPose, rgba));
  expect_true("readback empty when render fails", rgba.empty());

  backend->shutdown();
  expect_true("offscreen target null after shutdown",
              backend->offscreenTarget() == nullptr);
  delete backend;
}

bool vulkanLoaderAvailable() {
  uint32_t extension_count = 0;
  return vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                              nullptr) == VK_SUCCESS;
}

void meshPreviewOffscreenBackendGpuHarnessWhenAvailable() {
  using namespace Blunder;

  if (!vulkanLoaderAvailable()) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: GPU gate skipped (Vulkan loader "
                 "unavailable)\n");
    return;
  }

  ensureLogger();

  WindowSystem window;
  WindowCreateInfo win_info{};
  win_info.width = 64;
  win_info.height = 64;
  win_info.title = "mesh_preview_render_test";
  window.initialize(win_info);
  if (window.getNativeWindow() == nullptr) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: GPU gate skipped (SDL window "
                 "unavailable)\n");
    return;
  }

  rhi::RenderBackendInitInfo backend_init{};
  backend_init.device_desc.window_system = &window;
  backend_init.device_desc.enable_validation = false;
  eastl::unique_ptr<rhi::IRenderBackend> render_backend =
      rhi::RenderBackendFactory::create(rhi::RenderBackendType::Vulkan,
                                        backend_init);
  if (!render_backend) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: GPU gate skipped (Vulkan backend "
                 "create failed)\n");
    window.shutdown();
    return;
  }

  MeshPreviewOffscreenBackend* mesh_backend =
      new MeshPreviewOffscreenBackend();
  expect_true("initialize accepts Vulkan backend",
              mesh_backend->initialize(render_backend.get()));
  expect_true("offscreen target null before ensureResources",
              mesh_backend->offscreenTarget() == nullptr);

  Asset::Meta meta;
  meta.virtual_path = "test/gpu.mesh.yaml";
  const eastl::shared_ptr<MeshAsset> mesh =
      eastl::make_shared<MeshAsset>(meta, eastl::vector<MeshVertex>{},
                                    eastl::vector<uint32_t>{}, AssetHandle{},
                                    nullptr, MeshSkinData{}, false);
  MeshPreviewRenderRequest request{};
  request.width = 64;
  request.height = 64;
  MeshPreviewFramingParams framing_params{};
  framing_params.local_bounds =
      AABB::fromCenterExtents(glm::vec3(0.0f), glm::vec3(1.0f));
  framing_params.aspect = 1.0f;
  const MeshPreviewCameraFrame framing =
      computeMeshPreviewCameraFrame(framing_params);
  const MeshPreviewStudioLights lights = defaultMeshPreviewStudioLights();
  eastl::vector<uint8_t> rgba;
  const bool rendered = mesh_backend->renderMeshPreview(
      *mesh, request, framing, lights, MeshPreviewPoseMode::RestPose, rgba);
  if (!rendered) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: GPU gate skipped (renderMeshPreview "
                 "failed; no GPU readback verified)\n");
    mesh_backend->shutdown();
    delete mesh_backend;
    window.shutdown();
    return;
  }

  g_gpu_mesh_preview_readback_verified = true;
  expect_true("gpu offscreen target allocated after render",
              mesh_backend->offscreenTarget() != nullptr);
  expect_eq_u32("gpu readback width", request.width, 64u);
  expect_eq_u32("gpu readback height", request.height, 64u);
  expect_eq_u32("gpu readback rgba bytes",
                static_cast<uint32_t>(rgba.size()), 64u * 64u * 4u);
  expect_eq_u32("gpu readback alpha", rgba[3], 255u);
  const float bg = Blunder::kViewportBackgroundRgb * 255.0f;
  expect_near("gpu readback clear red", static_cast<float>(rgba[0]), bg, 2.0f);
  expect_near("gpu readback clear green", static_cast<float>(rgba[1]), bg, 2.0f);
  expect_near("gpu readback clear blue", static_cast<float>(rgba[2]), bg, 2.0f);

  mesh_backend->shutdown();
  delete mesh_backend;
  window.shutdown();
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
  expect_true("missing mesh rgba empty (upstream placeholder)",
              result.rgba.empty());
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
  expectStudioLightsDefault("Final studio lights", result.studio_lights);
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
  ClearReadbackMeshPreviewBackend stub_backend;
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit init;
  init.asset_manager = &manager;
  init.backend = &stub_backend;
  init.on_success = onSuccess;
  init.on_failure = onFailure;
  init.callback_user = &callbacks;
  service.initialize(init);

  const MeshPreviewRenderResult result =
      service.renderMeshAsset(eastl::string(kDescriptorPath));
  expect_true("Intermediate path ok (stub backend)", result.ok);
  expect_eq_u32("Intermediate load source",
                static_cast<uint32_t>(result.load_source),
                static_cast<uint32_t>(MeshPreviewLoadSource::Intermediate));
  expect_eq_u32("readback width", result.width, 128u);
  expect_eq_u32("readback height", result.height, 128u);
  expect_eq_u32("readback rgba bytes", static_cast<uint32_t>(result.rgba.size()),
                128u * 128u * 4u);
  expect_true("stub readback alpha populated",
              !result.rgba.empty() && result.rgba[3] == 255u);
  expectStudioLightsDefault("Intermediate studio lights", result.studio_lights);
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

void renderBackendFailureReturnsGpuErrorAndHook() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "33333333-4444-4555-8666-777777777703";
  const char* kDescriptorPath = "assets/Meshes/backend_fail.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "backend_fail.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "backend_fail.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/backend_fail.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  FailingMeshPreviewBackend backend;
  CallbackState callbacks{};
  MeshPreviewRenderService service;
  MeshPreviewRenderServiceInit init;
  init.asset_manager = &manager;
  init.backend = &backend;
  init.on_success = onSuccess;
  init.on_failure = onFailure;
  init.callback_user = &callbacks;
  service.initialize(init);

  const MeshPreviewRenderResult result =
      service.renderMeshAsset(eastl::string(kDescriptorPath));
  expect_true("backend failure !ok", !result.ok);
  expect_true("backend failure error non-empty", !result.error.empty());
  expect_true("backend failure rgba empty (upstream placeholder)",
              result.rgba.empty());
  expect_true("backend failure error mentions GPU",
              stringContains(result.error, "GPU") ||
                  stringContains(result.error, "backend"));
  expect_true("backend failure hook called", callbacks.failure_called);
  expect_true("backend success hook not called", !callbacks.success_called);
  expect_true("backend failure hook message non-empty",
              !callbacks.last_error.empty());

  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void collectMeshPreviewSubmeshesEnumeratesAllPrimitives() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "44444444-5555-4666-8777-888888888804";
  const char* kDescriptorPath = "assets/Meshes/dual.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "dual.gltf",
                kDualMaterialDualMeshGltf);
  writeTextFile(project / "Assets" / "Meshes" / "dual.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/dual.gltf\n" +
                    "import:\n  materials: true\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register dual mesh",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  const eastl::vector<MeshPreviewSubmeshDraw> submeshes =
      collectMeshPreviewSubmeshes(manager, eastl::string(kDescriptorPath));
  expect_true("dual mesh collects >= 2 submeshes", submeshes.size() >= 2u);
  expect_true("dual mesh submeshes have materials",
              submeshes[0].material != nullptr &&
                  submeshes[1].material != nullptr);
  expect_true("dual mesh materials differ",
              submeshes[0].material->getBaseColorFactor() !=
                  submeshes[1].material->getBaseColorFactor());

  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void meshPreviewGpuRendersMaterialColoredPixelsWhenAvailable() {
  using namespace Blunder;

  if (!vulkanLoaderAvailable()) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: material GPU gate skipped "
                 "(Vulkan loader unavailable)\n");
    return;
  }

  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "55555555-6666-4777-8888-999999999905";
  const char* kDescriptorPath = "assets/Meshes/dual_gpu.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "dual_gpu.gltf",
                kDualMaterialDualMeshGltf);
  writeTextFile(project / "Assets" / "Meshes" / "dual_gpu.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/dual_gpu.gltf\n" +
                    "import:\n  materials: true\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  WindowSystem window;
  WindowCreateInfo win_info{};
  win_info.width = 64;
  win_info.height = 64;
  win_info.title = "mesh_preview_material_gpu_test";
  window.initialize(win_info);
  if (window.getNativeWindow() == nullptr) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: material GPU gate skipped (SDL "
                 "window unavailable)\n");
    manager.shutdown();
    file_system.shutdown();
    fs::remove_all(project);
    return;
  }

  rhi::RenderBackendInitInfo backend_init{};
  backend_init.device_desc.window_system = &window;
  backend_init.device_desc.enable_validation = false;
  eastl::unique_ptr<rhi::IRenderBackend> render_backend =
      rhi::RenderBackendFactory::create(rhi::RenderBackendType::Vulkan,
                                        backend_init);
  if (!render_backend) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: material GPU gate skipped (Vulkan "
                 "backend create failed)\n");
    window.shutdown();
    manager.shutdown();
    file_system.shutdown();
    fs::remove_all(project);
    return;
  }

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  if (!mesh) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: material GPU gate skipped (mesh "
                 "load failed)\n");
    window.shutdown();
    manager.shutdown();
    file_system.shutdown();
    fs::remove_all(project);
    return;
  }

  MeshPreviewOffscreenBackend* mesh_backend =
      new MeshPreviewOffscreenBackend();
  expect_true("material gpu initialize accepts asset manager",
              mesh_backend->initialize(render_backend.get(), &manager));

  MeshPreviewRenderRequest request{};
  request.mesh_virtual_path = kDescriptorPath;
  request.width = 64;
  request.height = 64;
  MeshPreviewFramingParams framing_params{};
  framing_params.local_bounds = mesh->getLocalBounds();
  framing_params.padding = 1.2f;
  framing_params.aspect = 1.0f;
  const MeshPreviewCameraFrame framing =
      computeMeshPreviewCameraFrame(framing_params);
  const MeshPreviewStudioLights lights = defaultMeshPreviewStudioLights();
  eastl::vector<uint8_t> rgba;
  const bool rendered = mesh_backend->renderMeshPreview(
      *mesh, request, framing, lights, MeshPreviewPoseMode::RestPose, rgba);
  if (!rendered) {
    std::fprintf(stdout,
                 "mesh_preview_render_test: material GPU gate skipped "
                 "(renderMeshPreview failed)\n");
    mesh_backend->shutdown();
    delete mesh_backend;
    window.shutdown();
    manager.shutdown();
    file_system.shutdown();
    fs::remove_all(project);
    return;
  }

  g_gpu_mesh_preview_readback_verified = true;
  expect_true("material gpu submits >= 2 draws",
              mesh_backend->lastSubmittedDrawCount() >= 2u);
  expect_eq_u32("material gpu readback bytes",
                static_cast<uint32_t>(rgba.size()), 64u * 64u * 4u);

  mesh_backend->shutdown();
  delete mesh_backend;
  window.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void renderEmptyPathReturnsClearErrorAndHook() {
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

  const MeshPreviewRenderResult result = service.renderMeshAsset(eastl::string());
  expect_true("empty path !ok", !result.ok);
  expect_true("empty path error non-empty", !result.error.empty());
  expect_true("empty path error mentions empty",
              stringContains(result.error, "empty"));
  expect_true("empty path rgba empty (upstream placeholder)",
              result.rgba.empty());
  expect_true("empty path failure hook called", callbacks.failure_called);
  expect_true("empty path success hook not called", !callbacks.success_called);

  service.shutdown();
  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void renderWithoutInitializeReturnsErrorAndHook() {
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
  service.shutdown();

  const MeshPreviewRenderResult result =
      service.renderMeshAsset(eastl::string("assets/Meshes/any.mesh.yaml"));
  expect_true("not initialized !ok", !result.ok);
  expect_true("not initialized error mentions initialized",
              stringContains(result.error, "not initialized"));
  expect_true("not initialized failure hook called", callbacks.failure_called);
  expect_true("not initialized success hook not called",
              !callbacks.success_called);

  manager.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  framingUsesAabbWithPadding();
  meshPreviewRenderTargetOwnershipIsDedicated();
  meshPreviewOffscreenBackendRejectsInvalidInit();
  meshPreviewOffscreenBackendGpuHarnessWhenAvailable();
  skinnedMeshUsesBindPoseIntent();
  renderFailureReturnsClearErrorAndHook();
  renderPrefersFinalWhenAvailable();
  renderUsesIntermediateWhenFinalMissing();
  renderBackendFailureReturnsGpuErrorAndHook();
  collectMeshPreviewSubmeshesEnumeratesAllPrimitives();
  meshPreviewGpuRendersMaterialColoredPixelsWhenAvailable();
  renderEmptyPathReturnsClearErrorAndHook();
  renderWithoutInitializeReturnsErrorAndHook();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "mesh_preview_render_test: all passed");
    if (g_gpu_mesh_preview_readback_verified) {
      std::fprintf(stdout, " (GPU readback verified)\n");
    } else {
      std::fprintf(stdout,
                   " (GPU readback not verified; CPU/stub coverage only)\n");
    }
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
