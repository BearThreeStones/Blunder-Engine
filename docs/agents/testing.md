# Testing

First-party tests live under `engine/src/tests/` and are wired with CTest via `engine/src/tests/CMakeLists.txt`.

## Run

```powershell
cmake --build build/vs2026-debug --config Debug --target classdb_test
.\build\vs2026-debug\engine\src\tests\Debug\classdb_test.exe

# Or, from the build tree:
ctest --test-dir build/vs2026-debug -C Debug -R "classdb|variant|object_db|ptrcall|engine_c_abi|editor_history|scene_soft_delete|editor_commands" --output-on-failure
```

Tests that only pull ClassDB/Object (not SceneInstance/Vulkan) stay small and do not need `slang.dll`. Prefer `IEntityStore` fakes over linking `SceneInstance` in unit tests.

`scene_soft_delete_test` / `editor_commands_test` link `SceneInstance` and may need `slang.dll` + `slint_cpp.dll` on `PATH` (e.g. VulkanSDK Bin + `.cmake_deps/slint-build`).

## Reflection kernel coverage

| Target | Focus |
|--------|--------|
| `classdb_test` | ClassDB register/lookup |
| `variant_test` | Variant kinds |
| `object_db_test` | ObjectId, Scene Tree, Script Peer, lazy Entity via fake store |
| `ptrcall_lifecycle_test` | PtrCall, Variant properties, lifecycle hooks |
| `engine_c_abi_test` | C-ABI smoke (no .NET) |

## Editor History coverage

| Target | Focus |
|--------|--------|
| `editor_history_test` | DocumentHistory stack, dirty baseline, selection restore |
| `scene_soft_delete_test` | Soft-delete EntityId stability; export omits tombstones |
| `editor_commands_test` | Transform / spawn / soft-delete command undo/redo |

## See also

- [cmake.md](cmake.md)
- [build.md](build.md)
- [golden-principles.md](../golden-principles.md)
- [common-tasks.md](common-tasks.md)
- `tools/reflection_export/README.md`
