## 1. Sparse override on Mesh descriptor

- [x] 1.1 Add `material_override` sparse fields to `MeshAssetDescriptor` (optional scalars/colors/unlit + four slot GUID strings with empty = suppress)
- [x] 1.2 YAML round-trip: absent map means no keys; empty slot string ≠ missing key; unknown sibling keys still round-trip
- [x] 1.3 Rebuild `texture_guids` as Import-discovered GUIDs ∪ non-empty override slot GUIDs on write and after Import/Reimport overlay merge
- [x] 1.4 Tests: parse/write sparse bag; empty slot keeps Import GUID in `texture_guids`; new slot GUID is unioned

## 2. Load overlay and live BRDF

- [x] 2.1 After Import builds the Mesh Asset’s one MaterialAsset (`loadMesh` / first primitive), apply `material_override` (factors, unlit, textures by GUID)
- [x] 2.2 Extra primitive MaterialAssets used by Mesh Preview / scene import stay Import-built (no bag stamp)
- [x] 2.3 `applyBlinnPhongToMeshUniforms` / PBR path: kd/ks/ka/shininess/unlit from MaterialAsset or Mesh shading defaults; never from Editor shading overrides
- [x] 2.4 Studio light pack may still use `BlinnPhongEditorSettings` light dir/color on Mesh Preview / no-light Scene Thumbnail only
- [x] 2.5 Stop copying Inspector BRDF/SSAO sliders into live `frame_state.shading`; keep SSAO off
- [x] 2.6 Tests: overlay then Reimport keeps keys; no MaterialAsset → white / spec 0.4 / shininess 32 / ka 0 / not unlit; live draw ignores leftover editor bag

## 3. Evict Entity Inspector shading UI

- [x] 3.1 Remove Blinn-Phong / SSAO block, Sync Asset, and Reset from Entity Inspector Slint (and floating-window copies of those bindings)
- [x] 3.2 Delete dead Slint properties/callbacks once no remaining consumer

## 4. Material Inspector chrome

- [x] 4.1 Add Inspector object cell (22px recessed Godot row: name or None, pick, clear, Content Browser Texture drop) — do not use Editor Object Field
- [x] 4.2 Mesh Asset Inspector section: Unlit, Base Color, Metallic, Roughness, Ambient, Diffuse, Specular, Shininess, four slots, Reset overrides (Editor control chrome)
- [x] 4.3 Bind fields to the selected Mesh descriptor bag; live-refresh Mesh Preview and in-scene draws of that Mesh GUID after overlay apply

## 5. Global History

- [x] 5.1 Global Command: before/after sparse bag (+ `texture_guids`); write descriptor; reload overlay; one Command per committed field (focus loss / Enter / slot pick, clear, drop)
- [x] 5.2 Reset overrides: one Global Command that deletes the bag (not Reimport, not Texture delete)
- [x] 5.3 Focus-routed Undo: Inspector + Asset Inspector → Global History (same router as Content Browser)
- [x] 5.4 Tests: undo field commit restores bag; undo Reset restores bag; Ctrl+Z from Asset Inspector does not pop Document History

## 6. Reimport and graph

- [x] 6.1 Mesh Reimport rebuilds Import MaterialAsset then applies the existing bag; does not drop keys
- [x] 6.2 Dependency graph uses updated `texture_guids` (override-only textures invalidate)

## 7. Validate

- [x] 7.1 Build `engine_editor` (`docs/agents/build.md`); kill `engine_editor` / `engine_player` if the Player POST_BUILD copy is locked
- [x] 7.2 Run tests from 1.4, 2.6, 5.4
- [x] 7.3 Manual: Entity Inspector has no shading sliders; Mesh Asset Inspector edits first primitive only; extra primitives unchanged; Reset vs Reimport; SSAO stays off; viewport lights are Light Components
