// engine/src/runtime/pch.h
// Precompiled Header for Blunder Engine Runtime
//
// Guidelines:
// - Only include STABLE headers that rarely change
// - Only include headers used across MULTIPLE translation units
// - Do NOT include project headers (to avoid circular dependencies)
// - Changes to this file trigger full rebuild of all consumers
#pragma once

// ============================================================================
// Platform Detection & Configuration
// ============================================================================
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
#endif

// ============================================================================
// C++ Standard Library (Stable, Frequently Used)
// ============================================================================
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

// ============================================================================
// Third-Party: EASTL (Memory & Containers)
// ============================================================================
#include <EASTL/algorithm.h>
#include <EASTL/array.h>
#include <EASTL/functional.h>
#include <EASTL/memory.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/string.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>

// ============================================================================
// Third-Party: GLM (Math Library - Header-Only)
// ============================================================================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// ============================================================================
// Third-Party: spdlog (Logging - Heavy Template Library)
// ============================================================================
#include <spdlog/spdlog.h>

// ============================================================================
// Third-Party: Vulkan SDK (Graphics API)
// ============================================================================
#include <vulkan/vulkan.h>
