## Why

Compile knows which Transients are live, but `resolvedTexture` still returns null for them. Later GPU Execute needs graph-owned objects for those Transients. This slice allocates after compile through a stand-in allocator, without putting Vulkan into `compile()` or `frame_graph_test`.

## What Changes

- Add `FrameGraph::allocate(IFrameGraphAllocator&)`. After a successful compile it asks the allocator for an owned RHI object for each **live** Transient from that resource’s Frame graph resource desc. DCE’d Transients and Externals are not allocated.
- Allocator `createTexture` / `createBuffer` return `unique_ptr`. The graph takes ownership. `resolvedTexture` / `resolvedBuffer` return a raw pointer. Tests pass a dummy allocator; no Vulkan device. Real `createTexture(desc)` is a later knife.
- **BREAKING:** `execute()` after a valid compile but before a successful allocate returns `NotAllocated` and runs no callbacks. Existing `frame_graph_test` execute cases that use live Transients must `allocate()` first.
- `allocate()` without a valid compile returns `NotCompiled` and does not call the allocator. Allocate does not compile. Execute does not allocate.
- Sticky allocate: a second `allocate()` on the same compile is Ok and does not call the allocator. Setup mutation destroys graph-owned Transient objects, leaves External pointers, and Transient resolve returns null until compile and allocate succeed again.
- A null `unique_ptr` for any live Transient is `AllocFailed`: roll back every Transient object from that call. Execute stays `NotAllocated`. No throw.
- New [ADR 0064](../../../docs/adr/0064-frame-graph-allocate.md). [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) points here. Callbacks stay `void()`. No `vulkan.h` on the graph.

## User stories

1. I compile a live Transient Texture, then `allocate()` with a dummy allocator: before execute, `resolvedTexture` is the object the allocator returned; the resource stays Transient; no Vulkan device.
2. I compile a live Transient Buffer (size 256) then allocate: `resolvedBuffer` is that object; `resourceDesc` size is still 256.
3. I allocate with no successful compile: `NotCompiled`, the allocator is never called. I compile successfully then execute without allocate: `NotAllocated`, no callback runs. Two live Transients, the second create returns null: `AllocFailed`, both resolves are null, execute is `NotAllocated`.
4. I have a DCE’d Transient and a live one: allocate creates only the live one; the DCE’d resolve stays null. An imported External dummy pointer is not passed to the allocator.
5. I allocate twice on the same compile: create count does not increase; the pointer is still the first object. I then mutate Setup: Transient resolve is null; execute is `NotCompiled`. After compile and allocate again, Clear→Forward Sink execute still runs.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: `allocate()` after compile; graph-owned live Transient objects; `resolved*` after allocate; execute `NotAllocated`; allocate `NotCompiled` / `AllocFailed`; sticky allocate; Setup destroys owned Transients.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` — `IFrameGraphAllocator`, `allocate()`, `FrameGraphAllocateReason` / execute `NotAllocated`, owned Transient `unique_ptr`, resolve of live Transients after allocate. No `vulkan.h`. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp` with a dummy allocator. Every existing execute path that uses a live Transient must allocate first. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph allocate / allocator (Grill); ADR 0064; ADR 0061 pointer.
- **Non-goals:** Vulkan `createTexture(desc)`; memory aliasing; barriers; changing `setExecute`; replacing ForwardRenderPath; `VkImage` on the graph; JSON; Blackboard; Frame arena; Job System scheduling allocate.
