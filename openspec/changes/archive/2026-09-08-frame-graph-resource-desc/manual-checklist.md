# Manual checklist — frame-graph-resource-desc

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I create a Transient Texture (`R8G8B8A8_UNORM`, 8×8, samples 1, mips 1) and a Sink that Writes it: compile succeeds; `resourceDesc` matches that desc before compile; the resource stays Transient. | |
| 2 | I import an External Texture with the same viewport-color stand-in desc and a Sink that Writes it: compile succeeds; the resource stays External; `resourceDesc` matches; the graph does not treat it as a Transient it would allocate. | |
| 3 | I create a Transient Buffer (size 256), write it, then read it from a Sink: compile succeeds; `resourceDesc` size is 256. | |
| 4 | I create an illegal desc (Texture: `Undefined` / width or height 0 / samples or mips 0; or Buffer size 0) even when no Pass uses it: compile fails with `InvalidDesc`, does not throw, and the live order is empty. | |
| 5 | The same graph has a never-added Pass handle, an illegal desc, and a dangling access: compile reports only `InvalidPass`. With the illegal Pass removed, illegal desc plus dangling reports `InvalidDesc`. After `InvalidDesc`, `execute()` is `NotCompiled`. Clear→Forward Sink and DCE still work when every desc is legal. | |
