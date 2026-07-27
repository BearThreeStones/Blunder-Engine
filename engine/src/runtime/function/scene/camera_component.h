#pragma once

namespace Blunder {

struct CameraComponent final {
  float vertical_fov_degrees{45.0f};
  float near_clip{0.1f};
  float far_clip{1000.0f};
  bool is_main{false};
};

}  // namespace Blunder
