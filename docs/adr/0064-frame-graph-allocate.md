# Frame graph allocate is its own phase

Compile still builds only the CPU DAG. We decided live Transient RHI objects are created in a separate `allocate(IFrameGraphAllocator&)` after compile, not inside compile or execute. The graph takes `unique_ptr` ownership; tests pass a stand-in allocator and do not create a Vulkan device. Domain: [CONTEXT.md — Frame graph allocate](../../CONTEXT.md). CPU DAG: [ADR 0061](0061-frame-graph-cpu-setup.md). External attach: [ADR 0063](0063-frame-graph-resolve.md).

**Status:** accepted

## Considered Options

- **Packt-style create in `compile()`** — rejected. [ADR 0061](0061-frame-graph-cpu-setup.md) already forbids GPU work in compile. Allocate is a later phase that reads live Transients and Resource lifetimes.
- **First thing inside `execute()`** — rejected. Resolve would stay null until callbacks run. Allocate failure would mix with `NotCompiled`. Tests need compile → allocate → resolve without running callbacks.
- **Real Vulkan `createTexture(desc)` in this slice** — rejected. `IRenderDevice` has no blank-texture create. `frame_graph_test` stays device-free. A stand-in allocator proves ownership; the Vulkan allocator is a later knife.
- **Store the allocator on the graph** — rejected. The graph would hold a device-facing pointer. Pass it each `allocate()`.
- **Raw pointer plus `destroy()`** — rejected. Easy to leak. Allocator returns `unique_ptr`; the graph owns; resolve returns a raw view.
- **Null create as a successful allocate** — rejected. A live Transient would be half-imported. `AllocFailed` rolls back every Transient object from that call. Execute then remains `NotAllocated`.
- **Treat missing allocate as `NotCompiled`** — rejected. Compile can be valid. Execute uses `NotAllocated`. Allocate without compile still uses `NotCompiled`.
- **Reallocate on every `allocate()`** — rejected. Sticky until Setup mutates. A second `allocate()` is Ok and does not call the allocator.
- **Leave Transient objects after Setup mutation** — rejected. Setup destroys graph-owned Transient objects and leaves External pointers. Transient resolve returns null until compile and allocate succeed again.
- **Aliasing, barriers, command recording, `ForwardRenderPath`** — rejected for this slice.
