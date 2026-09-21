#pragma once

#include "runtime/function/debug/tracy_instrument.h"

#ifdef TRACY_ENABLE
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>
#define BLUNDER_TRACY_VK_ZONE(ctx, cmd, name)                                 \
  TracyVkNamedZone((ctx), ___tracy_gpu_zone, (cmd), name, (ctx) != nullptr)
#define BLUNDER_TRACY_VK_COLLECT(ctx, cmd)                                    \
  do {                                                                        \
    if ((ctx) != nullptr) {                                                   \
      TracyVkCollect((ctx), (cmd));                                           \
    }                                                                         \
  } while (0)
#else
#define BLUNDER_TRACY_VK_ZONE(ctx, cmd, name)
#define BLUNDER_TRACY_VK_COLLECT(ctx, cmd)
#endif
