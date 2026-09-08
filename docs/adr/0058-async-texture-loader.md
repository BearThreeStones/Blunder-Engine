# Async Texture Loader, not a second AssetManager

Material textures still stall the tick: disk decode then a graphics-queue copy that waits on a fence. We decided on a **Texture Loader**: **Jobs** read and decode into **Job data**; a Render-owned **upload queue** copies through a **staging pool** and polls fences; draws keep the **Bindless fallback index** until that copy is resident. v1 is color material textures only. Domain terms: [CONTEXT.md — Texture Loader](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Keep `endImmediateCommands` wait on the tick** — rejected. That is the hitch.
- **Put GPU copies inside a Job** — rejected. Jobs must not call RHI ([ADR 0057](0057-task-job-system.md)).
- **A second OS thread pool beside the Job System** — rejected. Complexity penalty; CPU work is Jobs.
- **Dedicated Vulkan transfer queue family in v1** — rejected. Graphics queue + fence poll already exists (`submitImmediateCommandsNoWait`). Transfer-family ownership can wait.
- **Texture + mesh buffer streaming in one change** — rejected. Mesh `GpuMesh` upload is a separate stall; Grill locked textures first.
- **Checkerboard placeholder or hide mesh until resident** — rejected. Bindless slot 0 is already the missing/overflow path ([ADR 0056](0056-bindless-texture-table.md)).
- **Fold this into AssetManager or ThumbnailGenerator** — rejected. AssetManager is the CPU cache and Pull cook. Thumbnails are Browser stills with their own queue.
