## Context

See proposal.md for motivation. CONTEXT **Animation Window**, **Fire slot**, **Edit animation preview**, **Icon-first chrome**; ADR 0042. Visual: `docs/mockups/animation-window-v1.html`.

Today Edit preview is `AnimationPreviewToolbar` over the viewport. `AnimationPreviewController` binds Objects that have **AnimationPlayer**, advances via `tickObjectAnimationPreviewFrame`, and TimeScale/slots write the Player. Overlay **Fire** is `SlintSystem::fireAnimationSyncPreview` → Sync Group `fireSameName`. Overlay CINE uses `AnimationSyncCinePreviewController`. Tree `RequestOneShot` already exists on the preview controller but is not the overlay Fire path.

AnimationTree still samples Player `TimeScale` (`animation_player_time_scale` on the scene entity). Inspector has no separate Tree TimeScale widget yet; the product field is the rate the tree already consumes.

Dock kinds today: `custom`, `viewport`, `hierarchy`, `inspector`, `content_browser`. Console uses `custom`. Default layout is built in `SlintSystem` dock bootstrap.

## Goals / Non-Goals

**Goals:**
- New dock panel kind + default bottom split
- Rehost preview on the bound AnimationTree
- Session Loop / CINE flags on the preview controller
- Fire = `requestOneShot` on that tree
- Remove overlay toolbar; keep TransformToolbar
- TimeScale commit dirties via the existing persisted rate field the tree reads
- Icon-first transport using `EditorGlyphs` / Godot SVGs

**Non-Goals:**
- Moving TimeScale serialization from Player to a new Tree-only YAML key
- Sync Group Edit in this window
- Canvas, Clip Binding rows, Travel/BlendSpace/Add2 chrome
- Changing Application Bar Play session (game Play), only Animation Window transport

## Decisions

### D1 — Own `DockPanelKind::animation`
**Choice:** Add `animation` to `DockPanelKind`. Default: horizontal split under viewport (Godot Animation-panel rhythm). Title "Animation". Retile/float through existing dock manager.
**Why:** `custom` shares Console's kind; selection/focus/snapshot need a distinct kind like Hierarchy.
**Rejected:** Reusing `custom`; viewport-bottom Slint strip that is not a dock widget.

### D2 — Extend `AnimationPreviewController`, do not add a second controller
**Choice:** Bind requires `hasAnimationTree()` (not Player-only). Play activates the tree then ticks tree preview. Stop/seek/Fire/CINE/Loop live on this controller. Keep Player helpers used by existing tests until those tests retarget; window path must not call `play()` as AnimationPlayer.Play.
**Why:** Tests and `tick()` already sit here; a parallel controller would double CINE/Fire.
**Rejected:** New `AnimationWindowController`; driving overlay Player two-slot as a compatibility shim.

### D3 — Window Fire is `requestOneShot` on the bound tree
**Choice:** Fire-target dropdown = Clip Binding logical names on that tree. Fire calls `AnimationPreviewController::requestOneShot`. Do not call `fireAnimationSyncPreview` / `fireSameName` from this chrome.
**Why:** CONTEXT Fire slot; overlay Sync path is the bug.
**Rejected:** Sync Group sugar in v1; Fire as Travel.

### D4 — Session Loop on the controller, not `AnimationPlayer.m_loop`
**Choice:** `m_session_loop` (or equivalent) on the preview controller. Wrap current ruler clip while Playing. Off → Pause last frame. Do not write Player loop or Clip Asset loop.
**Why:** Grilled: Loop is Edit preview wrap, not durable.
**Rejected:** Toggling `player->setLooping`.

### D5 — TimeScale writes the rate the tree already samples
**Choice:** Slider dual-tracks Inspector when that row exists; commit calls the same `setTimeScale` the tree reads (Player TimeScale today) and seals a Document Command / dirty like other Inspector numeric commits. Transport does not go through that Command.
**Why:** Product is one field; do not fork a preview-only scale. Relocating YAML to Tree-owned TimeScale is a later change.
**Rejected:** Overlay-style `applyAnimationPreviewParams` that `markDirty`s on every slider tick including BW/Fd (those controls go away).

### D6 — CINE marks on the bound preview, not Sync Group membership
**Choice:** `enterCine` / `endCine` session flags on `AnimationPreviewController` (in-CINE + input-suppression). Window Enter/End do not require Sync members. Stop/rebind call End.
**Why:** Overlay CINE is tied to `AnimationSyncCinePreviewController` members; v1 window is single-tree.
**Rejected:** Requiring `hasMembers()` before Enter; Enter Firing a clip.

### D7 — Ruler clock from tree sample, not Player playbackPosition
**Choice:** While Fire/OneShot occupies, length/position come from the insert clip; otherwise from the base dominant clip (same clock Animation step uses). Scrub seeks that clock.
**Why:** Overlay `playbackPosition` is Player.
**Rejected:** Ruler tied to Fire-target dropdown while Idle is still dominant.

### D8 — Icon-first via existing EditorGlyphs
**Choice:** Play/Pause/Stop from `EditorGlyphs`; add Loop (`Loop.svg`), Fire (`PlayStart.svg`), Enter CINE (`Camera3D.svg`), End CINE (`TransitionEnd.svg` or PlayEnd bar), TimeScale chrome `Time.svg`. `EditorSlider` for TimeScale (same as Content Browser thumb size).
**Why:** ADR 0042; mockup already matches.
**Rejected:** Word buttons; native `<input type="range">`.

### D9 — Drop overlay toolbar in the same change
**Choice:** Remove `AnimationPreviewToolbar` from `editor_window.slint` and stop wiring S0/S1/BW/Fd. Update unarchived `modern-dark-editor-theme` editor-shell delta so animation preview is not required as a viewport overlay.
**Why:** CONTEXT forbids a second overlay beside the window.
**Rejected:** Hiding the overlay with `visible: false` while leaving S0 state.

## Risks / Trade-offs

- [Existing preview tests assume Player bind] → Retarget window-path tests to Tree; keep Player helpers only where Phase 2 tests still need them
- [No Inspector TimeScale widget yet] → Window still writes Player TimeScale the tree samples; add Inspector dual-track when that row exists, do not block the dock
- [Default dock layout vs saved layout] → New kind appears in default bootstrap; existing saved layouts without the widget get it on next default reset or first-run inject — pick inject-if-missing so old layouts gain the panel
- [CINE flags vs Sync controller] → Window marks must not desync Overlay leftovers; overlay is deleted
- [LNK1168 if editor is running] → Kill `engine_editor` before rebuild

## Migration Plan

1. Dock kind + empty panel in default layout
2. Rehost controller bind/play/stop/loop/clock on Tree
3. Fire + CINE + TimeScale commit
4. Slint chrome (icons, EditorSlider, ruler)
5. Delete overlay toolbar; fix theme delta
6. Tests + `engine_editor` smoke (Chocomel / Camera)

Rollback: revert the change folder's code; no scene format change.

## Open Questions

None — grilling locked bind, clock, Fire, Loop, Stop, CINE, dock, history, Icon-first.
