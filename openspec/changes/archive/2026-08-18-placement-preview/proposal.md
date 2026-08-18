## Why

Dragging a Mesh Asset from the Content Browser into the viewport already spawns on drop, but the drag itself is blind: nothing follows the pointer in 3D, and the cursor stays the default arrow. Authors cannot tell where Ground placement will land or whether the current drop target is valid.

## What Changes

- Show a **Placement Preview** (transient, non-document mesh at Ground placement) while a Mesh Asset Content Browser drag is over the editor viewport.
- Preview shading matches the spawned MeshRenderer (opaque, Mesh Asset materials, scene lighting).
- Three-state **Content Browser drag cursor**: pointer over a valid viewport drop (Mesh or Scene Asset), move over a Browser folder, not-allowed otherwise.
- Escape cancels the drag (no spawn, no scene open, no reparent) and clears preview and cursor.
- OS file drop onto the viewport is **out of this slice**.

## Capabilities

### New Capabilities

- `placement-preview`: Placement Preview visibility, Ground placement follow, spawn-matching shading, Content Browser drag cursor, Escape cancel.

### Modified Capabilities

- `scene-edit-commands`: Spawn Command still seals only on successful drop; Placement Preview is not a spawn.
- `editor-viewport-pacing`: Placement Preview motion requests a viewport redraw under a static camera.

## Impact

- New editor helpers: Ground placement, drop classification / cursor resolve, Placement Preview controller.
- `EditorSceneEditSystem` spawn shares Ground placement with the preview.
- `syncSceneToRender` / engine tick appends preview draws (no Entity, no Outliner, no Document History).
- `SlintSystem` / `WindowSystem` drive preview pose, cursor, and Escape cancel.
- Unit tests for cursor, Ground placement, and preview visibility; manual editor QA for follow-mesh and cursor.
