# Blunder-Engine Codebase Search - Detailed Findings

## Executive Summary

**Status:** GLM is integrated but not actively used. Missing critical header `runtime/core/math/math.h` that is referenced by `input_system.h`.

---

## 1. GLM Integration Analysis

### Where GLM Is Located
- **Path:** `D:\Dev\Blunder-Engine\engine\3rdparty\glm\`
- **Type:** Header-only library
- **Version:** 1.0+
- **Status:** Fully integrated into CMake build system

### How GLM Is Exposed
```cmake
# engine/src/runtime/CMakeLists.txt
target_link_libraries(engine_runtime
    PUBLIC glm      # ← Exposed to all consumers
    PUBLIC spdlog
    PRIVATE glfw
)
```

### GLM Headers Available
```
glm/
├── glm.hpp (main header)
├── vec2.hpp, vec3.hpp, vec4.hpp
├── mat2x2.hpp, mat3x3.hpp, mat4x4.hpp
├── gtc/ (qualified types extensions)
│   ├── matrix_transform.hpp
│   ├── quaternion.hpp
│   └── ...
└── gtx/ (extended functions)
    ├── euler_angles.hpp
    ├── rotate_vector.hpp
    └── ...
```

### Current Usage in Engine Source
**No includes found** in engine/src/ directory
- GLM is available but not yet imported
- Test coverage exists in 3rdparty/glm/test/ (150+ test files)

---

## 2. Critical Missing Header

### The Problem
**File:** `D:\Dev\Blunder-Engine\engine\src\runtime\function\input\input_system.h`

**Line 3:**
```cpp
#include "runtime/core/math/math.h"
```

**Status:** ❌ MISSING - This header does not exist

### Where It's Needed
```cpp
class InputSystem {
 public:
  // ...
  Radian m_cursor_delta_yaw{0};      // ← Requires Radian type from math.h
  Radian m_cursor_delta_pitch{0};    // ← Requires Radian type from math.h
  // ...
};
```

### Impact
Without `math.h`:
- `input_system.h` cannot compile
- `Radian` type is undefined
- Build will fail with compilation errors

### Directory Status
```
engine/src/runtime/core/math/
├── (directory exists)
└── (no files - completely empty)
```

---

## 3. Code Patterns and Conventions

### Header Guard Convention
```cpp
#pragma once
```
Used consistently across all headers.

### Include Organization
```cpp
#pragma once

// Standard library (alphabetical)
#include <chrono>
#include <memory>
#include <thread>

// Third-party libraries (system style)
#include <spdlog/spdlog.h>

// Internal project headers (quote style)
#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
```

### Namespace Convention
All types and classes within `Blunder` namespace:
```cpp
namespace Blunder {
  class MyClass { };
  extern GlobalContext g_runtime_global_context;
}
```

### Class Member Variables
```cpp
class InputSystem {
 private:
  int m_cursor_delta_x{0};           // m_ prefix + brace initialization
  unsigned int m_game_command{0};    // Default value in braces
  Radian m_cursor_delta_yaw{0};
};
```

### Enum Convention
```cpp
enum class GameCommand : unsigned int {
  forward = 1 << 0,
  backward = 1 << 1,
  // ...
  invalid = (unsigned int)(1 << 31)
};
```

### Template Usage
```cpp
template <typename... TARGS>
void log(LogLevel level, TARGS&&... args) {
  // Perfect forwarding pattern
  switch (level) {
    case LogLevel::debug:
      m_logger->debug(std::forward<TARGS>(args)...);
      break;
  }
}
```

### Macro Convention
```cpp
#define LOG_HELPER(LOG_LEVEL, ...) \
  g_runtime_global_context.m_logger_system->log( \
      LOG_LEVEL, "[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__);

#define LOG_DEBUG(...) LOG_HELPER(LogSystem::LogLevel::debug, __VA_ARGS__);
```

---

## 4. Runtime Library Structure

### CMakeLists.txt Configuration

**Location:** `engine/src/runtime/CMakeLists.txt`

```cmake
add_library(engine_runtime
    engine.cpp
    "function/input/input_system.cpp" "function/input/input_system.h" 
    "core/log/log_system.h"
)

target_include_directories(engine_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}  # Exposes headers as "runtime/..."
)

target_link_libraries(engine_runtime
    PUBLIC
        glm                          # Math library (available to consumers)
        spdlog                       # Logging (available to consumers)
    PRIVATE
        glfw                         # Window system (internal only)
)
```

### Include Path Exposure
The `PUBLIC` include directory means any file that links against `engine_runtime` can include:

```cpp
#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/math/math.h"           // ← Missing!
#include "runtime/function/input/input_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/engine.h"
```

### Dependency Visibility

**PUBLIC (exposed to all consumers):**
- `glm` - GLM math library
- `spdlog` - Logging framework

**PRIVATE (internal use only):**
- `glfw` - Window system implementation

---

## 5. Complete File Structure

### Directory Tree
```
engine/src/runtime/
├── CMakeLists.txt                          [Build configuration]
├── engine.h                                [10 lines - minimal stub]
├── engine.cpp                              [94 bytes - empty]
│
├── core/
│   ├── base/
│   │   └── macro.h                         [32 lines - logging macros]
│   ├── log/
│   │   ├── log_system.h                    [52 lines - LogSystem class]
│   │   └── log_system.cpp
│   └── math/                               [EMPTY - NEEDS math.h]
│
├── function/
│   ├── global/
│   │   ├── global_context.h                [46 lines - system manager]
│   │   └── global_context.cpp              [86 lines - implementations]
│   ├── input/
│   │   ├── input_system.h                  [49 lines - references math.h]
│   │   └── input_system.cpp                [1 line - empty]
│   ├── framework/                          [Directory - unused]
│   ├── physics/                            [Directory - unused]
│   └── render/                             [Directory - unused]
│
├── platform/                               [Directory - unused]
└── resource/                               [Directory - unused]
```

### Key Files Content Summary

#### macro.h (32 lines)
- Logging macros: LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL
- Utility macros: PolitSleep, PolitNameOf
- Assert macro with DEBUG/RELEASE variants

#### log_system.h (52 lines)
- `LogSystem` class with spdlog backend
- Template-based logging with perfect forwarding
- LogLevel enum: debug, info, warn, error, fatal

#### input_system.h (49 lines)
- **References missing math.h on line 3**
- `InputSystem` class for handling keyboard/mouse input
- GameCommand enum with bit flags
- Radian type members: m_cursor_delta_yaw, m_cursor_delta_pitch

#### global_context.h (46 lines)
- `RuntimeGlobalContext` singleton for system management
- Forward declarations for all systems
- `std::shared_ptr` members for each subsystem

#### global_context.cpp (86 lines)
- System initialization: startSystems()
- System shutdown: shutdownSystems()
- Currently implements: LogSystem, InputSystem
- Stubs for: WorldManager, RenderSystem, PhysicsManager, etc.

---

## 6. Type Requirements

### Radian Type Usage
Found in: `input_system.h`

```cpp
class InputSystem {
 private:
  Radian m_cursor_delta_yaw{0};
  Radian m_cursor_delta_pitch{0};
};
```

**Requirements:**
- Constructor accepting float (or default constructible)
- Should represent angle in radians
- Interoperable with float operations
- Member initialization support: `Radian{0}`

---

## 7. Build System Integration

### CMake Chain
```
Root CMakeLists.txt
  └─ engine/CMakeLists.txt
      ├─ engine/3rdparty/CMakeLists.txt
      │   ├─ glm
      │   ├─ glfw
      │   ├─ spdlog
      │   ├─ imgui
      │   └─ EASTL
      └─ engine/src/CMakeLists.txt
          ├─ runtime/CMakeLists.txt
          │   └─ engine_runtime library
          │       ├─ Links: glm, spdlog (PUBLIC)
          │       └─ Links: glfw (PRIVATE)
          └─ editor/CMakeLists.txt
```

### GLM Configuration
- Header-only: No compilation needed
- Include path: `${CMAKE_PREFIX_PATH}/include/glm/`
- Available as: `glm::glm` or `glm::glm-header-only` target
- C++17 required
- SIMD optimizations available (disabled by default)

---

## 8. Code Examples

