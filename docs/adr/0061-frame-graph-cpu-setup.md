# CPU Frame graph Setup before Execute

Viewport work is still immediate-mode in `ForwardRenderPath`. Packt chapter4 stores `TextureHandle` on the resource and creates Vulkan images in `compile()`. We decided v1 is a caller-owned **Frame graph**: virtual handles, GraphBuilder Setup, compile to topo + DCE from Sinks + Resource lifetimes, no GPU allocation. Domain: [CONTEXT.md — Frame graph](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Packt-style GPU handles on Setup** — rejected. Grill: Setup identity is a Frame graph handle; Execute later resolves RHI objects. Tests must run without a device.
- **JSON `graph.json` as authoring** — rejected. No engine Pass list in this slice; C++ GraphBuilder is the product API.
- **Blackboard in v1** — rejected. No split Setup modules yet; handles pass as C++ values.
- **Frame arena and per-tick rebuild** — rejected for this slice. Ordinary containers on a caller-owned graph; arena when this runs every tick.
- **Replace ForwardRenderPath in this change** — rejected. Prove the CPU DAG first.
- **Treat every External as a Sink** — rejected. Sink is an explicit Pass mark; unused External writes must be cullable.
- **Exceptions from compile** — rejected. Return failure and an empty live order (`InvalidPass`, dangling access, zero Sinks, live cycle), one reason in that priority.
- **RDG-style Resource version** — rejected for this slice. Compile orders same-handle RAW, WAW, and WAR in Setup order (Pass add order, then that Pass’s read/write call order). Same-Pass accesses do not add an edge. Usage does not grow extra edges.
- **Silent drop of a never-added Pass handle** — rejected. `read` / `write` / `markSink` on that handle make compile return `InvalidPass`.
