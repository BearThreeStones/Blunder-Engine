## Why

Edit animation preview still lives in a viewport overlay that talks to **AnimationPlayer** (S0/S1, BW, Fd, Sync Group `fireSameName`). Product chrome is the **Animation Window**: a persistent bottom dock on the selected **AnimationTree**. Overlay and Player-hosted preview contradict CONTEXT and block Tree Fire-slot / CINE session preview.

## What Changes

- Add a retileable dock panel **Animation** (own `DockPanelKind`), default bottom under the viewport
- Host Edit preview transport, TimeScale, Fire target, CINE Enter/End, in-CINE / Inp badges, and a ruler+playhead on that panel
- Drive pose through the bound **AnimationTree** (activate on Play; Fire = `RequestOneShot` on the tree **Fire slot**)
- Ruler clock = **base dominant clip**; insert clip while Fire/OneShot occupies
- Persistent dock: single selection + AnimationTree enables chrome; otherwise open but disabled
- Hierarchy rebind **Stop**s the previous tree (seek 0, clear Fire slot, End CINE) then binds the new Tree or empty
- Session Loop wraps the current ruler clip while Playing; off → Pause at last frame, tree stays active
- Stop = seek 0 + clear Fire slot + End CINE; tree stays active. Play from last-frame Pause starts at 0
- TimeScale is the same AnimationTree field as Inspector (dual-track, document-dirty). Only window control on Document History
- Transport / playhead / Loop / Fire occupancy / CINE marks are session-only (no dirty)
- Icon-first chrome on transport, Fire, Enter/End (ADR 0042)
- **Remove** the viewport overlay `AnimationPreviewToolbar` (S0/S1/BW/Fd). Transform strip stays

**Out of scope:** AnimationTree Canvas inside this window; Clip Binding rows; Travel/BlendSpace/Add2; keyframe tracks; Sync Group multi-object Edit; Camera Preview pin; Godot auto-show/hide

## Capabilities

### New Capabilities
- `animation-window`: Docked Edit animation preview on the selection's AnimationTree — bind/disable, transport, ruler clock, Fire slot, CINE session marks, TimeScale history, Icon-first chrome

### Modified Capabilities
- (none in archived main specs — `editor-overlays` is OverlaySystem 3D chrome, not this Slint toolbar)

## Impact

- New Slint panel; `editor_window.slint` dock tile for the new panel kind; delete overlay `AnimationPreviewToolbar`
- `DockPanelKind` + default layout in `slint_system.cpp` dock bootstrap
- `AnimationPreviewController`: bind requires AnimationTree; play/stop/loop/seek/Fire/CINE against the tree, not AnimationPlayer two-slot
- `SlintSystem::fireAnimationSyncPreview` must not be the window Fire path (bound-tree `RequestOneShot`)
- TimeScale commits through the same Inspector dual-track Command as AnimationTree TimeScale
- Open change `modern-dark-editor-theme` still requires animation preview as a viewport overlay — apply updates that delta when removing the toolbar
- Docs: CONTEXT **Animation Window** / **Icon-first chrome** already locked; ADR 0042
- Mockup: `docs/mockups/animation-window-v1.html`
- Validation: `engine_editor` + Chocomel bind / Camera unbind smoke
