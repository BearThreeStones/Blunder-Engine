## 1. Allocate API

- [x] 1.1 Add `IFrameGraphAllocator` (`createTexture` / `createBuffer` → `unique_ptr`). Add `FrameGraphAllocateReason` (`Ok`, `NotCompiled`, `AllocFailed`) and `allocate(IFrameGraphAllocator&)`. Add execute reason `NotAllocated`. No `vulkan.h`.
- [x] 1.2 Store graph-owned Transient objects as `unique_ptr`. External stays a raw non-owning pointer. `resolvedTexture` / `resolvedBuffer` return the owned object for a live allocated Transient; DCE’d / not-yet-allocated Transient stays null.
- [x] 1.3 `allocate()`: live Transients only; sticky second call does not create; Setup mutation destroys owned Transients and leaves External pointers. No compile inside allocate. No allocate inside execute.
- [x] 1.4 Failures without throw: allocate without compile → `NotCompiled` (no create calls). Null create → `AllocFailed` and roll back all Transient objects from that call. Execute with valid compile and no allocate → `NotAllocated`. Compile invalid → execute `NotCompiled`.

## 2. Tests

- [x] 2.1 Dummy allocator in `frame_graph_test` (no Vulkan device). Update every existing execute path that uses a live Transient to `allocate()` first. Cover the five User stories: live Texture resolve after allocate; Buffer 256; `NotCompiled` / `NotAllocated` / `AllocFailed` rollback; DCE not created + External untouched; sticky allocate + Setup dirty + Clear→Forward execute.
- [x] 2.2 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. ADR

- [x] 3.1 Keep [ADR 0064](../../../docs/adr/0064-frame-graph-allocate.md) and the [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) pointer aligned with the shipped allocate API.
