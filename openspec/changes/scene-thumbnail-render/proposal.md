## Why

Content Browser shows `.scene.asset` entries with a generic File placeholder, so authors cannot tell scenes apart in the grid. Mesh Assets already get **Mesh Preview Render** stills; Scene Assets need a comparable **Camera still** product image without coupling to the live editor viewport or Camera Preview panel.

## What Changes

- Introduce **Scene Thumbnail Render**: dedicated offscreen still of an on-disk Scene Asset through the Play camera resolve rule (Main, else first), square aspect, recursive childScenes, no Editor Overlays, bind/rest pose only.
- Wire **ThumbnailGenerator** so `.scene.asset` entries produce Scene Thumbnail stills into the existing thumbnail cache (async, visibility-prioritized); failure → Scene placeholder.
- Cache invalidation: scene file mtime plus fingerprint of direct Mesh Asset References on root and recursive child scenes; cache stem suffix to retire old File-placeholder PNGs.
- Prefer scene lights when present; otherwise fixed studio lighting (same fallback spirit as Mesh Preview).
- Docs already grilled: CONTEXT **Scene Thumbnail Render** + [ADR 0024](../../../docs/adr/0024-scene-thumbnail-render-offscreen.md).

## Capabilities

### New Capabilities
- `scene-thumbnail-render`: Dedicated offscreen Scene Asset still generation, camera resolve, mesh-ref fingerprint cache invalidation, ThumbnailGenerator integration, Scene placeholder fallback.

### Modified Capabilities
- (none at main-spec level for this slice; thumbnail behavior is owned by the new capability.)

## Impact

- Code: `ThumbnailGenerator` / `ThumbnailCache` / placeholders; new Scene Thumbnail Render service (+ optional offscreen backend); temporary SceneInstance load path that must not clobber the active editor scene; Play camera resolve reuse.
- Docs: CONTEXT + ADR 0024 (already written during grill).
- Tests: fingerprint stability, no-camera → placeholder, camera resolve preference, generator routes `.scene.asset` (unit where GPU optional).
- Non-goals: dirty live SceneInstance as cache source; AnimationPlayer sampling; authored cover images; sharing Camera Preview / main viewport RTs; HDRI.
