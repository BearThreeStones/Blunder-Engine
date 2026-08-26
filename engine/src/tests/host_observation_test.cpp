#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/scene_thumbnail/capture.h"
#include "runtime/function/render/scene_thumbnail/i_scene_still_gpu.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"
#include "runtime/function/render/scene_thumbnail/scene_thumbnail_render.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/play_frame.h"
#include "runtime/project/play_step.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

class FakeStillGpu final : public Blunder::ISceneStillGpuBackend {
 public:
  bool renderSubmeshDraws(
      const eastl::vector<Blunder::MeshPreviewSubmeshDraw>&,
      const Blunder::MeshPreviewCameraFrame& framing,
      const Blunder::MeshPreviewStudioLights&, uint32_t width, uint32_t height,
      eastl::vector<uint8_t>& out_rgba,
      const Blunder::SceneInstance*) override {
    last_framing = framing;
    last_width = width;
    last_height = height;
    ++calls;
    const size_t bytes = static_cast<size_t>(width) * height * 4u;
    out_rgba.assign(bytes, 40);
    return true;
  }

  Blunder::MeshPreviewCameraFrame last_framing{};
  uint32_t last_width{0};
  uint32_t last_height{0};
  int calls{0};
};

}  // namespace

int main() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }

  {
    const SceneStillExtent cap = captureStillExtent();
    expect_true("capture 16:9 width", cap.width == 1280);
    expect_true("capture 16:9 height", cap.height == 720);
    expect_true("capture not square", cap.width != cap.height);
    const SceneStillExtent thumb = sceneStillExtent(1, 1, 128);
    expect_true("thumb square", thumb.width == 128 && thumb.height == 128);
  }

  {
    eastl::vector<uint8_t> src(4u * 4u * 4u, 200);
    eastl::vector<uint8_t> fitted;
    SceneStillExtent extent{};
    expect_true("fit rgba", fitRgbaToSceneStill(src.data(), 4, 4, 16, 9, 16,
                                               fitted, extent));
    expect_true("fit 16:9", extent.width == 16 && extent.height == 9);
    expect_true("fit not square", extent.width != extent.height);
    expect_true("fit bytes",
                fitted.size() ==
                    static_cast<size_t>(extent.width) * extent.height * 4u);
  }

  {
    int ticks = 0;
    float last_dt = 0.0f;
    expect_true("step while playing fails",
                !applyPlayStep(false, 30, [&](float) { ++ticks; }));
    expect_true("playing step applies no ticks", ticks == 0);
    expect_true("step while paused",
                applyPlayStep(true, 3, [&](float dt) {
                  ++ticks;
                  last_dt = dt;
                }));
    expect_true("paused step tick count", ticks == 3);
    expect_true("paused step dt 1/60",
                std::fabs(last_dt - k_play_step_dt) < 1e-6f);
  }

  {
    eastl::vector<uint8_t> pixels;
    uint32_t w = 0;
    uint32_t h = 0;
    expect_true("play frame without render system fails",
                !capturePlayProcessFrame(pixels, w, h));
  }

  FakeStillGpu gpu;
  SceneThumbnailRenderService stills;
  stills.initialize(nullptr, nullptr, &gpu);

  {
    SceneInstance scene;
    CaptureRequest req;
    req.subject = CaptureSubject::live;
    req.live_scene = &scene;
    req.write_cache = true;
    const CaptureResult got = captureScene(stills, req);
    expect_true("no camera fails", !got.ok);
    expect_true("no camera code",
                got.failure_code == k_request_capture_no_camera);
    expect_true("no camera no pixels", got.rgba.empty());
    expect_true("no camera no gpu", gpu.calls == 0);
    expect_true("no camera no cache", !got.wrote_cache);
  }

  {
    CaptureRequest req;
    req.subject = CaptureSubject::live;
    req.live_scene = nullptr;
    const CaptureResult got = captureScene(stills, req);
    expect_true("no live document", !got.ok);
    expect_true("no live code",
                got.failure_code == k_request_capture_no_live_document);
  }

  {
    SceneInstance live;
    CaptureRequest req;
    req.subject = CaptureSubject::onDisk;
    req.live_scene = &live;
    req.scene_virtual_path = "";
    const int calls_before = gpu.calls;
    const CaptureResult got = captureScene(stills, req);
    expect_true("on-disk unreadable", !got.ok);
    expect_true("on-disk unreadable code",
                got.failure_code == k_request_capture_scene_unreadable);
    expect_true("on-disk ignores live gpu", gpu.calls == calls_before);
  }

  {
    SceneInstance scene;
    DocumentHistory history;
    const EntityId cam = scene.createEntity(
        "Main Camera", Vec3(0, 0, 5), glm::identity<Quat>(), Vec3(1, 1, 1));
    CameraComponent camera{};
    camera.is_main = true;
    scene.setCamera(cam, camera);

    CaptureRequest req;
    req.subject = CaptureSubject::live;
    req.live_scene = &scene;
    req.write_cache = true;
    const CaptureResult got = captureScene(stills, req);
    expect_true("live capture ok", got.ok);
    expect_true("live 16:9 width", got.width == 1280);
    expect_true("live 16:9 height", got.height == 720);
    expect_true("live not square", got.width != got.height);
    expect_true("live pixels", !got.rgba.empty());
    expect_true("capture no cache write", !got.wrote_cache);
    expect_true("history unchanged", history.commandCount() == 0);

    const float eye_x_saved = gpu.last_framing.eye.x;
    scene.getEntity(cam)->setPosition(Vec3(10, 0, 5));
    scene.markTransformsDirty();
    const CaptureResult dirty = captureScene(stills, req);
    expect_true("live dirty ok", dirty.ok);
    expect_true("live sees unsaved pose",
                std::fabs(gpu.last_framing.eye.x - eye_x_saved) > 1.0f);

    SceneInstance from_disk;
    const EntityId disk_cam = from_disk.createEntity(
        "Main Camera", Vec3(0, 0, 5), glm::identity<Quat>(), Vec3(1, 1, 1));
    from_disk.setCamera(disk_cam, camera);
    SceneStillRequest disk_still{};
    disk_still.live_instance = &from_disk;
    disk_still.width = 1280;
    disk_still.height = 720;
    disk_still.require_mesh = false;
    const SceneThumbnailRenderResult disk_got =
        stills.renderSceneStill(disk_still);
    expect_true("on-disk pose still ok", disk_got.ok);
    expect_true("on-disk ignores unsaved",
                std::fabs(gpu.last_framing.eye.x - eye_x_saved) < 0.1f);
  }

  {
    SceneThumbnailRenderRequest thumb{};
    thumb.scene_virtual_path = "assets/Scenes/missing.scene.asset";
    thumb.width = 128;
    thumb.height = 128;
    const SceneThumbnailRenderResult thumb_got = stills.renderSceneAsset(thumb);
    expect_true("thumb still square request",
                thumb_got.width == 128 && thumb_got.height == 128);
    expect_true("thumb missing stays unreadable not capture pixels",
                !thumb_got.ok);
  }

  stills.shutdown();
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
