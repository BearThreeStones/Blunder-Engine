## 1. Foundations

- [x] 1.1 Add annotation macros and ClassDB core types (TypeInfo, PropertyInfo, MethodBind stubs)
- [x] 1.2 Implement ClassDB registry register/lookup APIs and module init hook
- [x] 1.3 Add small Variant type covering pilot property kinds (bool, int, float, Vec3, Quat, string)
- [x] 1.4 Wire new sources into `engine/src/runtime/CMakeLists.txt` / global context as needed

## 2. Object identity

- [x] 2.1 Implement Object base with ObjectId allocation, name, enabled, parent/child Scene Tree links
- [x] 2.2 Implement optional EntityId binding + destroy/despawn pairing
- [x] 2.3 Implement projected spatial accessors with lazy Entity+Transform materialization (façade over current storage OK)
- [x] 2.4 Implement Script Peer handle get/set/clear (opaque void*/GCHandle slot only)

## 3. Call paths and lifecycle

- [x] 3.1 Implement PtrCall path on MethodBind for pilot methods
- [x] 3.2 Implement Variant property get/set on ClassDB for pilot properties
- [x] 3.3 Implement per-type lifecycle dispatch table (Ready/Tick) with clear/replace and peer-gated invoke

## 4. Clang export toolchain

- [x] 4.1 Add generator script/tool (libclang) and document how to obtain compile flags under MSVC
- [x] 4.2 Emit `.gen.cpp` registration for pilot Object (+ Transform projection properties/methods)
- [x] 4.3 Emit API Blueprint JSON consumed only as an artifact in this milestone
- [x] 4.4 Add CMake/custom command or documented manual step to regenerate pilots

## 5. C-ABI skeleton

- [x] 5.1 Add versioned `extern "C"` header: ObjectId resolve, property get/set, PtrCall, lifecycle hook register/clear
- [x] 5.2 Ensure engine links and starts with empty hooks and no .NET dependency
- [x] 5.3 Add a small native smoke test or sample calling the C-ABI (no C# required)

## 6. Validation and docs

- [x] 6.1 Unit tests for ObjectId lifecycle, lazy materialization, ClassDB lookup, PtrCall/Variant round-trip
- [x] 6.2 Confirm ADRs `0003`–`0005` and `CONTEXT.md` still match implementation
- [x] 6.3 Build with `vs2026-debug` (or documented preset) and run smoke test
