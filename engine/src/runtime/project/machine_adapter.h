#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/editor/authorship_system.h"
#include "runtime/function/render/scene_thumbnail/capture.h"
#include "runtime/project/authorship_issue.h"
#include "runtime/project/editor_launch.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Blunder {

class EditorCamera;
class EditorSceneEditSystem;
class FileSystem;
class PhysicsManager;
class PlaySessionController;
class SceneInstance;
class SceneThumbnailRenderService;

inline constexpr const char* k_request_launch_scene_required =
    "launch.scene_required";
inline constexpr const char* k_request_cli_out_required = "cli.out_required";
inline constexpr const char* k_request_cli_save_required = "cli.save_required";
inline constexpr const char* k_request_cli_subject_required =
    "cli.subject_required";
inline constexpr const char* k_request_cli_save_unsupported =
    "cli.save_unsupported";
inline constexpr const char* k_request_play_not_playing = "play.not_playing";
inline constexpr const char* k_request_viewport_no_camera = "viewport.no_camera";
inline constexpr const char* k_request_viewport_lookat_required =
    "viewport.lookat_required";

struct MachineAdapterHost {
  std::filesystem::path project_root;
  AuthorshipSystem* authorship{nullptr};
  FileSystem* file_system{nullptr};
  SceneInstance* live_scene{nullptr};
  PlaySessionController* play{nullptr};
  SceneThumbnailRenderService* thumbs{nullptr};
  EditorSceneEditSystem* scene_edit{nullptr};
  EditorCamera* editor_camera{nullptr};
  PhysicsManager* physics{nullptr};
  std::function<void()> pump;
  std::function<CaptureResult(const CaptureRequest&)> capture_override;
  std::function<bool()> save_live;
};

struct MachineResult {
  bool ok{false};
  eastl::string failure_code;
  eastl::vector<Issue> issues;
  eastl::vector<eastl::string> names;
  AuthorshipEntityQuery entity{};
  bool has_entity{false};
  uint32_t width{0};
  uint32_t height{0};
  eastl::vector<uint8_t> png;
  eastl::string out_path;
  bool has_camera{false};
  Vec3 camera_eye{0.0f};
  Vec3 camera_target{0.0f};
  Vec3 camera_up{0.0f, 0.0f, 1.0f};
  Vec3 camera_forward{0.0f, 1.0f, 0.0f};
  float camera_yaw{0.0f};
  float camera_pitch{0.0f};
  float camera_distance{0.0f};
  float camera_fov{0.0f};
  float camera_near{0.0f};
  float camera_far{0.0f};
  bool has_physics_hit{false};
  bool physics_hit{false};
  bool physics_is_area{false};
  float physics_distance{0.0f};
  Vec3 physics_point{0.0f};
  Vec3 physics_normal{0.0f, 0.0f, 1.0f};
  eastl::string physics_groups;
  eastl::string collider_shape;
  eastl::string collider_body;
  int exit_code{1};
};

bool encodeRgbaToPngFile(const uint8_t* rgba, uint32_t width, uint32_t height,
                         const std::filesystem::path& path);
bool encodeRgbaToPngBytes(const uint8_t* rgba, uint32_t width, uint32_t height,
                          eastl::vector<uint8_t>& out_png);
std::string base64Encode(const uint8_t* data, size_t size);
std::string machineResultJson(const MachineResult& result);
void dispatchMachineAdapter(const EditorSessionLaunch& launch,
                            MachineAdapterHost& host, MachineResult& out);

}  // namespace Blunder
