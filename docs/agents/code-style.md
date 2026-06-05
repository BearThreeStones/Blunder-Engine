# Code Style

## Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `LogSystem`, `InputSystem` |
| Methods | camelCase/PascalCase | `initialize()`, `Tick()` |
| Member vars | m_prefix | `m_logger`, `m_game_command` |
| Local vars | snake_case | `console_sink` |
| Constants | k_prefix | `k_complement_control_command` |
| Enums | PascalCase class, lowercase values | `LogLevel::debug` |
| Macros | SCREAMING_CASE | `LOG_DEBUG`, `ASSERT` |
| Template params | ALL_CAPS | `TARGS` |
| Namespace | `Blunder` | All engine code |

## Header Files

- Use `#pragma once`
- Include order: corresponding header → project → third-party → standard
- Use full paths from `runtime/` root: `#include "runtime/core/log/log_system.h"`

## Formatting

- 2-space indentation (no tabs)
- Opening brace on same line
- Use `final` for non-inheritable classes
- Close namespaces with comment: `}  // namespace Blunder`

```cpp
class LogSystem final {
 public:
  LogSystem();
  
 private:
  std::shared_ptr<spdlog::logger> m_logger;
};
```

## Logging

```cpp
LOG_DEBUG("Debug: {}", value);
LOG_INFO("Info message");
LOG_WARN("Warning: {}", warning);
LOG_ERROR("Error: {}", error);
LOG_FATAL("Fatal - throws: {}", msg);
```

## Memory Management

- `std::shared_ptr` for system singletons
- Global context: `g_runtime_global_context`
- Reset in reverse order during shutdown

## Error Handling

- `LOG_FATAL` throws `std::runtime_error`
- `ASSERT()` for debug-only checks (disabled in NDEBUG)

## See also

- [golden-principles.md](../golden-principles.md)
- [cmake.md](cmake.md)
- [msvc-defines.md](msvc-defines.md)
- [cursor-cloud.md](cursor-cloud.md) — optional clang-format / clang-tidy
