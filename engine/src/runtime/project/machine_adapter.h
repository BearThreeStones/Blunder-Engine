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

class EditorSceneEditSystem;
class FileSystem;
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

struct MachineAdapterHost {
  std::filesystem::path project_root;
  AuthorshipSystem* authorship{nullptr};
  FileSystem* file_system{nullptr};
  SceneInstance* live_scene{nullptr};
  PlaySessionController* play{nullptr};
  SceneThumbnailRenderService* thumbs{nullptr};
  EditorSceneEditSystem* scene_edit{nullptr};
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
