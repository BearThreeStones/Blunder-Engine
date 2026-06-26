#pragma once

#include <cstdint>

namespace Blunder {

enum class DockFloatingFlag : uint32_t {
  none = 0,
  drag_preview = 0x01,
  native_os_window = 0x02,
#if defined(_WIN32)
  default_config = drag_preview | native_os_window,
#else
  default_config = drag_preview,
#endif
};

inline DockFloatingFlag operator|(DockFloatingFlag a, DockFloatingFlag b) {
  return static_cast<DockFloatingFlag>(static_cast<uint32_t>(a) |
                                       static_cast<uint32_t>(b));
}

inline bool testFloatingFlag(DockFloatingFlag config, DockFloatingFlag flag) {
  return (static_cast<uint32_t>(config) & static_cast<uint32_t>(flag)) != 0;
}

}  // namespace Blunder
