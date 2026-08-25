## Why

Entity Inspector still hosts a process-global Blinn-Phong / SSAO block (Editor controls sliders) that ignores selection, is not a Light Component, and is not a Material. Live shading still copies `kd`/`ks`/shininess/unlit from that bag even when the viewport is PBR. Authors have no Mesh-side Material Inspector, so surface authorship has nowhere legal to live.

## What Changes

- **BREAKING (authoring UX):** Remove the Entity Inspector Blinn-Phong / SSAO block (including Sync Asset / Reset on that block). It is not a product surface.
- Stop applying **Editor shading overrides** to the editor viewport, Player, Camera Preview, Placement Preview, Mesh Preview Render, and Scene Thumbnail Render. Lights stay Light Components (or Studio lighting on preview/thumbnail paths already specified). SSAO stays off until Scene environment.
- When a draw has no MaterialAsset, use **Mesh shading defaults** (white albedo/diffuse, specular 0.4, shininess 32, ambient 0, not unlit). No hidden ambient floor.
- Add **Material Inspector** on Mesh Asset Inspector: Unlit, Base Color, Metallic, Roughness, Ambient, Diffuse, Specular, Shininess, and four texture slots. Rows are Inspector property fields / Inspector object cells — not Editor Slider/Toggle/Color Field/Object Field.
- Persist edits as a **sparse Mesh material override** on the Mesh descriptor, overlaid on the Import-built MaterialAsset for the Mesh Asset’s one surface (`loadMesh` / `getMaterialAsset()`, glTF first primitive). Extra primitives stay Import-built. No Material Asset Content Browser type. No glTF/Source writes.
- Reimport rebuilds Import MaterialAsset and **keeps** the override bag. `texture_guids` = Import-discovered GUIDs ∪ non-empty override-slot GUIDs. Empty slot key suppresses Import texture; does not delete the Texture Asset.
- Each committed field and **Reset overrides** is a **Global Command**. Focus-routed Undo targets Global History when Inspector is in Asset Inspector (or Content Browser focused).
- **Reset overrides** (Inspector chrome, Editor control) deletes the whole sparse bag. It is not Reimport. Per-field Revert is out of this slice.

## Capabilities

### New Capabilities

- `mesh-material-inspector`: Material Inspector on Mesh Asset Inspector; sparse Mesh material override; Inspector object cells; Reset overrides; Global Commands for those writes; first-primitive-only overlay

### Modified Capabilities

- `inspector-transform`: Drop the “shading stays unchanged” carve-out; Entity Inspector MUST NOT host Editor shading overrides
- `scene-light-component`: Live and preview BRDF from MaterialAsset or Mesh shading defaults; never from Editor shading overrides; SSAO off until Scene environment
- `asset-import`: Mesh Reimport preserves the Mesh material override bag
- `asset-pull-cook`: `texture_guids` includes non-empty override-slot GUIDs
- `document-history`: Focus-routed Undo also uses Global History when Inspector is in Asset Inspector

## Impact

- Slint: `inspector_panel.slint` (remove shading block; add Material Inspector); possibly `editor_window.slint` / `floating_panel_window.slint` preview-settings bindings; Inspector object cell primitive
- Render: `forward_shading.cpp` (`applyBlinnPhongToMeshUniforms` / `applyPbrToMeshUniforms` stop taking editor kd/ks/shininess/unlit for live views); `render_system.cpp` stop copying Slint bag into `frame_state.shading` for live draws; Mesh Preview / Scene Thumbnail already Studio/lights — confirm they do not read the bag
- Assets: Mesh descriptor YAML + `MeshAssetDescriptor`; load overlay; `texture_guids` union; MaterialAsset application on `loadMesh` / first primitive
- History: new Global Commands; `SlintSystem` / focus routing (Asset Inspector)
- Docs: `CONTEXT.md` already grilled; optional short ADR if implementers need a durable lighting/material split pointer
- Tests: descriptor overlay/sparse keys, `texture_guids` union, Reimport keeps bag, Reset clears bag, live view ignores editor bag, focus-routed undo
- Out of scope: Material Asset type; per-primitive override list; per-field Revert; Scene environment / SSAO authorship; writing glTF/Source
