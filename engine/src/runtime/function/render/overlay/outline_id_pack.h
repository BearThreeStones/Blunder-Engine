#pragma once

#include <cstdint>

namespace Blunder {

/// Packs Blender-style outline object IDs into R16_UINT prepass output.
struct OutlineIdPack {
  static constexpr uint32_t k_max_ob_id = 0x3FFFu;

  static uint16_t pack(uint32_t color_id, uint32_t ob_id) {
    return static_cast<uint16_t>(((color_id & 3u) << 14) | (ob_id & k_max_ob_id));
  }

  static uint32_t colorId(uint16_t packed) { return static_cast<uint32_t>(packed >> 14); }

  static uint32_t objectId(uint16_t packed) {
    return static_cast<uint32_t>(packed & k_max_ob_id);
  }
};

inline uint32_t resolveOutlineColorId(bool translate_modal_active,
                                      bool gizmo_dragging) {
  if (translate_modal_active) {
    return 2u;
  }
  if (gizmo_dragging) {
    return 0u;
  }
  return 1u;
}

}  // namespace Blunder
