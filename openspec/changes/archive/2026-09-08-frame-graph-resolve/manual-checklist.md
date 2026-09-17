# Manual checklist — frame-graph-resolve

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I import an External Texture with a legal desc and a dummy `IGpuTexture*`: before compile, `resolvedTexture` is that pointer; the resource stays External; no Vulkan device. | |
| 2 | I import an External Buffer (size 256) with a dummy `IGpuBuffer*`: `resolvedBuffer` is that pointer; `resourceDesc` size is still 256. | |
| 3 | I import with a null pointer, including an External no Pass uses: compile fails with `InvalidImport`, does not throw, live order is empty; `resolved*` is `nullptr`; `execute()` is `NotCompiled`. | |
| 4 | The same graph has a never-added Pass handle, an illegal desc, a null import, and a dangling access: compile reports only `InvalidPass`. With the illegal Pass removed, illegal desc plus null import plus dangling reports `InvalidDesc`. With a legal desc, null import plus dangling reports `InvalidImport`. | |
| 5 | Transient, a Texture handle asked as `resolvedBuffer`, and a never-created handle all return `nullptr` without throwing. A DCE'd External still returns the pointer that was imported. Clear→Forward Sink plus DCE still compile and execute when every External used on the live path has a dummy non-null pointer. | |
