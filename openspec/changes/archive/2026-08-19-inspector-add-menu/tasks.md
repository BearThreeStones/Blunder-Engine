## 1. Skeleton hydration helper

- [x] 1.1 Extract shared populate-skeleton-from-cgltf-skin helper used by GltfSceneImporter (no behavior change)
- [x] 1.2 Hydrate selected Object Skeleton from entity mesh Intermediate glTF (skinned → bones; static/fail → empty + warn)
- [x] 1.3 Tests: skinned hydration fills named bones; static mesh stays empty; Add does not spawn glTF child entities

## 2. Editor Commands

- [x] 2.1 Camera Add/Remove Document Commands on EntityId (today Add Camera is not on History)
- [x] 2.2 Composite Add Unique-attachment Command: records newly created Object/Skeleton/Player/Tree + hydration; one undo restores all
- [x] 2.3 Remove Unique-attachment Commands: Player/Tree do not clear Skeleton; Remove Skeleton no-ops while Player, Tree, or any Modifier remains; keep Object
- [x] 2.4 Add clip / clip-row Remove via clip-bindings Command (empty name+GUID row allowed as draft)
- [x] 2.5 Add… Behaviour and SkeletonModifier still use existing Commands (no extra cascade)
- [x] 2.6 Tests: undo/redo Add Player cascade; undo Add Camera; undo Add clip; Remove Skeleton blocked while Player present

## 3. Inspector Add… UI

- [x] 3.1 Slint Add… grouped popup (Unique attachments, Behaviours, Skeleton Modifiers); empty catalog keeps Behaviours group + build-Scripts hint
- [x] 3.2 Remove standalone Add Camera / Add Behaviour / Add Skeleton Modifier buttons; wire those types through Add…
- [x] 3.3 Unique rows visible and disabled when present; Add… disabled unless exactly one entity selected
- [x] 3.4 Unique section Remove on Camera / Skeleton / Player / Tree headers; Skeleton Remove disabled when occupied
- [x] 3.5 Animation Player section Add clip control; empty map can gain a row
- [x] 3.6 `slint_system` dispatch: ensureBoundObject for ClassDB hosts; Camera does not create Object; host cascade Tree → Player → Skeleton; new Tree empty and inactive

## 4. Docs / validation

- [x] 4.1 Confirm CONTEXT Add… terms and ADR 0033 / 0034 match shipped UI strings (prefer no churn)
- [x] 4.2 Build `engine_editor`; focused tests from 1.3 and 2.6
- [ ] 4.3 Manual: spawn skinned mesh → Add… AnimationPlayer → Add clip → Edit preview Play deforms → Ctrl+Z removes Player+Skeleton together; Add Camera on mesh-only entity leaves no Object
