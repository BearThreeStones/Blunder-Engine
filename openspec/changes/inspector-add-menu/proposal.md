## Why

Authors cannot add AnimationPlayer, Skeleton, or AnimationTree from the Inspector, and Camera / Behaviour / Skeleton Modifier each have a separate Add button. Scene assembly for animation still depends on hand-edited JSON, while the dual-track runtime (ClassDB + ECS) is already in place (ADR 0003, 0033, 0034).

## What Changes

- Replace parallel Add Camera / Add Behaviour / Add Skeleton Modifier buttons with one Inspector **Add…** picker (Unity-like gesture; not Add Component, not Add Node)
- First-slice items: Unique attachments (Camera, Skeleton, AnimationPlayer, AnimationTree — present rows stay visible and disabled); Behaviour types from the Behaviour type catalog; SkeletonModifier types
- Grouped flat list, no search; exactly one selected entity; Mesh stays Content Browser spawn
- Host cascade: AnimationTree implies AnimationPlayer implies Skeleton; adding Skeleton does not create Player or Tree; new Tree is empty and inactive with no Asset GUID required
- Object materialization for ClassDB-hosted attachments; Camera does not create an Object
- Skeleton hydration from the selected entity’s skinned mesh Intermediate glTF onto the same Object; static/failed reads yield an empty Skeleton (warn, do not fail)
- Animation Player section **Add clip** (empty name→GUID row); clips are not Add… items; Import still does not auto-fill the map (ADR 0031)
- Remove attachment: Unique attachments have section Remove; no reverse cascade; Remove Skeleton disabled while Player, Tree, or any SkeletonModifier remains
- Each Add… click, Remove attachment, Add clip, and clip-row Remove is one Document History Command (cascade + hydration included)

**Out of scope:** multi-select Add…; Content Browser drop onto clip GUID; Add… type-ahead search; creating an AnimationTree Asset from Add…; expanding `importUnderEntity` children on Add Skeleton; lights/physics/Mesh in Add…

## Capabilities

### New Capabilities
- `inspector-add-menu`: Inspector Add… picker, Unique attachments, host cascade, Object materialization, Skeleton hydration, Add clip, Remove attachment, replacement of parallel Add buttons

### Modified Capabilities
- `scene-edit-commands`: Add… / Remove attachment / Add clip / clip-row Remove are EntityId Document History Commands; one click is one Command including cascade and hydration

## Impact

- Slint Inspector (`inspector_panel.slint`) + `slint_system` sync/apply
- Editor Commands + factories (Camera, Skeleton, AnimationPlayer, AnimationTree, clip rows; reuse Behaviour / SkeletonModifier commands)
- `SceneInstance::ensureBoundObject`; Skeleton hydration from mesh glTF (`populateSkeletonFromSkin` or equivalent)
- Scene serialize/export already has `hasSkeleton` / `animationPlayer` / `animationTree` / camera — wire Inspector create/remove through that path
- Tests: command undo/redo (cascade as one step), hydration from skinned glTF vs static mesh, Unique attachment disable, Object-not-created-for-Camera
- Docs: CONTEXT glossary, ADR 0033, ADR 0034
