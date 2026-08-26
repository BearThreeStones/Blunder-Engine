#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/authorship_system.h"
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/scene_thumbnail/capture.h"
#include "runtime/function/render/scene_thumbnail/i_scene_still_gpu.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"
#include "runtime/function/render/scene_thumbnail/scene_thumbnail_render.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/editor_launch.h"
#include "runtime/project/player_launch.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

std::vector<char*> makeArgv(std::vector<std::string>& storage) {
  std::vector<char*> argv;
  argv.reserve(storage.size());
  for (std::string& s : storage) {
    argv.push_back(s.data());
  }
  return argv;
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
  namespace fs = std::filesystem;

  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }

  expect_true("headless editor still editor",
              EngineHostMode::Editor != static_cast<EngineHostMode>(2));
  expect_true("headless editor no window",
              !hostCreatesOsWindow(true));
  expect_true("windowed editor has window",
              hostCreatesOsWindow(false));
  expect_true("headless editor no shell",
              !hostMountsEditorShell(EngineHostMode::Editor, true));
  expect_true("windowed editor has shell",
              hostMountsEditorShell(EngineHostMode::Editor, false));
  expect_true("headless player no shell",
              !hostMountsEditorShell(EngineHostMode::Player, true));
  expect_true("headless editor authorship",
              authorshipSystemEnabled(EngineHostMode::Editor));
  expect_true("headless player no authorship",
              !authorshipSystemEnabled(EngineHostMode::Player));
  expect_true("headless editor play session",
              hostMountsPlaySession(EngineHostMode::Editor));
  expect_true("headless player no play session",
              !hostMountsPlaySession(EngineHostMode::Player));

  {
    std::vector<std::string> args = {"engine_editor", "--project-root",
                                     "C:/Games/Demo", "--headless"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch launch = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("editor launch headless", launch.ok && launch.headless);
    expect_true("editor launch still editor mode",
                hostMountsPlaySession(EngineHostMode::Editor));
    expect_true("editor launch no window", !hostCreatesOsWindow(launch.headless));
    expect_true("editor launch no shell",
                !hostMountsEditorShell(EngineHostMode::Editor, launch.headless));
  }

  {
    std::vector<std::string> args = {
        "engine_player", "--project-root", "C:/Games/Demo", "--scene",
        "assets/Scenes/main.scene.asset", "--headless"};
    auto argv = makeArgv(args);
    const PlayerLaunch launch =
        parsePlayerLaunch(static_cast<int>(argv.size()), argv.data());
    expect_true("player launch headless", launch.ok && launch.headless);
    expect_true("player launch no authorship",
                !authorshipSystemEnabled(EngineHostMode::Player));
    expect_true("player launch no window", !hostCreatesOsWindow(launch.headless));
  }

  {
    const SceneStillExtent cap = captureStillExtent();
    expect_true("capture extent 16:9", cap.width == 1280 && cap.height == 720);
    expect_true("offscreen matches capture",
                cap.width != cap.height);
  }

  {
    FakeStillGpu gpu;
    SceneThumbnailRenderService stills;
    stills.initialize(nullptr, nullptr, &gpu);
    SceneInstance scene;
    const EntityId cam = scene.createEntity(
        "Main Camera", Vec3(0, 0, 5), glm::identity<Quat>(), Vec3(1, 1, 1));
    CameraComponent camera{};
    camera.is_main = true;
    scene.setCamera(cam, camera);
    CaptureRequest req;
    req.subject = CaptureSubject::live;
    req.live_scene = &scene;
    const CaptureResult got = captureScene(stills, req);
    expect_true("headless capture ok", got.ok);
    expect_true("headless capture 16:9", got.width == 1280 && got.height == 720);
    expect_true("headless capture not square", got.width != got.height);
    expect_true("headless capture gpu size",
                gpu.last_width == 1280 && gpu.last_height == 720);
    stills.shutdown();
  }

  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "headless_host_test: all passed\n");
  return 0;
}
