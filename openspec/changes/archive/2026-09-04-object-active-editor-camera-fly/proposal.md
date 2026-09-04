## Why

Authors cannot turn an Object off in the scene document the way Unity's GameObject checkbox does, so isolating meshes means deleting or hoping viewport pick finds the right GEO. Separately, Editor Camera WASD flies whenever the pointer is over the viewport, so typing in Hierarchy or Inspector (including the new Active shortcut A) also strafes the view.

## What Changes

- Persist **Object Active** on the Scene Asset (Unity `activeSelf`). Hierarchy A (pointer over Hierarchy), Hierarchy checkbox, and Inspector identity checkbox author the same field as one Document History Command. Multi-select aligns to one value (all off if every selected Object is Active, otherwise all on). Parent toggle does not rewrite descendants' flags.
- **Active in Hierarchy** (derived) gates draw, viewport pick, outline/gizmo, Light contribution, Play Camera resolve, and Behaviour Tick. Inactive Objects stay in the Hierarchy. Not Scene Visibility ([ADR 0053](../../../docs/adr/0053-object-active-not-scene-visibility.md)).
- **Object Active** is a v1 **Play authorship patch**.
- **Editor Camera fly** (WASD / Q / E, Shift sprint) runs only while right or middle mouse was pressed in the viewport and is still held. Hover-only fly is removed. Gameplay Input WASD in the Player is unchanged.

## User stories

1. Pointer over Hierarchy, select an Object, press A (or click the row checkbox): only that Object's Object Active flips. The name greys or restores from Active in Hierarchy; the mesh leaves or returns in the viewport; children's checkboxes stay. Inline Rename types the letter A.
2. Multi-select, press A or click a checkbox: if every selected Object is Active they all turn off; otherwise they all turn on (mixed becomes all on). Inspector identity checkbox is the same field; mixed selection is indeterminate.
3. Uncheck a parent: descendants keep their own checkboxes but names grey, viewport pick misses them, no gizmo/outline, their lights do not illuminate, Behaviours do not Tick. Grey Hierarchy rows remain selectable.
4. Uncheck an Object, Save, reopen: it stays inactive. Play does not draw it or Tick it. If Main Camera is not Active in Hierarchy, Play fails with no available Camera — no Editor Camera fallback.
5. While Play is running, change Object Active in the editor: the Player window updates without Reload. Undo/Redo patches both sides.
6. WASD / Q / E (Shift sprint) move the Editor Camera only while middle or right mouse was pressed in the viewport and is still held; middle-mouse pan still runs. Those keys do not fly the camera from Hierarchy or Inspector, or from viewport hover without that button.

## Capabilities

### New Capabilities

- `object-active`: Persisted Object Active, derived Active in Hierarchy, Hierarchy/Inspector authoring, Document History Command, and participation (draw, pick, gizmo, Light, Play Camera, Tick).
- `editor-camera-fly`: Editor Camera WASD/Q/E fly gated on viewport-started right or middle mouse hold.

### Modified Capabilities

- `hierarchy-panel`: Hierarchy Active checkbox, grey names when not Active in Hierarchy, A key while pointer is over the panel.
- `play-authorship-patch`: v1 catalog includes Object Active.
- `play-camera`: Play Camera resolve skips Cameras that are not Active in Hierarchy.
- `viewport-mesh-pick`: Viewport pick skips Objects that are not Active in Hierarchy.
- `scene-light-component`: Lights on Objects that are not Active in Hierarchy do not contribute.

## Impact

- **Engine:** Scene serialize/instantiate (`Object Active` distinct from tombstone `Entity.enabled`), gather/draw, pick, Light eval, Play Camera resolve, Behaviour Tick, Hierarchy/Inspector Slint, Editor Command, Play authorship patch payload, `EditorCamera::onUpdate` fly gate.
- **Tests:** Object Active persist/inherit/command; pick/light/camera skip; Editor Camera fly gate.
- **Docs:** [ADR 0053](../../../docs/adr/0053-object-active-not-scene-visibility.md); `CONTEXT.md` already names the terms from Grill.
- **Non-goals:** Scene Visibility / Hierarchy eye; using tombstone `enabled` as this flag; rewriting descendants' Object Active; Gameplay Input WASD; Player Editor Camera; checkbox on Light enabled as Object Active.
