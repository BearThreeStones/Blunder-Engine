#include "runtime/function/render/scene_thumbnail/capture.h"

#include "runtime/function/render/scene_thumbnail/scene_thumbnail_render.h"

namespace Blunder {

CaptureResult captureScene(SceneThumbnailRenderService& service,
                           const CaptureRequest& request) {
  CaptureResult out{};
  out.wrote_cache = false;
  if (request.subject == CaptureSubject::live) {
    if (request.live_scene == nullptr) {
      out.failure_code = k_request_capture_no_live_document;
      return out;
    }
  } else if (request.scene_virtual_path.empty()) {
    out.failure_code = k_request_capture_scene_unreadable;
    return out;
  }

  const SceneStillExtent extent = captureStillExtent();
  SceneStillRequest still{};
  still.width = extent.width;
  still.height = extent.height;
  still.require_mesh = false;
  if (request.subject == CaptureSubject::live) {
    still.live_instance = request.live_scene;
  } else {
    still.scene_virtual_path = request.scene_virtual_path;
  }

  const SceneThumbnailRenderResult still_result = service.renderSceneStill(still);
  out.width = still_result.width;
  out.height = still_result.height;
  out.framing = still_result.framing;
  out.rgba = still_result.rgba;
  out.wrote_cache = false;
  if (!still_result.ok) {
    if (!still_result.failure_code.empty()) {
      out.failure_code = still_result.failure_code;
    } else if (still_result.error == "No camera in scene") {
      out.failure_code = k_request_capture_no_camera;
    } else {
      out.failure_code = k_request_capture_scene_unreadable;
    }
    out.rgba.clear();
    return out;
  }
  out.ok = true;
  return out;
}

}  // namespace Blunder
