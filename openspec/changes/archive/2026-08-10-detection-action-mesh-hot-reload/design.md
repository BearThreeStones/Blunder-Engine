## Context

ADR 0029 and CONTEXT define **Detection Action**, **Asset Watch**, **Reimport**, and **Editor Asset Hot Reload**. Today `ContentBrowserWatch` (efsw) already watches `Assets/` + `Resources/`, debounces (~300ms), supports `suppressNotificationsFor`, invalidates Finals for Intermediate/descriptor changes, and **silently** auto-Reimports Source-archive hits via `findGuidsByArchivedSource` + `requestReimports`. Intermediate-direct overwrites (common for glTF under `Resources/Models|Animations|…`) mostly invalidate only — Clip YAML / derived data stay stale. Loaded Mesh GPU/session state does not refresh after Reimport.

Constraints: keep manual Reimport; ADR 0028 Mesh vs Clip Reimport ownership; no new DCC watch roots outside the Project in this change; editor-user preference home (not Project).

## Goals / Non-Goals

**Goals:**
- One Detection pipeline for Source-archive and Intermediate-direct (plus glTF sidecars).
- Detection Action preference: Prompt (default, coalesced) or Auto.
- Stop silent always-Auto Source Reimport.
- After successful Reimport, Mesh session hot reload (AssetManager + viewport / Mesh Preview / placed meshes).

**Non-Goals:**
- External Watch Root directories.
- AnimationClip / AnimationPlayer live hot-swap.
- Forced Cook on every Reimport.
- Per-Project Detection preference; per-Asset Prompt checklists.
- Rewriting obsolete COLLADA-only wording in legacy main specs beyond what this change requires.

## Decisions

1. **Detection sits on Asset Watch, does not replace Reimport**  
   Watch attributes paths → GUID set; Detection Action chooses Prompt vs `requestReimports`. Manual Reimport remains.  
   _Alt rejected:_ Watch-only updates without Reimport API.

2. **Intermediate-direct uses descriptor `source` mapping (parallel to `archived_source`)**  
   Extend path→GUID beyond Source archive so `Resources/Animations/…/*.gltf` (and Models textures) trigger the owning Asset(s). Sidecars (sibling `.bin`, glTF-relative textures) attribute to the same GUID set and coalesce in one debounce window.  
   _Alt rejected:_ Invalidate-only for Intermediate; whole-directory stem Reimport.

3. **Default Prompt; Auto optional; user-level preference**  
   Persist with other editor preferences. Source and Intermediate share the same Action (breaking vs today’s Source silent Auto).  
   _Alt rejected:_ Source forever Auto; Project-level preference in v1.

4. **Prompt UX = one coalesced toast**  
   “N assets updated” → Reimport All / Dismiss.  
   _Alt rejected:_ Per-asset toasts; v1 multi-select picker.

5. **Hot reload = Mesh session only after successful Reimport**  
   Reload/rebind Mesh through AssetManager; refresh viewport, Mesh Preview, scene-placed meshes. Clip playback hot-swap and immediate Cook deferred. If Prompt is Dismissed, no hot reload (optional invalidate-only remains for safety).  
   _Alt rejected:_ Treat invalidate alone as “hot reload”; include Clip swap in v1.

6. **Self-write suppression stays**  
   Import/Reimport engine writes continue to call `suppressNotificationsFor` so Detection does not re-enter on our own writes.

## Risks / Trade-offs

- [Breaking Source silent Auto] → Mitigation: document in release notes; preference switch to Auto restores prior feel.
- [Prompt fatigue on large batch exports] → Mitigation: coalesce + debounce; Auto available.
- [Wrong GUID from shared Intermediate path] → Mitigation: map all descriptors whose `source`/`archived_source` (or sidecar parent) match; Reimport each; log ambiguities.
- [Hot reload tears / half-updated GPU buffers] → Mitigation: serialize reload on editor thread; fail soft leave previous mesh + log.
- [Clip Intermediate Reimport without Clip hot reload] → Mitigation: accepted; YAML/Final refresh still helps next play; Clip hot-swap later.

## Migration Plan

1. Ship Detection preference defaulting to Prompt (behavior change for Source watchers).
2. No on-disk Project migration required.
3. Rollback: preference Auto approximates old Source behavior; Intermediate Detection can be feature-flagged if needed.

## Open Questions

- Exact editor preference storage key / UI panel placement (Inspector vs dedicated Editor Settings dock) — resolve during apply.
- Whether Dismissed Prompt should still invalidate Finals (recommend yes) — confirm in UI pass.
