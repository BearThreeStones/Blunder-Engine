# Design

## Context

See proposal.md for why. Grill locked unique Mesh Asset streaming (not entity paging), editor and Player on the same `SceneSystem::loadScene` path, skip-draw until `GpuMesh` is resident, GUID bind kept, DogWalk `se-world.scene.asset` as the human walk, and the collision ~10k glTF reimport out. Decision: [ADR 0072](../../../docs/adr/0072-async-scene-mesh-streaming.md).

Today `loadScene` → `instantiateScene` → `completeSceneDocumentInstantiate` → `GltfSceneImporter::attachEntityMeshes` is synchronous. On `main`, attach still opens glTF under each mesh entity. The flatten apply tree (`c0c60a0`) binds shared Mesh Asset GUIDs (`loadMesh` / `loadMeshByGuid`) so SE-world is 252 unique meshes, not ~10k glTF documents; `se_world_flatten_test` timed that attach (~5283.6 ms CPU, gate `<10s`, no GPU). `syncSceneToRender` then calls `getOrUploadGpuMesh` → `GpuMesh::create` (`CPU_TO_GPU` mapped upload) on the tick. Texture Loader already Jobs-decode + timeline-copy textures; draws use Bindless fallback until resident ([ADR 0058](../../../docs/adr/0058-async-texture-loader.md)). Jobs must not call RHI ([ADR 0057](../../../docs/adr/0057-task-job-system.md)). Player still `loadScene`s the entry scene in `startSystems` before Vulkan. Product Project: `E:\Blunder Projects\DogWalk`. Do not edit `cursor/scene-collision-bridge-8a89` / PR 24. Do not land commits on flatten PR 31.

## Goals / Non-Goals

**Goals:**

- One Mesh Loader: unique GUID coalesce, document-order CPU Jobs, RHI-tick `GpuMesh` budget, skip-draw, fail-skip, generation cancel.
- `loadScene` / `openScene` / `engine_player` return to Iterate with the entity table up and unique meshes still in flight.
- Tests on a synthetic multi-GUID fixture. Human walk on DogWalk `se-world` with GUID bind.

**Non-Goals (design-level):**

- Streaming `openGltfImportDocument` per instance (collision exe).
- Entity paging, frustum Job priority, binary `.scene.asset`, loading-bar UI, first-frame ms cap.
- Rewriting Texture Loader, 2/frame material hydrate as open, MultiMesh Unique, Nanite disk stream.
- Commits on PR 24 / PR 31.

## Decisions

1. **Mesh Loader is Render-owned; SceneSystem only enqueues unique GUIDs.**  
   GPU objects and tick upload live next to Texture Loader (`function/render/`). `loadScene` instantiates entities, records unique Mesh Asset GUIDs in document order, `request`s the Loader, and returns. Headless with no device skips `GpuMesh`.  
   *Alternatives:* put GPU upload on AssetManager (pulls RHI into resource); a SceneSystem-owned GPU queue (duplicates Render lifetime).

2. **CPU half Submits Jobs and does not `wait()` to Iterate.**  
   Job data is a caller-owned slot (GUID / path, CPU mesh bytes or MeshAsset handle published after the Job, done/failed). Workers run the Job. The tick polls flags, binds MeshRenderers, and enqueues GPU work. `wait()` is shutdown or a still-path drain (Thumbnail), not product open.  
   *Alternatives:* `wait()` all unique meshes in `loadScene` (status quo hitch); a second IO thread pool (Complexity penalty; Job System exists).

3. **GPU half is a per-tick unique-mesh budget of `GpuMesh::create` on the RHI thread.**  
   v1 keeps mapped `CPU_TO_GPU` uploads; it budgets how many unique meshes (or outstanding upload bytes) finish per tick. It does not require Texture Loader’s image staging copies or a transfer-queue family. Jobs never call `GpuMesh::create`. `syncSceneToRender` uses `findUploadedGpuMesh` (or Loader lookup) and skips; it must not call `getOrUploadGpuMesh` for every missing unique mesh in one sync.  
   *Alternatives:* one sync `create` of all 252 (forbidden); Texture-Loader-style buffer copies in v1 (extra machinery; mapped upload is the stall to budget first); dedicated transfer family (rejected for textures already).

4. **Coalesce by Mesh Asset GUID; bind many MeshRenderers to one resident mesh.**  
   Flatten-style GUID bind is the product attach. Thousands of instances sharing 252 GUIDs enqueue 252 Jobs, not 10794. Path-only registered descriptors resolve to the same GUID when the registry knows them.  
   *Alternatives:* stream per MeshRenderer (recreates the 10k problem); coalesce by glTF source path only (misses GUID identity).

5. **Per-instance glTF import is not streamed.**  
   `openGltfImportDocument` under every entity stays the old importer. This change does not wrap it in Jobs. Collision product exe ~10k glTF reimport stays out. Apply on `main` must use unique Mesh Asset refs as the streaming unit; DogWalk acceptance needs GUID bind (flatten product), not a merge of PR 31 into this branch.  
   *Alternatives:* Job-wrap the glTF importer (Grill: out); wait to propose until flatten merges (planning can sit on `main`).

6. **Skip-draw, not placeholder boxes, not Bindless-style mesh fallback.**  
   Textures can sample slot 0; a missing `GpuMesh` cannot. Skip that MeshRenderer. Failed GUID: skip that class; keep the instance.  
   *Alternatives:* unit cube placeholders (Grill: not the forest); hide the whole scene until complete (blocks Iterate visually).

7. **Document order; no frustum queue; no loading UI; no first-frame ms gate.**  
   Grill defaults. Geometry popping is progress. Flatten `<10s` remains flatten’s CPU attach gate.  
   *Alternatives:* Slint “loading meshes” (rejected this slice); view-dependent Job sort (later knife).

8. **Materials sidecar with CPU mesh; Texture Loader untouched.**  
   When the CPU Mesh is ready, bind its MaterialAsset. Do not reopen 2/frame `tickDeferredGltfMaterials` as the open policy (flatten drain stays flatten). Checkerboard albedo is Texture Loader, not Mesh Loader failure.  
   *Alternatives:* stream materials as a third loader (out); wait Texture Loader to call the scene complete (QC wait, not open gate).

9. **Player streams in-process after `loadScene` returns; do not move Live pointers.**  
   `startSystems` MAY still call `loadScene` before Vulkan if that call no longer waits unique mesh CPU+GPU. Iterate (SDL loop) is the product gate. Play Reload instantiates a new instance with the same enqueue rules.  
   *Alternatives:* delay Player `loadScene` until after Vulkan (optional later; not required if `loadScene` is cheap); stream editor Live into Player (forbidden).

10. **Scene Thumbnail / Mesh Preview MAY drain; they are not the open path.**  
    Content Browser 2/tick thumbnails are not this stream. A still MAY `wait()` CPU Jobs or upload on its own queue so a thumbnail is complete. Product Viewport / Player must not use that drain.  
    *Alternatives:* make thumbnails async in this slice (Grill: not scene stream).

11. **Skinned upload stays on the existing path.**  
    Unique static Mesh Assets are the 252-GUID stall. CPU/GPU skinning `getOrUploadGpuMeshByKey` is unchanged.  
    *Alternatives:* stream skinned buffers too (no forest need).

12. **Independent of collision-bridge and flatten-apply git.**  
    This branch is from `main`. No files from `cursor/scene-collision-bridge-8a89`. No commits on `cursor/se-world-flatten-apply-5ff3`.  
    *Alternatives:* stack this apply on flatten (Human can, later; this planning PR must not).

## Risks / Trade-offs

- **[Risk] JSON deserialize + instantiate still hitch** → Entities stay sync by lock. Binary Scene Asset is a later knife; do not expand this slice.
- **[Risk] Apply on `main` still glTF-attaches per entity** → Streaming unit is unique Mesh Asset GUID; tests use a GUID fixture. Human walk needs GUID-bind `se-world` (flatten product), not collision exe.
- **[Risk] Mapped `CPU_TO_GPU` upload still hitches one fat mesh** → Budget unique meshes per tick; prefer skip-draw over a 252-create sync. Staging copies can wait.
- **[Risk] Player `loadScene` before Vulkan** → CPU Jobs may run during remaining boot; GPU starts when RenderSystem ticks. Do not `wait()` unique meshes before SDL Iterate.
- **[Risk] In-flight Job after scene drop** → Generation check before bind / `GpuMesh` insert, same family as Texture Loader.
- **[Trade-off] First frames sparse / empty forest** → Allowed. No loading bar this slice.
- **[Trade-off] Checkerboard albedo** → Texture Loader; not a fail.

## Migration Plan

1. Land planning artifacts (this change). No Mesh Loader C++ yet.
2. On apply: Mesh Loader + synthetic tests on `main`; wire `loadScene` / `openScene` / Player; skip-draw in `syncSceneToRender`.
3. Human walk on Windows DogWalk `se-world.scene.asset` with GUID bind. Do not merge this planning PR into flatten apply or collision.
4. Rollback: revert Loader and restore blocking `loadMesh` + `getOrUploadGpuMesh` on the open path.

No content format migration. No cook-format change.

## Open Questions

None that block specs. Grill defaults are locked: no new loading UI, document-order Jobs, no binary Scene Asset, no first-frame ms gate.
