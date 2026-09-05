## Why

Bones are not stored in the scene file. `chocomel-locomotion-play` accepted same-session Play after Inspector Add… Skeleton and treated a T-pose after reopen as out of scope. Authors now reopen `chocomel_locomotion` (and any other skinned Player/Tree scene) expecting Live and Play to already have named bones. That follow-up is this change.

## What Changes

- Scene **document instantiate** (editor open, Player load, Reload from disk, Scene Thumbnail still) fills empty Skeletons from the entity mesh Intermediate glTF **after** mesh attach. Same gate as ADR 0034: only Objects that already have AnimationPlayer or AnimationTree.
- `SceneSystem::loadScene` / `reloadActiveFromDisk` remain the product path; tests assert named bones **through** `loadScene`, not by calling hydrate as a private step.
- Scene Thumbnail instantiate uses the same attach+hydrate sequence so stills of a posed tree scene are not a rest T-pose solely because bones were empty.
- Inspector Add… hydration is unchanged. GEO children and cube Skeletons with no Player and no Tree stay empty. Do **not** serialize bones into the scene file.

## User stories

1. I quit the editor, reopen the Test Project, open `chocomel_locomotion`, Play — the Player window walks (idle at rest, stick walk), not a T-pose.
2. In a new editor session I open that scene and the Inspector already lists named bones. I do not Add… Skeleton again.
3. I open a cube (or other) entity that has an empty Skeleton and no AnimationPlayer and no AnimationTree. After load the Skeleton is still empty.
4. A GEO child still has an empty Skeleton after load, and skinning still uses the parent’s hydrated bones.

## Capabilities

### New Capabilities

- `skeleton-hydration-on-instantiate`: Scene document instantiate hydrates empty Skeletons from the entity mesh after attach, gated on AnimationPlayer or AnimationTree.

### Modified Capabilities

- (none — Add… Skeleton hydration in `inspector-add-menu` stays as archived. This change only adds instantiate-time hydrate.)

## Impact

- **Engine:** `SceneSystem::instantiateScene`, Scene Thumbnail instantiate, `hydrateEmptySkeletonsFromEntityMeshes` (call sites). GUID mesh refs still resolve through `AssetRegistry` on global context.
- **Tests:** `skeleton_hydration_test` covers `SceneSystem::loadScene` (path mesh and GUID `.mesh.yaml`) plus GEO child / skeleton-only empty.
- **Docs:** ADR 0034 already names scene load; keep that contract. `chocomel-locomotion-play` stays a content slice; this change owns reopen Play.
- **Non-goals:** Tree Asset, AnimationTree Canvas, serializing bones, hydrating skeleton-only entities, Player skip / IEntityStore / `hasInputFocus` (those stay as they are).
