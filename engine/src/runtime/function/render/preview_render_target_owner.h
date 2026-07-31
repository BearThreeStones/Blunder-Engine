#pragma once

namespace Blunder {

/// Logical owner of an editor offscreen target. Owners must never share target
/// instances because their sizes, render cadence, and readback lifetimes differ.
enum class PreviewRenderTargetOwner {
  MainViewport,
  CameraPreview,
  MeshPreview,
};

}  // namespace Blunder
