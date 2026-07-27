## Context

Player already hides Editor Overlays (`editor_overlay_policy`). Authorship still reaches `EditorCamera::onEvent` and viewport pick; Player ticks still use Editor Camera matrices. No Camera Component exists on scene entities.

## Goals / Non-Goals

**Goals:**

- `playerAuthorshipInputEnabled(Player) == false` gates Editor Camera, pick, and authorship shortcuts in Player.
- `CameraComponent` on entities; JSON `"camera"` round-trip; load attach; Inspector FOV/near/far/Main + Add Camera.
- `resolvePlayCamera` / `resolvePlayCameraFromScene`: Main wins, else first; empty → not ok.
- Player `tickVulkan` uses resolved Camera only; Edit Mode unchanged Editor Camera.
- `runPlayCameraGate` before Player spawn; fail message when no Camera.

**Non-Goals:**

- Camera-follow Behaviour / DogWalk follow
- Camera gizmo toolbox / editor viewport forced to Main Camera
- Live sync of editor edits into running Player
- Env override to re-enable Player authorship input

## Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Isolation packaging | One change with Camera | Same product story |
| Editor Camera in Player | Off always (incl. Pause) | Not Pause-only orbit |
| View source | Scene Camera Component only | User-mandated; no temporary EditorCamera read-only |
| Multi-camera | Main flag, else first | Simple deterministic |
| Missing camera | Play preflight fail | No spawn / no silent fallback |
| Authoring MVP | Serialize + thin Inspector + Add | No toolbox |

## Risks / Trade-offs

- [Preflight needs loaded Scene asset] → Prefer AssetManager load of entry scene before spawn; fail closed.
- [Save path forgets cameras] → Export must copy `getCamera` into definition/`"camera"` JSON.
- [Vulkan Y flip / look axis] → Match existing RenderSystem perspective + glTF -Z look.

## Migration Plan

Seed Main Camera on Test Play entry scene. Scenes without camera cannot Play until author adds one.

## Open Questions

None — grilled decisions locked in plan `docs/superpowers/plans/2026-07-27-player-gameplay-camera.md`.
