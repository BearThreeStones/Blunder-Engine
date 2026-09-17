# Manual checklist — frame-graph-allocate

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I compile a live Transient Texture, then `allocate()` with a dummy allocator: before execute, `resolvedTexture` is the object the allocator returned; the resource stays Transient; no Vulkan device. | |
| 2 | I compile a live Transient Buffer (size 256) then allocate: `resolvedBuffer` is that object; `resourceDesc` size is still 256. | |
| 3 | I allocate with no successful compile: `NotCompiled`, the allocator is never called. I compile successfully then execute without allocate: `NotAllocated`, no callback runs. Two live Transients, the second create returns null: `AllocFailed`, both resolves are null, execute is `NotAllocated`. | |
| 4 | I have a DCE’d Transient and a live one: allocate creates only the live one; the DCE’d resolve stays null. An imported External dummy pointer is not passed to the allocator. | |
| 5 | I allocate twice on the same compile: create count does not increase; the pointer is still the first object. I then mutate Setup: Transient resolve is null; execute is `NotCompiled`. After compile and allocate again, Clear→Forward Sink execute still runs. | |
