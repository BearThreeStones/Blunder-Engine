## Why

The Animation Window already previews the bound **AnimationTree**, but the timeline is only a ruler and playhead. Authors cannot see which bones and TRS keys the current ruler **AnimationClip** actually contains. v1 froze that on purpose; this pass adds read-only **Clip anatomy** so the window shows clip information without becoming a Godot AnimationPlayer editor.

## User stories

1. I select Chocomel (it has an AnimationTree). The Animation Window shows **Clip anatomy** for the current ruler AnimationClip: bones grouped by name, only the Position / Rotation / Scale rows the clip already has (Editor Icon + word), diamonds at existing key times, and a playhead vertical line through the ruler and visible rows.
2. I Fire a clip, or OneShot occupies, or Clip Play override changes the ruler clip. Anatomy switches to that clip's **clip tracks**. Bone groups all re-expand.
3. I type a bone name in the filter. Groups whose names do not contain that string hide. The document and the AnimationClip stay clean.
4. I collapse a bone group. The fold lives only in this Editor Session. Changing the ruler clip expands every group again. The document stays clean.
5. I press or drag in the timeline column (ruler + key lanes). The playhead seeks with today's ruler rule. Clicking a diamond does not select or drag a key. Clicking the name, filter, or fold column does not seek.
6. The clip has bones the host Skeleton does not. Those groups still appear, with no warning. I select an Object with no AnimationTree. The Animation panel stays open and disabled, with no anatomy.

## What Changes

- Replace the ruler-only timeline with **Clip anatomy**: read-only listing of **clip tracks** on the current ruler AnimationClip
- Group rows by bone name (first appearance in the clip); within a group Position → Rotation → Scale, omit missing channels
- Channel rows: Position / Rotation / Scale **Editor Icon** + that word; bone groups are bone name only
- Draw key-time diamonds on each clip-track lane; not selectable or draggable
- Playhead is one vertical line through the ruler and currently visible clip-track rows
- Seek only from the timeline column (ruler + key lanes), same rule as today's ruler
- Session bone-name filter (substring); does not dirty document or clip
- Session-collapsible bone groups, default expanded; re-expand when the ruler clip changes
- Unmatched Skeleton bones still listed; no badge, no hide
- Unbound selection: panel stays open and disabled; no anatomy

**Out of scope:** keyframe/track authorship; Method tracks; mute / interpolation / wrap / delete controls; Skeleton3D or scene-tree fake root; Clip Binding rows; Canvas; AnimationPlayer as host

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `animation-window`: Timeline is Clip anatomy (read-only clip tracks, grouping, diamonds, spanning playhead, column seek, session filter and fold) instead of ruler-and-playhead-only

## Impact

- `animation_window_panel.slint`: track list, filter, fold, diamonds, spanning playhead, split seek hit-testing
- `editor_icons.slint` / `EditorGlyphs`: wrap `KeyTrackPosition` / `KeyTrackRotation` / `KeyTrackScale` (and search if not already used in this panel)
- `AnimationPreviewController` (or a thin helper beside it): expose grouped clip-track rows + key times for the current ruler clip via existing `resolveAnimationClipFromAssets`
- Slint ↔ C++ model wiring in `slint_system` / animation window bind
- Tests: group/order/filter/fold/unmatched-bone/ruler-clip-switch against clip YAML, without UI authorship
- Docs: CONTEXT **Clip anatomy** / **Clip track** already locked in Grill
- Validation: `engine_editor` + Chocomel anatomy smoke; relevant first-party tests
- Human acceptance: walk the six User stories in the windowed editor
