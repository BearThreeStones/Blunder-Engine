## Why

The editor viewport and Player still shade from a process-global Blinn-Phong directional plus an ambient floor. Authors cannot place lights in the Scene Asset, New Scene has no light, and glossary promises (scene Light Components, Scene Thumbnail preferring them) are unimplemented. Viewport lighting is a hidden editor rig, not document state.

## What Changes

- Add a native **Light Component** (Unique attachment, like Camera): type field **Directional / Point / Spot / Area**; shared **Light color**, **Light intensity**, **Light enabled**, **Light contribution**; type-specific **Light range**, **Spot cone**, Area **width / height**
- Live shading (editor viewport, Player, Camera Preview, Placement Preview) uses only Light Components — **no hidden editor directional and no ambient floor**. Mesh Preview stays **Studio lighting**. Scene Thumbnail prefers Light Components, else Studio lighting
- All Light enabled lights that affect a MeshRenderer contribute (after **Light linking**), capped at **8 per MeshRenderer** (stable EntityId order). **Light shadows** this slice: at most one Directional
- **Light linking**: optional inclusive MeshRenderer receiver list on the Light Inspector section; empty = affect all
- **Light contribution**: Illuminate and shadows (default) / Illuminate only / Shadows only
- **Add… Light** (default type Directional); Inspector type change stays the same Unique; Add Light does not create an Object
- **New Scene** ships a second entity with a default Directional (above XY ground, emit axis slanted at the plane), not on the Main Camera entity
- **Light Gizmo** Editor Overlay (type-shaped wire; pick to select; no drag handles this slice; Player never shows it)

**Out of scope:** Scene environment / IBL; Point/Spot/Area shadows; Shadow linking; viewport light-link mode; LTC/GGX area specular; Light Gizmo drag handles; physical light units; Kelvin; four Add… Unique kinds for light types

## Capabilities

### New Capabilities

- `scene-light-component`: Light Component document model, types, live shading rules, linking, contribution, evaluation cap, shadows policy, New Scene default Directional, Inspector Light section
- `light-gizmo`: Light Component visualization and pick in the editor viewport

### Modified Capabilities

- `inspector-add-menu`: Light is a Unique attachment; Add… Light defaults to Directional; Add Light does not create an Object
- `inspector-present-only-sections`: Light Inspector section is present-only
- `editor-overlays`: Light Gizmo is authorship chrome; Player does not draw or hit-test it
- `placement-preview`: Placement Preview uses Light Components (no studio lighting, no hidden directional)
- `scene-edit-commands`: Add/Remove Light and Light property/linking edits are Document History Commands
- `camera-preview`: Camera Preview shades from Light Components in the previewed scene, not Studio lighting or a hidden editor light

## Impact

- `SceneInstance` / scene serializer / New Scene starter (`EditorSceneEditSystem::createNewSceneAsset`)
- `RenderSystem` / `ForwardRenderPath` / `basic.slang` (replace single `BlinnPhongEditorSettings` directional+ambient with up to 8 scene lights)
- Shadow map: one Directional by Light shadows rule
- Inspector Slint + `inspector_add_ops` Unique kind Light
- OverlaySystem Light Gizmo (draw + pick vs Camera Gizmo / mesh)
- Camera Preview / Placement Preview / Scene Thumbnail light gather
- Tests: serializer, Add… Unique, shading gather/cap/linking, shadow caster pick, New Scene starter, gizmo pick
- Glossary already updated in `CONTEXT.md`
