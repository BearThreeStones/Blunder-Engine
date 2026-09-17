# CPU Frame graph Setup before Execute

Viewport Scene dispatch through Frame graph execute is [ADR 0067](0067-frame-graph-viewport-wire.md). Packt chapter4 stores `TextureHandle` on the resource and creates Vulkan images in `compile()`. We decided v1 is a caller-owned **Frame graph**: virtual handles, GraphBuilder Setup, compile to topo + DCE from Sinks + Resource lifetimes, no GPU allocation in compile. External RHI attach is [ADR 0063](0063-frame-graph-resolve.md). Transient allocate is [ADR 0064](0064-frame-graph-allocate.md). Barrier plan is [ADR 0065](0065-frame-graph-barrier-plan.md). GPU Execute recording onto a Frame graph recorder is [ADR 0066](0066-frame-graph-gpu-execute.md). Domain: [CONTEXT.md — Frame graph](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Packt-style GPU handles on Setup** — rejected. Grill: Setup identity is a Frame graph handle; Execute later resolves RHI objects. Tests must run without a device.
- **JSON `graph.json` as authoring** — rejected. No engine Pass list in this slice; C++ GraphBuilder is the product API.
- **Blackboard in v1** — rejected. No split Setup modules yet; handles pass as C++ values.
- **Frame arena and per-tick rebuild** — rejected for this slice. Ordinary containers on a caller-owned graph; arena when this runs every tick.
- **Replace ForwardRenderPath in this change** — rejected. Prove the CPU DAG first.
- **Treat every External as a Sink** — rejected. Sink is an explicit Pass mark; unused External writes must be cullable.
- **Exceptions from compile** — rejected. Return failure and an empty live order (`InvalidPass`, `InvalidDesc`, dangling access, zero Sinks, live cycle), one reason in that priority.
- **GPU or RHI objects on create/import** — rejected for this slice. Create and import take a CPU Frame graph resource desc (format, extent, samples, mips; Buffer size). Import attaching a non-owning RHI pointer is [ADR 0063](0063-frame-graph-resolve.md). Tests still run without a device.
- **Reuse `VkFormat` or `rhi::PixelFormat` as Setup format** — rejected. `FrameGraphFormat` lives on the graph (`Undefined`, `R8G8B8A8_UNORM`, `D32_SFLOAT`). Buffer has no format.
- **Treat illegal desc as `DanglingAccess`, or ignore unused illegal desc** — rejected. The resource exists; compile returns `InvalidDesc` even if DCE would drop it. Priority: `InvalidPass`, then `InvalidDesc`, then `DanglingAccess`, then `NoSink`, then `Cycle`. Create/import do not throw.
- **Require compile before `resourceDesc`** — rejected. `resourceDesc` is Setup state. DCE does not erase it. A never created/imported handle returns a default-constructed desc.
- **RDG-style Resource version** — rejected for this slice. Compile orders same-handle RAW, WAW, and WAR in Setup order (Pass add order, then that Pass’s read/write call order). Same-Pass accesses do not add an edge. Usage does not grow extra edges.
- **Silent drop of a never-added Pass handle** — rejected. `read` / `write` / `markSink` / `setExecute` on that handle make compile return `InvalidPass`.
- **Packt-style GPU work inside compile / execute** — rejected for the CPU DAG slices. Transient GPU objects are [ADR 0064](0064-frame-graph-allocate.md), not compile. CPU barrier lists are [ADR 0065](0065-frame-graph-barrier-plan.md), not `vkCmdPipelineBarrier`. Recording those rows onto a Frame graph recorder is [ADR 0066](0066-frame-graph-gpu-execute.md).
- **Implicit compile inside execute** — rejected. If compile has never succeeded, the last compile failed, or Setup changed since the last successful compile, execute returns `NotCompiled` and runs nothing. It does not echo compile failure reasons.
- **Stale live order after Setup mutation** — rejected. Any GraphBuilder Setup call dirties compile, allocate, and the barrier plan, and clears compile output.
- **Catching callback exceptions** — rejected. Callbacks must not throw; execute does not catch. No `CallbackFailed` reason.
- **Fence for GraphBuilder / nested execute from a callback** — rejected (`/review` Skip). Callbacks must not reenter, same contract as must-not-throw. GPU Execute snapshots live Pass order, the barrier list, and callbacks so a forbidden Setup mutation does not UAF, then still returns `Ok`. Decision: [ADR 0066](0066-frame-graph-gpu-execute.md).
