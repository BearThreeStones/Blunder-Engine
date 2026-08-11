## Context

Content Browser Scene Assets currently fall through to a generic File placeholder. Grilling locked **Scene Thumbnail Render** as a Camera still (Play camera resolve, square aspect, on-disk only, dedicated offscreen) in CONTEXT and [ADR 0024](../../../docs/adr/0024-scene-thumbnail-render-offscreen.md). Mesh Preview already owns a dedicated RT; Scene thumbs must not share Camera Preview or main viewport targets.

## Goals / Non-Goals

**Goals:**
- Dedicated Scene Thumbnail Render still → thumbnail cache for `.scene.asset`.
- Play camera resolve; aspect=1; recursive childScenes; no Editor Overlays; bind/rest pose.
- Cache invalidation: scene mtime + fingerprint of direct Mesh Asset References (root + recursive children).
- Soft fail → Scene placeholder.
- Scene lights when available, else studio lights.

**Non-Goals:**
- Dirty live SceneInstance as source.
- AnimationPlayer sampling.
- Authored cover images.
- Sharing Camera Preview / Mesh Preview / main viewport RTs.
- Full dependency-graph fingerprint beyond direct mesh refs.

## Decisions

1. **Dedicated Scene Thumbnail offscreen owner** — New `PreviewRenderTargetOwner::SceneThumbnail` (or reuse Mesh Preview backend draw path with SceneThumbnail owner when sharing GPU plumbing is safe). Prefer extending Mesh Preview offscreen **draw submission** with an explicit multi-draw + camera frame API while keeping a distinct logical owner flag so lifecycles stay auditable. Rejected: Camera Preview RT borrow; main viewport steal.
2. **Private temporary SceneInstance tree** — Instantiate from on-disk Scene Asset (recursive children) without setting SceneSystem active. Rejected: snapshotting the live active instance.
3. **Reuse Play camera resolve** — `resolvePlayCamera` / `resolvePlayCameraFromScene` at aspect 1. Rejected: Editor Camera; Main-only without fallback.
4. **Fingerprint in cache stem/meta** — `_scv1` + mesh-ref fingerprint so old File placeholders and mesh-only edits invalidate. Rejected: scene mtime alone.
5. **Async queue unchanged** — Same ThumbnailGenerationQueue visible priority.

## Risks / Trade-offs

- [Heavy scene instantiate per thumb] → Async queue + cache; placeholder until done.
- [No formal Light components yet] → Studio fallback dominates until lights land.
- [Active scene already loaded] → Private tree avoids dirty/live bleed and unload races.
- [GPU backend unavailable in unit tests] → Logic tests without GPU; placeholder path covered; GPU harness optional.

## Migration Plan

- No Project content migration. First regenerate after upgrade replaces File-placeholder PNGs via `_scv1` stem change.
- Rollback: revert; Scene entries fall back to File placeholder.

## Open Questions

- Whether Scene Thumbnail shares Mesh Preview Vulkan pipelines with a distinct owner enum value vs fully separate RT instance — implementer picks separate RT instance (safer) if VRAM allows; otherwise shared pipelines with distinct owner tag and non-overlapping ensureResources calls.
