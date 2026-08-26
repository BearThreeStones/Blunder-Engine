#include "runtime/core/math/math_types.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/authorship_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/machine_adapter.h"
#include "runtime/project/machine_mcp.h"
#include "runtime/project/play_session_controller.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::EditorSessionLaunch cliLaunch(const char* verb) {
  Blunder::EditorSessionLaunch launch;
  launch.ok = true;
  launch.headless = true;
  launch.adapter = Blunder::MachineAdapterKind::cli;
  launch.cli.verb = verb;
  launch.project_root = "C:/Games/Demo";
  return launch;
}

}  // namespace

int main() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }

  {
    const uint8_t rgba[] = {255, 0, 0, 255, 0, 255, 0, 255,
                            0, 0, 255, 255, 255, 255, 255, 255};
    eastl::vector<uint8_t> png;
    expect_true("png bytes", encodeRgbaToPngBytes(rgba, 2, 2, png));
    expect_true("png not empty", !png.empty());
    const std::string b64 = base64Encode(png.data(), png.size());
    expect_true("base64 not empty", !b64.empty());
  }

  {
    EditorSessionLaunch launch = cliLaunch("capture");
    launch.cli.subject = "live";
    launch.cli.out_path = "shot.png";
    MachineAdapterHost host;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("live capture no scene", !result.ok);
    expect_true("live capture code",
                result.failure_code == k_request_subject_no_live_document);
    expect_true("live capture exit", result.exit_code != 0);
  }

  {
    EditorSessionLaunch launch = cliLaunch("op");
    launch.scene = "assets/Scenes/main.scene.asset";
    launch.cli.entity = "Player";
    MachineAdapterHost host;
    AuthorshipSystem authorship;
    SceneInstance scene;
    DocumentHistory history;
    authorship.setTestLiveDocument(&scene, &history);
    host.authorship = &authorship;
    host.live_scene = &scene;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("cli op no save", !result.ok);
    expect_true("cli op save code",
                result.failure_code == k_request_cli_save_required);
    expect_true("no command", history.commandCount() == 0);
  }

  {
    EditorSessionLaunch launch = cliLaunch("save");
    MachineAdapterHost host;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("cli save unsupported", !result.ok);
    expect_true("cli save code",
                result.failure_code == k_request_cli_save_unsupported);
  }

  {
    SceneInstance scene;
    DocumentHistory history;
    AuthorshipSystem authorship;
    authorship.setTestLiveDocument(&scene, &history);
    const EntityId id = scene.createEntity(
        "Player", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1, 1, 1));
    expect_true("created player", isValid(id));

    EditorSessionLaunch launch = cliLaunch("query");
    launch.scene = "assets/Scenes/main.scene.asset";
    launch.cli.subject = "live";
    MachineAdapterHost host;
    host.authorship = &authorship;
    host.live_scene = &scene;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("query ok", result.ok);
    expect_true("query names", result.names.size() == 1 && result.names[0] == "Player");
    expect_true("query exit 0", result.exit_code == 0);

    launch.cli.verb = "op";
    launch.cli.save = true;
    launch.cli.entity = "Player";
    launch.cli.tx = 1.0f;
    bool saved = false;
    host.save_live = [&saved]() {
      saved = true;
      return true;
    };
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("op save ok", result.ok);
    expect_true("op pushed", history.commandCount() == 1);
    expect_true("op persisted", saved);

    launch.cli.verb = "diagnose";
    launch.cli.subject = "live";
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("diagnose ran", result.ok);
    expect_true("diagnose exit 0", result.exit_code == 0);
    expect_true("diagnose missing camera",
                issueListHasCode(result.issues, k_issue_play_missing_camera));
    const std::string json = machineResultJson(result);
    expect_true("json has issue code",
                json.find("play.missing_camera") != std::string::npos);
    expect_true("json ok true", json.find("\"ok\":true") != std::string::npos);
  }

  {
    const fs::path out = fs::temp_directory_path() / "blunder-adapter-shot.png";
    std::error_code ec;
    fs::remove(out, ec);
    EditorSessionLaunch launch = cliLaunch("capture");
    launch.scene = "assets/Scenes/main.scene.asset";
    launch.cli.subject = "live";
    launch.cli.out_path = out.string().c_str();
    SceneInstance scene;
    MachineAdapterHost host;
    host.live_scene = &scene;
    host.capture_override = [](const CaptureRequest&) {
      CaptureResult still;
      still.ok = true;
      still.width = 2;
      still.height = 2;
      still.rgba = {255, 0, 0, 255, 0, 255, 0, 255,
                    0, 0, 255, 255, 255, 255, 255, 255};
      return still;
    };
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("capture ok", result.ok);
    expect_true("capture file", fs::is_regular_file(out));
    fs::remove(out, ec);
  }

  {
    EditorSessionLaunch launch;
    launch.ok = true;
    launch.headless = true;
    launch.adapter = MachineAdapterKind::mcp;
    launch.scene = "assets/Scenes/main.scene.asset";
    launch.cli.verb = "play-frame";
    PlaySessionController play;
    MachineAdapterHost host;
    host.play = &play;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("mcp frame stopped", !result.ok);
    expect_true("mcp frame code",
                result.failure_code == k_request_play_not_playing);
  }

  {
    EditorSessionLaunch session;
    session.ok = true;
    session.headless = true;
    session.adapter = MachineAdapterKind::mcp;
    session.scene = "assets/Scenes/main.scene.asset";
    session.project_root = "C:/Games/Demo";
    MachineAdapterHost host;
    const std::string init = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}",
        session, host);
    expect_true("mcp initialize",
                init.find("blunder-editor") != std::string::npos);
    const std::string listed = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}", session,
        host);
    expect_true("mcp tools play-frame",
                listed.find("play-frame") != std::string::npos);

    SceneInstance scene;
    DocumentHistory history;
    AuthorshipSystem authorship;
    authorship.setTestLiveDocument(&scene, &history);
    const EntityId id = scene.createEntity(
        "Player", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1, 1, 1));
    expect_true("mcp query entity created", isValid(id));
    host.authorship = &authorship;
    host.live_scene = &scene;
    const std::string queried = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{"
        "\"name\":\"query\",\"arguments\":{\"subject\":\"live\",\"name\":"
        "\"Player\"}}}",
        session, host);
    expect_true("mcp query entity name",
                queried.find("Player") != std::string::npos);
    expect_true("mcp query not error",
                queried.find("\"isError\":false") != std::string::npos);

    host.capture_override = [](const CaptureRequest&) {
      CaptureResult still;
      still.ok = true;
      still.width = 2;
      still.height = 2;
      still.rgba = {255, 0, 0, 255, 0, 255, 0, 255,
                    0, 0, 255, 255, 255, 255, 255, 255};
      return still;
    };
    const std::string captured = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{"
        "\"name\":\"capture\",\"arguments\":{\"subject\":\"live\"}}}",
        session, host);
    expect_true("mcp capture image",
                captured.find("\"mimeType\":\"image/png\"") != std::string::npos);
    expect_true("mcp capture not path still",
                captured.find("\"out\"") == std::string::npos);
  }

  {
    EditorSessionLaunch launch = cliLaunch("play-frame");
    MachineAdapterHost host;
    PlaySessionController play;
    host.play = &play;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("play-frame no scene", !result.ok);
    expect_true("play-frame no scene code",
                result.failure_code == k_request_launch_scene_required);
  }

  {
    EditorSessionLaunch launch = cliLaunch("capture");
    launch.scene = "assets/Scenes/main.scene.asset";
    launch.cli.subject = "live";
    SceneInstance scene;
    MachineAdapterHost host;
    host.live_scene = &scene;
    host.capture_override = [](const CaptureRequest&) {
      CaptureResult still;
      still.ok = true;
      still.width = 2;
      still.height = 2;
      still.rgba = {255, 0, 0, 255, 0, 255, 0, 255,
                    0, 0, 255, 255, 255, 255, 255, 255};
      return still;
    };
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("capture missing out", !result.ok);
    expect_true("capture missing out code",
                result.failure_code == k_request_cli_out_required);
  }

  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
