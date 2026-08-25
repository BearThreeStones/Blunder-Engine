## 1. Light Component data + serialize

- [x] 1.1 Add `LightComponent` (type, color, intensity, enabled, contribution, range, cone, area size, linking EntityIds) and `SceneInstance` get/set/clear/forEach parallel to Camera
- [x] 1.2 Scene JSON `"light"` round-trip; persist linking as entity names; missing `"light"` → no component
- [x] 1.3 Tests: serializer round-trip all types; absent key; linking names resolve; stale names ignored

## 2. Gather, cap, shadows, falloff (TDD)

- [x] 2.1 Pure gather: Light enabled, linking (empty = all), contribution, EntityId order, cap 8 per MeshRenderer
- [x] 2.2 Shadow caster: first Light enabled Directional whose contribution includes shadows; none → shadows off
- [x] 2.3 Emit axis local -Z → world; Directional/Spot L = -emit; Point/Spot inverse-square to 0 at range
- [x] 2.4 Tests: empty linking includes new mesh; non-empty excludes; ninth dropped; two Directionals one shadow; disabled ignored; beyond range; identity emit is world -Z

## 3. Add… Unique Light + history

- [x] 3.1 `InspectorUniqueKind::Light`; Add defaults Directional; does not create Object; Remove restores snapshot
- [x] 3.2 Document History Commands for Add/Remove Light and Inspector field/linking commits (type change is same Unique)
- [x] 3.3 Tests: undo Add Light; Add Light on mesh-only entity has no Object; Unique already-present; undo type change

## 4. Forward shading (no hidden editor light)

- [x] 4.1 Replace live-view `BlinnPhongEditorSettings` directional + ambient floor with gathered lights (editor viewport, Player, Camera Preview, Placement Preview)
- [x] 4.2 `basic.slang` / `pbr.slang` / skinned variants: up to 8 lights; ambient 0 in live views; one directional shadow map from 2.2
- [x] 4.3 Area: front-face rectangle samples, not a point; Mesh Preview stays Studio lighting; Scene Thumbnail prefers lights else studio
- [x] 4.4 Build `engine_editor`; smoke a scene with the New Scene Directional once 5.x exists

## 5. New Scene starter Directional

- [x] 5.1 `createNewSceneAsset`: second entity with Directional, not on Main Camera; above XY; local -Z slanted at the ground
- [x] 5.2 Test: New Scene export has Main Camera + separate Directional Light

## 6. Inspector Light section

- [x] 6.1 Add… picker Light row + kind icon; present-only Light section; Remove
- [x] 6.2 Fields: type, color, intensity, enabled, contribution, range, inner/outer, width/height, linking receiver list
- [x] 6.3 `slint_system` dispatch; Camera+Light may coexist on one entity

## 7. Light Gizmo

- [x] 7.1 Draw all lights (Directional arrow / Point range sphere / Spot outer cone / Area rect); muted vs selection; `editorOverlaysEnabled`
- [x] 7.2 Pick before mesh; vs Camera Gizmo closer wins; Player host ignores draw and pick
- [x] 7.3 Tests: pick priority helpers; Player overlay gate includes Light Gizmo

## 8. Docs / validation

- [x] 8.1 Confirm `CONTEXT.md` Light terms match shipped behavior (prefer no churn)
- [x] 8.2 Build `engine_editor`; run focused tests from 1.3, 2.4, 3.3, 5.2, 7.3
- [x] 8.3 Manual: New Scene is lit; delete Directional → dark; Add Point with linking; Shadows only Directional; Light Gizmo pick; Camera Preview / Placement Preview use scene lights; Player has no Light Gizmo
