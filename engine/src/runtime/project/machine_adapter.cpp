#include "runtime/project/machine_adapter.h"

#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/physics/physics_world.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/collision_overlay_still.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"
#include "runtime/function/render/scene_thumbnail/scene_thumbnail_render.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/play_pose_preview.h"
#include "runtime/project/play_session_controller.h"
#include "runtime/project/play_step.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include <stb_image_write.h>

namespace Blunder {

namespace {

struct PngWriteBuffer {
  eastl::vector<uint8_t> bytes;
};

void pngWriteCallback(void* context, void* data, int size) {
  if (context == nullptr || data == nullptr || size <= 0) {
    return;
  }
  auto* buffer = static_cast<PngWriteBuffer*>(context);
  const size_t offset = buffer->bytes.size();
  buffer->bytes.resize(offset + static_cast<size_t>(size));
  std::memcpy(buffer->bytes.data() + offset, data, static_cast<size_t>(size));
}

void fail(MachineResult& out, const char* code) {
  out.ok = false;
  out.failure_code = code;
  out.exit_code = 1;
}

void succeed(MachineResult& out) {
  out.ok = true;
  out.failure_code.clear();
  out.exit_code = 0;
}

bool parseSubject(const eastl::string& text, AuthorshipSubject& out) {
  if (text == "live") {
    out = AuthorshipSubject::live;
    return true;
  }
  if (text == "on-disk" || text == "ondisk" || text == "on_disk") {
    out = AuthorshipSubject::onDisk;
    return true;
  }
  return false;
}

eastl::string onDiskScenePath(const EditorSessionLaunch& launch) {
  if (!launch.cli.asset.empty()) {
    return launch.cli.asset;
  }
  return launch.scene;
}

const char* severityName(IssueSeverity severity) {
  switch (severity) {
    case IssueSeverity::log:
      return "Log";
    case IssueSeverity::warning:
      return "Warning";
    case IssueSeverity::error:
      return "Error";
  }
  return "Error";
}

void jsonAppendEscaped(std::string& out, const eastl::string& text) {
  out.push_back('"');
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  out.push_back('"');
}

void jsonAppendEscaped(std::string& out, const char* text) {
  jsonAppendEscaped(out, eastl::string(text ? text : ""));
}

void applyAuthorshipStatus(const AuthorshipStatus& status, MachineResult& out) {
  if (!status.ok) {
    fail(out, status.failure_code.empty() ? "request.failed"
                                          : status.failure_code.c_str());
    return;
  }
  succeed(out);
}

bool writeStill(const CaptureResult& still, const EditorSessionLaunch& launch,
                MachineResult& out) {
  if (!still.ok) {
    fail(out, still.failure_code.empty() ? k_request_capture_scene_unreadable
                                         : still.failure_code.c_str());
    return false;
  }
  out.width = still.width;
  out.height = still.height;
  if (still.rgba.empty() || still.width == 0 || still.height == 0) {
    fail(out, k_request_capture_scene_unreadable);
    return false;
  }
  if (!encodeRgbaToPngBytes(still.rgba.data(), still.width, still.height,
                            out.png)) {
    fail(out, "capture.png_encode_failed");
    return false;
  }
  if (launch.adapter == MachineAdapterKind::cli) {
    if (launch.cli.out_path.empty()) {
      fail(out, k_request_cli_out_required);
      out.png.clear();
      return false;
    }
    const std::filesystem::path path(launch.cli.out_path.c_str());
    if (!encodeRgbaToPngFile(still.rgba.data(), still.width, still.height,
                             path)) {
      fail(out, "cli.out_write_failed");
      return false;
    }
    out.out_path = launch.cli.out_path;
  }
  succeed(out);
  return true;
}

void refreshPlayObservation(MachineAdapterHost& host);
void paintCollisionObservation(MachineAdapterHost& host, CaptureResult& still,
                               const MeshPreviewCameraFrame& framing_hint);
CaptureResult captureLiveViewport(MachineAdapterHost& host,
                                  const CaptureRequest& req);

CaptureResult runCapture(MachineAdapterHost& host, const CaptureRequest& req) {
  if (host.capture_override) {
    return host.capture_override(req);
  }
  refreshPlayObservation(host);
  if (req.subject == CaptureSubject::live && req.live_scene != nullptr) {
    CaptureResult viewport = captureLiveViewport(host, req);
    if (viewport.ok) {
      paintCollisionObservation(host, viewport, req.framing_override);
      return viewport;
    }
  }
  CaptureResult still;
  if (host.thumbs != nullptr) {
    still = captureScene(*host.thumbs, req);
  }
  if (!still.ok && req.subject == CaptureSubject::live &&
      req.live_scene != nullptr) {
    still = {};
    still.framing = req.framing_override.ok ? req.framing_override
                                            : defaultCollisionCaptureFraming();
    ensureCollisionStillBuffer(still.rgba, still.width, still.height);
    still.ok = true;
  }
  if (req.subject == CaptureSubject::live && req.live_scene != nullptr) {
    paintCollisionObservation(host, still, req.framing_override);
  }
  if (!still.ok && still.failure_code.empty()) {
    still.failure_code = k_request_capture_scene_unreadable;
  }
  return still;
}

void pumpHost(MachineAdapterHost& host) {
  if (host.pump) {
    host.pump();
  }
}

const PlayPoseOverlayMap* hostPlayPoseOverlay(MachineAdapterHost& host) {
  if (host.play == nullptr) {
    return nullptr;
  }
  if (host.play->state() != PlaySessionState::Playing &&
      host.play->state() != PlaySessionState::Paused) {
    return nullptr;
  }
  if (host.play->poseOverlay().empty()) {
    return nullptr;
  }
  return &host.play->poseOverlay();
}

void refreshPlayObservation(MachineAdapterHost& host) {
  if (host.play != nullptr) {
    host.play->poll();
  }
  pumpHost(host);
}

void waitPlayPoses(MachineAdapterHost& host, PlaySessionController& play,
                   int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms < 0 ? 0 : timeout_ms);
  for (;;) {
    play.poll();
    pumpHost(host);
    if (!play.poseOverlay().empty()) {
      return;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void paintCollisionObservation(MachineAdapterHost& host, CaptureResult& still,
                               const MeshPreviewCameraFrame& framing_hint) {
  MeshPreviewCameraFrame framing = still.framing;
  if (!framing.ok) {
    framing = framing_hint.ok ? framing_hint : defaultCollisionCaptureFraming();
  }
  still.framing = framing;
  ensureCollisionStillBuffer(still.rgba, still.width, still.height);
  SceneInstance* scene = host.live_scene;
  if (scene == nullptr) {
    still.ok = !still.rgba.empty() && still.width > 0 && still.height > 0;
    return;
  }
  paintCollisionOverlayRgba(still.rgba.data(), still.width, still.height, framing,
                            *scene, hostPlayPoseOverlay(host));
  still.ok = !still.rgba.empty() && still.width > 0 && still.height > 0;
  if (still.ok) {
    still.failure_code.clear();
  }
}

CaptureResult captureLiveViewport(MachineAdapterHost& host,
                                  const CaptureRequest& req) {
  CaptureResult out{};
  if (g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->requestViewportRedraw();
  }
  for (int i = 0; i < 3; ++i) {
    pumpHost(host);
  }
  if (!g_runtime_global_context.m_render_system) {
    return out;
  }
  eastl::vector<uint8_t> src;
  uint32_t src_w = 0;
  uint32_t src_h = 0;
  if (!g_runtime_global_context.m_render_system->readbackOffscreenRgba(src, src_w,
                                                                      src_h)) {
    return out;
  }
  SceneStillExtent extent{};
  if (!fitRgbaToSceneStill(src.data(), src_w, src_h, k_capture_aspect_w,
                           k_capture_aspect_h, k_capture_longest_edge, out.rgba,
                           extent)) {
    return out;
  }
  out.width = extent.width;
  out.height = extent.height;
  out.framing = req.framing_override;
  out.ok = !out.rgba.empty();
  return out;
}

bool waitPlayReady(MachineAdapterHost& host, PlaySessionController& play,
                   int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms < 0 ? 0 : timeout_ms);
  for (;;) {
    play.poll();
    pumpHost(host);
    if (play.state() == PlaySessionState::Playing ||
        play.state() == PlaySessionState::Paused) {
      return true;
    }
    if (play.state() == PlaySessionState::Stopped) {
      return false;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

MeshPreviewCameraFrame frameFromEditorCamera(const EditorCamera& camera) {
  MeshPreviewCameraFrame framing{};
  framing.eye = camera.getPosition();
  framing.target = camera.getFocalPoint();
  framing.up = camera.getUpDirection();
  framing.vertical_fov_rad = camera.getVerticalFov();
  framing.ok = framing.vertical_fov_rad > 0.0f;
  return framing;
}

void fillCameraResult(const EditorCamera& camera, MachineResult& out) {
  out.has_camera = true;
  out.camera_eye = camera.getPosition();
  out.camera_target = camera.getFocalPoint();
  out.camera_up = camera.getUpDirection();
  out.camera_forward = camera.getForwardDirection();
  out.camera_yaw = camera.getYaw();
  out.camera_pitch = camera.getPitch();
  out.camera_distance = camera.getDistance();
  out.camera_fov = camera.getVerticalFov();
  out.camera_near = camera.getNearClip();
  out.camera_far = camera.getFarClip();
}

void jsonAppendVec3(std::string& out, const Vec3& v) {
  out += '[';
  out += std::to_string(v.x);
  out += ',';
  out += std::to_string(v.y);
  out += ',';
  out += std::to_string(v.z);
  out += ']';
}

bool dispatchViewportCamera(const EditorSessionLaunch& launch,
                            MachineAdapterHost& host, MachineResult& out) {
  const eastl::string& verb = launch.cli.verb;
  if (verb != "get-camera" && verb != "set-camera" && verb != "orbit" &&
      verb != "orbit-camera" && verb != "pan" && verb != "zoom") {
    return false;
  }
  if (host.editor_camera == nullptr) {
    fail(out, k_request_viewport_no_camera);
    return true;
  }
  EditorCamera& camera = *host.editor_camera;
  if (verb == "set-camera") {
    if (!launch.cli.has_eye || !launch.cli.has_target) {
      fail(out, k_request_viewport_lookat_required);
      return true;
    }
    camera.snapLookAt(
        Vec3(launch.cli.eye_x, launch.cli.eye_y, launch.cli.eye_z),
        Vec3(launch.cli.target_x, launch.cli.target_y, launch.cli.target_z));
  } else if (verb == "orbit") {
    camera.orbitAroundWorldOrigin(Vec2(launch.cli.dx, launch.cli.dy));
  } else if (verb == "orbit-camera") {
    camera.orbitAroundCamera(Vec2(launch.cli.dx, launch.cli.dy));
  } else if (verb == "pan") {
    camera.panByMouseDelta(Vec2(launch.cli.dx, launch.cli.dy));
  } else if (verb == "zoom") {
    camera.zoomByWheel(launch.cli.wheel);
  }
  fillCameraResult(camera, out);
  succeed(out);
  return true;
}

bool copyPlayFramePng(PlaySessionController& play, const EditorSessionLaunch& launch,
                      MachineAdapterHost& host, MachineResult& out) {
  refreshPlayObservation(host);
  CaptureResult still;
  const PlayIpcFrameRecord& frame = play.lastPlayFrame();
  if (frame.width != 0 && frame.height != 0 && !frame.rgba.empty()) {
    still.ok = true;
    still.width = frame.width;
    still.height = frame.height;
    still.rgba.resize(frame.rgba.size());
    if (!frame.rgba.empty()) {
      std::memcpy(still.rgba.data(), frame.rgba.data(), frame.rgba.size());
    }
  }
  MeshPreviewCameraFrame framing{};
  if (host.editor_camera != nullptr) {
    framing = frameFromEditorCamera(*host.editor_camera);
  }
  paintCollisionObservation(host, still, framing);
  return writeStill(still, launch, out);
}

}  // namespace

bool encodeRgbaToPngBytes(const uint8_t* rgba, uint32_t width, uint32_t height,
                          eastl::vector<uint8_t>& out_png) {
  out_png.clear();
  if (rgba == nullptr || width == 0 || height == 0) {
    return false;
  }
  PngWriteBuffer buffer;
  const int ok = stbi_write_png_to_func(
      pngWriteCallback, &buffer, static_cast<int>(width),
      static_cast<int>(height), 4, rgba, static_cast<int>(width * 4));
  if (ok == 0) {
    return false;
  }
  out_png = std::move(buffer.bytes);
  return !out_png.empty();
}

bool encodeRgbaToPngFile(const uint8_t* rgba, uint32_t width, uint32_t height,
                         const std::filesystem::path& path) {
  eastl::vector<uint8_t> png;
  if (!encodeRgbaToPngBytes(rgba, width, height, png)) {
    return false;
  }
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
  }
  FILE* file = nullptr;
#if defined(_MSC_VER)
  fopen_s(&file, path.string().c_str(), "wb");
#else
  file = std::fopen(path.string().c_str(), "wb");
#endif
  if (file == nullptr) {
    return false;
  }
  const size_t wrote = std::fwrite(png.data(), 1, png.size(), file);
  std::fclose(file);
  return wrote == png.size();
}

std::string base64Encode(const uint8_t* data, size_t size) {
  static const char k_table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  if (data == nullptr || size == 0) {
    return out;
  }
  out.reserve(((size + 2) / 3) * 4);
  size_t i = 0;
  while (i + 3 <= size) {
    const uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) |
                       uint32_t(data[i + 2]);
    out.push_back(k_table[(n >> 18) & 63]);
    out.push_back(k_table[(n >> 12) & 63]);
    out.push_back(k_table[(n >> 6) & 63]);
    out.push_back(k_table[n & 63]);
    i += 3;
  }
  if (i < size) {
    uint32_t n = uint32_t(data[i]) << 16;
    if (i + 1 < size) {
      n |= uint32_t(data[i + 1]) << 8;
    }
    out.push_back(k_table[(n >> 18) & 63]);
    out.push_back(k_table[(n >> 12) & 63]);
    if (i + 1 < size) {
      out.push_back(k_table[(n >> 6) & 63]);
    } else {
      out.push_back('=');
    }
    out.push_back('=');
  }
  return out;
}

std::string machineResultJson(const MachineResult& result) {
  std::string out = "{\"ok\":";
  out += result.ok ? "true" : "false";
  out += ",\"failure_code\":";
  jsonAppendEscaped(out, result.failure_code);
  out += ",\"exit_code\":";
  out += std::to_string(result.exit_code);
  out += ",\"issues\":[";
  for (size_t i = 0; i < result.issues.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    const Issue& issue = result.issues[i];
    out += "{\"code\":";
    jsonAppendEscaped(out, issue.code);
    out += ",\"severity\":";
    jsonAppendEscaped(out, severityName(issue.severity));
    out += ",\"address\":";
    jsonAppendEscaped(out, issue.address);
    out += ",\"explanation\":";
    jsonAppendEscaped(out, issue.explanation);
    out += '}';
  }
  out += "],\"names\":[";
  for (size_t i = 0; i < result.names.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    jsonAppendEscaped(out, result.names[i]);
  }
  out += ']';
  if (result.has_entity) {
    out += ",\"entity\":{\"name\":";
    jsonAppendEscaped(out, result.entity.name);
    out += ",\"parent\":";
    jsonAppendEscaped(out, result.entity.parent_name);
    out += ",\"position\":[";
    out += std::to_string(result.entity.position.x);
    out += ',';
    out += std::to_string(result.entity.position.y);
    out += ',';
    out += std::to_string(result.entity.position.z);
    out += "],\"rotation\":[";
    out += std::to_string(result.entity.rotation.x);
    out += ',';
    out += std::to_string(result.entity.rotation.y);
    out += ',';
    out += std::to_string(result.entity.rotation.z);
    out += ',';
    out += std::to_string(result.entity.rotation.w);
    out += "],\"scale\":[";
    out += std::to_string(result.entity.scale.x);
    out += ',';
    out += std::to_string(result.entity.scale.y);
    out += ',';
    out += std::to_string(result.entity.scale.z);
    out += "]}";
  }
  if (!result.out_path.empty()) {
    out += ",\"out\":";
    jsonAppendEscaped(out, result.out_path);
  }
  if (result.has_camera) {
    out += ",\"camera\":{\"eye\":";
    jsonAppendVec3(out, result.camera_eye);
    out += ",\"target\":";
    jsonAppendVec3(out, result.camera_target);
    out += ",\"up\":";
    jsonAppendVec3(out, result.camera_up);
    out += ",\"forward\":";
    jsonAppendVec3(out, result.camera_forward);
    out += ",\"yaw\":";
    out += std::to_string(result.camera_yaw);
    out += ",\"pitch\":";
    out += std::to_string(result.camera_pitch);
    out += ",\"distance\":";
    out += std::to_string(result.camera_distance);
    out += ",\"fov\":";
    out += std::to_string(result.camera_fov);
    out += ",\"near\":";
    out += std::to_string(result.camera_near);
    out += ",\"far\":";
    out += std::to_string(result.camera_far);
    out += '}';
  }
  if (result.has_physics_hit) {
    out += ",\"hit\":";
    out += result.physics_hit ? "true" : "false";
    out += ",\"is_area\":";
    out += result.physics_is_area ? "true" : "false";
    out += ",\"distance\":";
    out += std::to_string(result.physics_distance);
    out += ",\"point\":";
    jsonAppendVec3(out, result.physics_point);
    out += ",\"normal\":";
    jsonAppendVec3(out, result.physics_normal);
    out += ",\"groups\":";
    jsonAppendEscaped(out, result.physics_groups);
  }
  if (!result.collider_shape.empty() || !result.collider_body.empty()) {
    out += ",\"collider\":{\"shape\":";
    jsonAppendEscaped(out, result.collider_shape);
    out += ",\"body\":";
    jsonAppendEscaped(out, result.collider_body);
    out += '}';
  }
  out += ",\"width\":";
  out += std::to_string(result.width);
  out += ",\"height\":";
  out += std::to_string(result.height);
  out += '}';
  return out;
}

void dispatchMachineAdapter(const EditorSessionLaunch& launch,
                            MachineAdapterHost& host, MachineResult& out) {
  out = {};
  const eastl::string& verb = launch.cli.verb;
  if (verb.empty()) {
    fail(out, "cli.verb_required");
    return;
  }
  if (dispatchViewportCamera(launch, host, out)) {
    return;
  }
  if (verb == "ray" || verb == "shapecast" || verb == "group" ||
      verb == "collider") {
    SceneInstance* scene = host.live_scene;
    if (scene == nullptr) {
      fail(out, k_request_subject_no_live_document);
      return;
    }
    if (verb == "ray") {
      PhysicsManager* physics = host.physics;
      if (physics == nullptr) {
        physics = g_runtime_global_context.m_physics_manager.get();
      }
      if (physics == nullptr) {
        fail(out, "physics.unavailable");
        return;
      }
      Vec3 origin(launch.cli.ox, launch.cli.oy, launch.cli.oz);
      Vec3 direction(launch.cli.dx, launch.cli.dy, launch.cli.dz);
      if (glm::length(direction) < 1e-6f) {
        direction = Vec3(0.0f, 0.0f, -1.0f);
      }
      PhysicsSceneHit hit{};
      const bool ok = physics->raycast(
          *scene, origin, direction, launch.cli.max_distance, launch.cli.mask,
          launch.cli.collide_with_areas, hit);
      out.has_physics_hit = true;
      out.physics_hit = ok && hit.hit;
      out.physics_is_area = hit.is_area;
      out.physics_distance = hit.distance;
      out.physics_point = hit.point;
      out.physics_normal = hit.normal;
      for (size_t i = 0; i < hit.groups.size(); ++i) {
        if (i != 0) {
          out.physics_groups.append(",");
        }
        out.physics_groups.append(hit.groups[i]);
      }
      if (out.physics_hit) {
        succeed(out);
      } else {
        fail(out, "physics.miss");
      }
      return;
    }
    if (verb == "shapecast") {
      PhysicsManager* physics = host.physics;
      if (physics == nullptr) {
        physics = g_runtime_global_context.m_physics_manager.get();
      }
      if (physics == nullptr) {
        fail(out, "physics.unavailable");
        return;
      }
      PhysicsSweepShape sweep = PhysicsSweepShape::Sphere;
      if (launch.cli.sweep_shape == "box") {
        sweep = PhysicsSweepShape::Box;
      } else if (launch.cli.sweep_shape == "capsule") {
        sweep = PhysicsSweepShape::Capsule;
      } else if (!launch.cli.sweep_shape.empty() &&
                 launch.cli.sweep_shape != "sphere") {
        fail(out, "physics.sweep_shape");
        return;
      }
      Vec3 origin(launch.cli.ox, launch.cli.oy, launch.cli.oz);
      Vec3 direction(launch.cli.dx, launch.cli.dy, launch.cli.dz);
      if (glm::length(direction) < 1e-6f) {
        direction = Vec3(0.0f, 0.0f, -1.0f);
      }
      Quat rotation(launch.cli.qw, launch.cli.qx, launch.cli.qy, launch.cli.qz);
      PhysicsSceneHit hit{};
      const bool ok = physics->shapecast(
          *scene, sweep, origin, rotation,
          Vec3(launch.cli.hx, launch.cli.hy, launch.cli.hz),
          launch.cli.sphere_radius, launch.cli.capsule_radius,
          launch.cli.capsule_half_height, direction, launch.cli.max_distance,
          launch.cli.mask, launch.cli.collide_with_areas, hit);
      out.has_physics_hit = true;
      out.physics_hit = ok && hit.hit;
      out.physics_is_area = hit.is_area;
      out.physics_distance = hit.distance;
      out.physics_point = hit.point;
      out.physics_normal = hit.normal;
      for (size_t i = 0; i < hit.groups.size(); ++i) {
        if (i != 0) {
          out.physics_groups.append(",");
        }
        out.physics_groups.append(hit.groups[i]);
      }
      if (out.physics_hit) {
        succeed(out);
      } else {
        fail(out, "physics.miss");
      }
      return;
    }
    if (verb == "group") {
      if (launch.cli.entity.empty()) {
        fail(out, k_request_address_unknown);
        return;
      }
      scene->forEachEntity([&](EntityId id, const Entity&) {
        if (scene->isInGroup(id, launch.cli.entity)) {
          if (const Entity* entity = scene->getEntity(id)) {
            out.names.push_back(entity->getName());
          }
        }
      });
      succeed(out);
      return;
    }
    if (launch.cli.entity.empty()) {
      fail(out, k_request_address_unknown);
      return;
    }
    const EntityId id = scene->findEntityByName(launch.cli.entity);
    const ColliderComponent* collider =
        isValid(id) ? scene->getCollider(id) : nullptr;
    if (collider == nullptr) {
      fail(out, "collider.missing");
      return;
    }
    out.collider_shape = colliderShapeKindJson(collider->shape);
    out.collider_body = colliderBodyKindJson(collider->body_kind);
    succeed(out);
    return;
  }
  if (verb == "save") {
    if (launch.adapter == MachineAdapterKind::cli) {
      fail(out, k_request_cli_save_unsupported);
      return;
    }
    if (launch.scene.empty() || host.live_scene == nullptr) {
      fail(out, k_request_subject_no_live_document);
      return;
    }
    bool saved = false;
    if (host.save_live) {
      saved = host.save_live();
    } else if (host.scene_edit != nullptr) {
      saved = host.scene_edit->saveActiveScene();
    }
    if (!saved) {
      fail(out, "save.failed");
      return;
    }
    succeed(out);
    return;
  }

  if (verb == "query" || verb == "diagnose" || verb == "capture" ||
      verb == "op") {
    AuthorshipSubject subject = AuthorshipSubject::live;
    if (verb != "op") {
      if (!parseSubject(launch.cli.subject, subject)) {
        fail(out, k_request_cli_subject_required);
        return;
      }
    } else {
      subject = AuthorshipSubject::live;
    }
    if (subject == AuthorshipSubject::live &&
        (launch.scene.empty() ||
         (host.live_scene == nullptr && host.authorship == nullptr))) {
      fail(out, k_request_subject_no_live_document);
      return;
    }
    if (subject == AuthorshipSubject::live && host.live_scene == nullptr &&
        host.authorship != nullptr) {
      // Authorship System may still have a Live document from Scene Edit.
    }
    if (verb == "op") {
      if (launch.adapter == MachineAdapterKind::cli && !launch.cli.save) {
        fail(out, k_request_cli_save_required);
        return;
      }
      if (host.authorship == nullptr) {
        fail(out, k_request_subject_no_live_document);
        return;
      }
      if (launch.cli.entity.empty()) {
        fail(out, k_request_address_unknown);
        return;
      }
      const AuthorshipStatus status = host.authorship->setTransform(
          AuthorshipSubject::live, launch.cli.entity,
          Vec3(launch.cli.tx, launch.cli.ty, launch.cli.tz),
          Quat(launch.cli.qw, launch.cli.qx, launch.cli.qy, launch.cli.qz),
          Vec3(launch.cli.sx, launch.cli.sy, launch.cli.sz));
      applyAuthorshipStatus(status, out);
      if (!out.ok) {
        return;
      }
      if (launch.adapter == MachineAdapterKind::cli) {
        bool saved = false;
        if (host.save_live) {
          saved = host.save_live();
        } else if (host.scene_edit != nullptr) {
          saved = host.scene_edit->saveActiveScene();
        }
        if (!saved) {
          fail(out, "save.failed");
        }
      }
      return;
    }
    const eastl::string disk_path = onDiskScenePath(launch);
    if (verb == "query") {
      if (host.authorship == nullptr) {
        fail(out, k_request_subject_no_live_document);
        return;
      }
      if (launch.cli.entity.empty()) {
        const AuthorshipStatus status =
            host.authorship->queryNames(subject, disk_path, out.names);
        applyAuthorshipStatus(status, out);
        return;
      }
      const AuthorshipStatus status = host.authorship->queryEntity(
          subject, disk_path, launch.cli.entity, out.entity);
      applyAuthorshipStatus(status, out);
      if (out.ok) {
        out.has_entity = true;
        if (subject == AuthorshipSubject::live) {
          refreshPlayObservation(host);
          if (const PlayPoseOverlayMap* overlay = hostPlayPoseOverlay(host)) {
            applyNamedPlayPose(*overlay, out.entity.name, out.entity.position,
                               out.entity.rotation, out.entity.scale);
          }
        }
      }
      return;
    }
    if (verb == "diagnose") {
      if (host.authorship == nullptr) {
        fail(out, k_request_subject_no_live_document);
        return;
      }
      const AuthorshipStatus status =
          host.authorship->diagnosePlay(subject, disk_path, out.issues);
      if (!status.ok) {
        applyAuthorshipStatus(status, out);
        return;
      }
      succeed(out);
      return;
    }
    if (verb == "capture") {
      CaptureRequest req;
      req.subject = subject == AuthorshipSubject::live ? CaptureSubject::live
                                                       : CaptureSubject::onDisk;
      req.live_scene = host.live_scene;
      req.scene_virtual_path = disk_path;
      if (req.subject == CaptureSubject::live && req.live_scene == nullptr) {
        fail(out, k_request_subject_no_live_document);
        return;
      }
      if (req.subject == CaptureSubject::live && host.editor_camera != nullptr &&
          launch.adapter == MachineAdapterKind::mcp) {
        req.override_framing = true;
        req.framing_override = frameFromEditorCamera(*host.editor_camera);
      }
      writeStill(runCapture(host, req), launch, out);
      return;
    }
  }

  if (verb == "play" || verb == "pause" || verb == "resume" || verb == "stop" ||
      verb == "step" || verb == "play-frame") {
    if (launch.scene.empty()) {
      fail(out, k_request_launch_scene_required);
      return;
    }
    if (host.play == nullptr) {
      fail(out, k_request_play_not_playing);
      return;
    }
    PlaySessionController& play = *host.play;
    if (verb == "play") {
      PlaySessionRequest req;
      req.project_root = host.project_root;
      req.scene = launch.scene.c_str();
      req.headless = true;
      if (!play.play(req)) {
        out.issues = play.lastIssues();
        fail(out, play.lastRequestFailure().empty()
                      ? (play.lastError().empty() ? "play.failed"
                                                  : play.lastError().c_str())
                      : play.lastRequestFailure().c_str());
        return;
      }
      if (!waitPlayReady(host, play, 15000)) {
        fail(out, "play.start_timeout");
        return;
      }
      waitPlayPoses(host, play, 2000);
      succeed(out);
      return;
    }
    if (verb == "pause") {
      if (!play.pause()) {
        fail(out, k_request_play_not_playing);
        return;
      }
      succeed(out);
      return;
    }
    if (verb == "resume") {
      if (!play.resume()) {
        fail(out, k_request_play_not_playing);
        return;
      }
      succeed(out);
      return;
    }
    if (verb == "stop") {
      play.stop();
      succeed(out);
      return;
    }
    if (verb == "step") {
      if (!play.step(launch.cli.steps == 0 ? 1 : launch.cli.steps)) {
        fail(out, play.lastRequestFailure().empty()
                      ? k_request_play_step_requires_pause
                      : play.lastRequestFailure().c_str());
        return;
      }
      waitPlayPoses(host, play, 2000);
      succeed(out);
      return;
    }
    if (verb == "play-frame") {
      const bool episode = launch.adapter == MachineAdapterKind::cli;
      if (episode) {
        PlaySessionRequest req;
        req.project_root = host.project_root;
        req.scene = launch.scene.c_str();
        req.headless = true;
        if (!play.play(req)) {
          out.issues = play.lastIssues();
          fail(out, play.lastRequestFailure().empty() ? "play.failed"
                                                      : play.lastRequestFailure()
                                                            .c_str());
          return;
        }
        if (!waitPlayReady(host, play, 15000)) {
          play.stop();
          fail(out, "play.start_timeout");
          return;
        }
        if (launch.cli.steps > 0) {
          if (!play.pause() || !play.step(launch.cli.steps)) {
            play.stop();
            fail(out, play.lastRequestFailure().empty()
                          ? k_request_play_step_requires_pause
                          : play.lastRequestFailure().c_str());
            return;
          }
          waitPlayPoses(host, play, 2000);
        }
        if (!play.waitForPlayFrame(5000, [&host]() { pumpHost(host); })) {
          play.stop();
          fail(out, "play.frame_timeout");
          return;
        }
        const bool wrote = copyPlayFramePng(play, launch, host, out);
        play.stop();
        if (!wrote) {
          return;
        }
        succeed(out);
        return;
      }
      if (play.state() != PlaySessionState::Playing &&
          play.state() != PlaySessionState::Paused) {
        fail(out, k_request_play_not_playing);
        return;
      }
      if (!play.waitForPlayFrame(5000, [&host]() { pumpHost(host); })) {
        fail(out, "play.frame_timeout");
        return;
      }
      copyPlayFramePng(play, launch, host, out);
      return;
    }
  }

  fail(out, "cli.unknown_verb");
}

}  // namespace Blunder
