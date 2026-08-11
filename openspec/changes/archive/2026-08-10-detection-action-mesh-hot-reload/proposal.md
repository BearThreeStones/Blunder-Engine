## Why

Artists overwrite Source or Intermediate files from DCC tools, but Blunder today either silently auto-Reimports only `Resources/Source/` or merely invalidates Finals for Intermediate — so Intermediate-direct Assets (including companion glTF under `Resources/Animations/`) stay stale, and loaded Mesh previews do not update until restart or manual Reimport. ADR 0029 locks Unreal-style Detection Action plus editor Mesh hot reload so external saves become Prompt/Auto Reimport and the viewport reflects Mesh changes in-session.

## What Changes

- Unify Source-archive and Intermediate-direct disk changes under **Detection Action** (editor-user preference): default **Prompt** (one coalesced toast → Reimport All / Dismiss) or **Auto** (debounced Reimport).
- **BREAKING:** Replace today’s always-silent auto-Reimport for `Resources/Source/` with Detection Action (Prompt by default).
- Map watched paths to Asset GUID(s) via descriptor `source` / `archived_source`, including glTF sidecars (sibling `.bin`, glTF-relative textures) coalesced into one Detection event.
- Intermediate overwrite triggers Detection → **Reimport** (not Final-invalidate alone) for Intermediate-direct Assets.
- After successful Reimport (manual or Detection-driven), **Editor Asset Hot Reload** refreshes loaded Mesh presentation (AssetManager / viewport / Mesh Preview / placed scene meshes).
- Keep manual Reimport. Out of scope: Project-external watch roots, AnimationClip/AnimationPlayer live hot-swap, forced Cook on every Reimport, per-Project Detection preference, per-asset Prompt pickers.

## Capabilities

### New Capabilities
- `detection-action`: Asset Watch Detection Action (Prompt/Auto), path→GUID mapping including sidecars, coalesced Prompt UI, Intermediate-direct Reimport on change.
- `editor-mesh-hot-reload`: Session Mesh hot reload after successful Reimport (AssetManager + viewport/preview/scene mesh presentation).

### Modified Capabilities
- `asset-import`: Reimport remains available manually; Source auto-Reimport behavior becomes Detection-driven rather than always-silent; Intermediate-direct Reimport path must support watch-triggered refresh.
- `asset-pull-cook`: Asset Watch / invalidation interaction with Detection (invalidate still applies where Reimport is declined or fails; Watch no longer implies silent Source Reimport-only).

## Impact

- `ContentBrowserWatch` / `asset_watch_path` (classification, sidecar attribution, Prompt vs Auto, stop silent Source-only auto).
- `AssetImportService::requestReimport(s)` (Intermediate-direct + Source; Mesh vs Clip ownership per ADR 0028).
- Editor preferences (user-level Detection Action).
- Editor UI toast / notification for Prompt mode.
- `AssetManager` + viewport / Mesh Preview / scene mesh bind paths for hot reload.
- Docs: ADR 0029, CONTEXT glossary (already updated); OpenSpec specs below.
- Tests: watch→Detection→Reimport; sidecar coalesce; Prompt/Auto preference; Mesh hot reload after Reimport.
