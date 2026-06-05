# CMake Conventions

## Adding Source Files

```cmake
add_library(engine_runtime
    engine.cpp
    "function/input/input_system.cpp"
)
```

## Linking

- `PUBLIC`: Dependencies in public headers
- `PRIVATE`: Internal implementation only

```cmake
target_link_libraries(engine_runtime
    PUBLIC glm spdlog
    PRIVATE glfw
)
```

## Adding a New System

1. Create header/source in `engine/src/runtime/function/<system>/`
2. Add `std::shared_ptr<YourSystem>` to `RuntimeGlobalContext`
3. Initialize in `startSystems()`, cleanup in `shutdownSystems()`
4. Add files to `engine/src/runtime/CMakeLists.txt`

## See also

- [structure.md](structure.md)
- [build.md](build.md)
- [design-docs/architecture.md](../design-docs/architecture.md)
- [golden-principles.md](../golden-principles.md)
