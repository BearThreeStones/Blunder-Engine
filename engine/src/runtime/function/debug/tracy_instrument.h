#pragma once

// CPU Tracy Client macros. GPU Vulkan helpers live in tracy_vk_instrument.h
// so TUs that do not include Vulkan stay free of TracyVulkan.hpp.

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN(name)
#define FrameMark
#define FrameMarkNamed(name)
#define FrameMarkStart(name)
#define FrameMarkEnd(name)
#define TracyPlot(name, val)
#endif
