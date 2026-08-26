#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"

#include <cstdint>

namespace Blunder {

class SceneInstance;
class SceneThumbnailRenderService;

enum class CaptureSubject : uint8_t {
  live = 0,
  onDisk = 1,
};

inline constexpr const char* k_request_capture_no_camera = "capture.no_camera";
inline constexpr const char* k_request_capture_no_live_document =
    "capture.no_live_document";
inline constexpr const char* k_request_capture_scene_unreadable =
    "capture.scene_unreadable";

struct CaptureRequest {
  CaptureSubject subject{CaptureSubject::live};
  SceneInstance* live_scene{nullptr};
  eastl::string scene_virtual_path;
  /// Capture never writes the Content Browser Thumbnail cache.
  bool write_cache{false};
};

struct CaptureResult {
  bool ok{false};
  eastl::string failure_code;
  uint32_t width{0};
  uint32_t height{0};
  eastl::vector<uint8_t> rgba;
  MeshPreviewCameraFrame framing{};
  bool wrote_cache{false};
};

/// 16:9 Scene still beside Authorship (not Query / Op / Diagnose).
CaptureResult captureScene(SceneThumbnailRenderService& service,
                           const CaptureRequest& request);

}  // namespace Blunder
