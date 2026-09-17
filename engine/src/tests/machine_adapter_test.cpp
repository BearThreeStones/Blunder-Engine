#include "runtime/core/math/math_types.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/authorship_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/machine_adapter.h"
#include "runtime/project/machine_mcp.h"
#include "runtime/project/play_session_controller.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdio>
#include <cmath>
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
    expect_true("mcp tools get-camera",
                listed.find("get-camera") != std::string::npos);
    expect_true("mcp tools set-camera",
                listed.find("set-camera") != std::string::npos);
    expect_true("mcp tools orbit", listed.find("\"name\":\"orbit\"") !=
                                       std::string::npos);
    expect_true("mcp tools orbit-camera",
                listed.find("orbit-camera") != std::string::npos);
    expect_true("mcp tools pan",
                listed.find("\"name\":\"pan\"") != std::string::npos);
    expect_true("mcp tools zoom",
                listed.find("\"name\":\"zoom\"") != std::string::npos);
    expect_true("mcp tools ray",
                listed.find("\"name\":\"ray\"") != std::string::npos);
    expect_true("mcp tools group",
                listed.find("\"name\":\"group\"") != std::string::npos);
    expect_true("mcp tools collider",
                listed.find("\"name\":\"collider\"") != std::string::npos);

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

  {
    EditorSessionLaunch launch;
    launch.ok = true;
    launch.headless = true;
    launch.adapter = MachineAdapterKind::mcp;
    launch.cli.verb = "get-camera";
    MachineAdapterHost host;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("get-camera no editor camera", !result.ok);
    expect_true("get-camera no camera code",
                result.failure_code == k_request_viewport_no_camera);
  }

  {
    EditorCamera camera(nullptr);
    camera.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);
    camera.snapLookAt(Vec3(20.0f, 8.0f, 6.0f), Vec3(12.0f, 4.0f, 3.0f));
    const Vec3 eye_before = camera.getPosition();
    const float dist_before = glm::length(eye_before);

    EditorSessionLaunch launch;
    launch.ok = true;
    launch.headless = true;
    launch.adapter = MachineAdapterKind::mcp;
    MachineAdapterHost host;
    host.editor_camera = &camera;

    launch.cli.verb = "get-camera";
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("get-camera ok", result.ok);
    expect_true("get-camera has pose", result.has_camera);
    expect_true("get-camera eye",
                glm::length(result.camera_eye - eye_before) < 1e-3f);
    const std::string got = machineResultJson(result);
    expect_true("get-camera json eye", got.find("\"eye\"") != std::string::npos);

    launch.cli.verb = "orbit";
    launch.cli.dx = 60.0f;
    launch.cli.dy = -30.0f;
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("orbit ok", result.ok);
    expect_true("orbit keeps distance to origin",
                std::fabs(glm::length(camera.getPosition()) - dist_before) <
                    1e-2f);
    expect_true("orbit does not snap look-at to origin",
                glm::length(camera.getFocalPoint()) > 1.0f);
    expect_true("orbit moves the eye",
                glm::length(camera.getPosition() - eye_before) > 1e-3f);

    launch.cli.verb = "orbit-camera";
    launch.cli.dx = 20.0f;
    launch.cli.dy = -10.0f;
    const Vec3 rmb_eye = camera.getPosition();
    const Vec3 rmb_focal = camera.getFocalPoint();
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("orbit-camera ok", result.ok);
    expect_true("orbit-camera keeps eye",
                glm::length(camera.getPosition() - rmb_eye) < 1e-3f);
    expect_true("orbit-camera moves target",
                glm::length(camera.getFocalPoint() - rmb_focal) > 1e-3f);

    launch.cli.verb = "pan";
    launch.cli.dx = 30.0f;
    launch.cli.dy = -30.0f;
    const Vec3 pan_focal = camera.getFocalPoint();
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("pan ok", result.ok);
    expect_true("pan moves target",
                glm::length(camera.getFocalPoint() - pan_focal) > 1e-4f);

    launch.cli.verb = "zoom";
    launch.cli.wheel = -8.0f;
    const float zoom_before = camera.getDistance();
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("zoom ok", result.ok);
    expect_true("zoom-out increases distance",
                camera.getDistance() > zoom_before);

    launch.cli.verb = "set-camera";
    launch.cli.has_eye = true;
    launch.cli.has_target = true;
    launch.cli.eye_x = 12.0f;
    launch.cli.eye_y = 12.0f;
    launch.cli.eye_z = 12.0f;
    launch.cli.target_x = 0.0f;
    launch.cli.target_y = 0.0f;
    launch.cli.target_z = 0.0f;
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("set-camera ok", result.ok);
    expect_true("set-camera eye",
                glm::length(camera.getPosition() - Vec3(12.0f, 12.0f, 12.0f)) <
                    1e-2f);
    expect_true("set-camera target",
                glm::length(camera.getFocalPoint()) < 1e-2f);

    launch.cli.verb = "set-camera";
    launch.cli.has_eye = true;
    launch.cli.has_target = false;
    result = {};
    dispatchMachineAdapter(launch, host, result);
    expect_true("set-camera missing target", !result.ok);
    expect_true("set-camera missing target code",
                result.failure_code == k_request_viewport_lookat_required);
  }

  {
    EditorSessionLaunch session;
    session.ok = true;
    session.headless = true;
    session.adapter = MachineAdapterKind::mcp;
    session.scene = "assets/Scenes/sponza.scene.asset";
    session.project_root = "C:/Games/Demo";
    EditorCamera camera(nullptr);
    camera.setViewportRect(0, 0, 1280.0f, 720.0f, 1280.0f, 720.0f);
    camera.snapLookAt(Vec3(20.0f, 8.0f, 6.0f), Vec3(12.0f, 4.0f, 3.0f));
    MachineAdapterHost host;
    host.editor_camera = &camera;
    const std::string orbited = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{"
        "\"name\":\"orbit\",\"arguments\":{\"dx\":40,\"dy\":-20}}}",
        session, host);
    expect_true("mcp orbit not error",
                orbited.find("\"isError\":false") != std::string::npos);
    expect_true("mcp orbit camera json",
                orbited.find("\\\"eye\\\"") != std::string::npos);
    const std::string zoomed = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{"
        "\"name\":\"zoom\",\"arguments\":{\"wheel\":-4}}}",
        session, host);
    expect_true("mcp zoom not error",
                zoomed.find("\"isError\":false") != std::string::npos);
  }

  {
    EditorCamera camera(nullptr);
    camera.snapLookAt(Vec3(5.0f, 5.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f));
    EditorSessionLaunch launch = cliLaunch("capture");
    launch.adapter = MachineAdapterKind::mcp;
    launch.scene = "assets/Scenes/main.scene.asset";
    launch.cli.subject = "live";
    SceneInstance scene;
    MachineAdapterHost host;
    host.live_scene = &scene;
    host.editor_camera = &camera;
    bool used_editor_view = false;
    host.capture_override = [&](const CaptureRequest& req) {
      used_editor_view = req.override_framing && req.framing_override.ok &&
                         glm::length(req.framing_override.eye -
                                     camera.getPosition()) < 1e-3f;
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
    expect_true("mcp live capture uses editor camera", used_editor_view);
    expect_true("mcp live capture ok", result.ok);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("IceArea", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.addGroup(id, "TerrainIce");
    ColliderComponent collider{};
    collider.body_kind = ColliderBodyKind::Area;
    scene.setCollider(id, collider);

    EditorSessionLaunch launch = cliLaunch("group");
    launch.scene = "assets/Scenes/root.scene.asset";
    launch.cli.entity = "TerrainIce";
    MachineAdapterHost host;
    host.live_scene = &scene;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("group ok", result.ok);
    expect_true("group names", result.names.size() == 1 &&
                                   result.names[0] == "IceArea");

    launch.cli.verb = "collider";
    launch.cli.entity = "IceArea";
    dispatchMachineAdapter(launch, host, result);
    expect_true("collider ok", result.ok);
    expect_true("collider body area", result.collider_body == "area");
    expect_true("collider shape box", result.collider_shape == "box");

    launch.cli.verb = "ray";
    launch.cli.oz = 5.0f;
    launch.cli.dz = -1.0f;
    launch.cli.max_distance = 20.0f;
    dispatchMachineAdapter(launch, host, result);
    expect_true("ray without physics host fails", !result.ok);
    expect_true("ray physics code", result.failure_code == "physics.unavailable");
  }

  {
    SceneInstance scene;
    const EntityId floor_id =
        scene.createEntity("Floor", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent floor{};
    floor.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
    scene.setCollider(floor_id, floor);
    scene.addGroup(floor_id, "TerrainIce");

    PhysicsManager physics;
    EditorSessionLaunch launch = cliLaunch("ray");
    launch.cli.oz = 5.0f;
    launch.cli.dz = -1.0f;
    launch.cli.max_distance = 20.0f;
    MachineAdapterHost host;
    host.live_scene = &scene;
    host.physics = &physics;
    MachineResult result;
    dispatchMachineAdapter(launch, host, result);
    expect_true("ray with physics host hits", result.ok && result.physics_hit);
    expect_true("ray groups csv", result.physics_groups.find("TerrainIce") !=
                                      eastl::string::npos);

    EditorSessionLaunch session;
    session.ok = true;
    session.headless = true;
    session.adapter = MachineAdapterKind::mcp;
    session.scene = "assets/Scenes/root.scene.asset";
    const std::string rayed = mcpHandleMessage(
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/call\",\"params\":{"
        "\"name\":\"ray\",\"arguments\":{\"ox\":0,\"oy\":0,\"oz\":5,\"dz\":-1,"
        "\"max_distance\":20}}}",
        session, host);
    expect_true("mcp ray not error",
                rayed.find("\"isError\":false") != std::string::npos);
    expect_true("mcp ray hit json",
                rayed.find("\\\"hit\\\":true") != std::string::npos);
  }

  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
