## Context

See `proposal.md` for why. Grill locked **Clip anatomy** / **Clip track** in CONTEXT. The Animation Window already binds the selection's **AnimationTree**; `AnimationPreviewController` exposes ruler name, length, and playhead. Clip Intermediate YAML is `AnimationClipData` (`tracks[]` of bone + channel + keys). `resolveAnimationClipFromAssets` already loads that YAML.

Today `animation_window_panel.slint` is a 36px ruler. Channel kind SVGs exist at `engine/3rdparty/godot-icons/KeyTrackPosition.svg` (and Rotation / Scale) but are not in `EditorGlyphs`.

## Goals / Non-Goals

**Goals:**

- Build a read-only anatomy model from the current ruler clip and show it in the existing Animation dock.
- Keep transport, Fire, CINE, TimeScale, and ruler-clock source rules unchanged.
- Test grouping, order, filter, fold reset, and unmatched bones without a windowed editor.

**Non-Goals:**

- Keyframe authorship, Method tracks, mute/interpolation/wrap/delete chrome.
- A second preview controller or bringing back AnimationPlayer as host.
- An ADR unless a later pass asks for one (Grill offered it; it is not in this change).
- Virtualizing thousands of diamonds as a first slice; revisit only if Chocomel clips jank.

## Decisions

### D1 — Pure anatomy builder beside the preview controller
**Choice:** A small function (e.g. `buildClipAnatomy(const AnimationClipData&)`) groups tracks by first bone appearance and orders Position → Rotation → Scale. `AnimationPreviewController` (or the Slint bind path) resolves the current ruler clip GUID the same way playback already does, then calls that builder. Filter is applied when producing the view model (substring on bone name). Fold is a session set of collapsed bone names, cleared when `rulerClipName()` changes.
**Why:** Grouping is deterministic clip data; tests can feed YAML-shaped `AnimationClipData` without Slint. The window stays Tree-hosted.
**Rejected:** Parsing YAML in Slint; a new `AnimationWindowController`; listing from Skeleton bones instead of clip tracks.

### D2 — Same panel, split columns, one playhead
**Choice:** Extend `AnimationWindowPanel`: left column is filter + fold + bone/channel names; right column is ruler + key lanes. One playhead rectangle spans the right column. Existing ruler seek math (`playhead / clip-length`) is reused by a TouchArea covering the whole right column, including over diamonds.
**Why:** Spec requires one vertical playhead and timeline-column-only seek. Floating Animation already instances the same panel.
**Rejected:** A second dock; seeking from the name column; per-diamond hit targets.

### D3 — Channel icons keep Godot fill; do not gray-colorize
**Choice:** Add `track-position` / `track-rotation` / `track-scale` to `EditorGlyphs` from `KeyTrackPosition.svg` / `KeyTrackRotation.svg` / `KeyTrackScale.svg`. Draw them without `colorize` to theme icon gray so the orange / purple / yellow fills stay. Row text stays Position / Rotation / Scale.
**Why:** Grill: channel color follows that Godot set. `colorize` would flatten them to chrome gray.
**Rejected:** Reusing Move/Rotate/Scale tool glyphs; gray theme colorize.

### D4 — Session chrome stays off Document History
**Choice:** Filter string and collapsed-bone set live on the panel/controller session, same as Loop / playhead. Do not emit an Editor Command.
**Why:** Already grilled; TimeScale remains the only window control on history.
**Rejected:** Persisting fold in the scene or clip YAML.

### D5 — Unmatched bones are clip rows, not a Skeleton diff
**Choice:** The builder never consults Skeleton bone names. Extra clip bones appear; missing skeleton bones do not get empty tracks.
**Why:** Anatomy is "what is in this clip." Sampling already ignores unknown bones.
**Rejected:** Warning badges; hiding extras.

## Risks / Trade-offs

- [Dense clips → many diamond Rectangles in Slint] → First slice draws all keys on visible (non-filtered, non-folded) rows. If Chocomel walk janks, follow-up can virtualize without changing the spec.
- [Ruler clip GUID vs logical name] → Resolve through the bound tree's Clip Binding for `rulerClipName()`, same path playback uses. Empty binding → no anatomy rows, ruler still shows clock text.
- [Folded group still in first-appearance order] → Collapsed groups keep their place; only child channel rows hide.
- [Floating Animation panel] → Wire the same anatomy properties on `floating_panel_window.slint` that `editor_window.slint` already mirrors for transport.

## Migration Plan

1. Land builder + tests, then Slint anatomy + icon glyphs, then bind from the preview controller.
2. No content or clip YAML migration.

Rollback: revert the change; window returns to ruler-only.

## Open Questions

None.
