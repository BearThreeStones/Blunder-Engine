## Context

See proposal.md for why. ADR 0034 already says scene load hydrates empty Skeletons on Objects that have AnimationPlayer or AnimationTree after mesh attach. `SceneSystem::instantiateScene` already calls `hydrateEmptySkeletonsFromEntityMeshes`. Bones are still not in the scene file. `skeleton_hydration_test` covers Inspector Add… and a **manual** `SceneInstance::instantiate` + hydrate call; it does not go through `SceneSystem::loadScene`. Scene Thumbnail instantiate attaches meshes/cameras locally and never hydrates.

`chocomel-locomotion-play` design treated hydrate-on-instantiate as a follow-up. Player Play on Chocomel already hydrates in product (GUID mesh + registry). This change makes that the required path for every scene-document instantiate and locks it with tests.

World is Z-up, +X right, +Y forward. Do not touch Player idle-skip, `IEntityStore`, or `hasInputFocus`.

## Goals / Non-Goals

**Goals:**

- One attach+hydrate sequence for scene-document instantiate (`SceneSystem` and Scene Thumbnail).
- `skeleton_hydration_test` asserts named bones after `SceneSystem::loadScene` (virtual glTF path and GUID `.mesh.yaml`) without calling hydrate as the test’s primary step.
- GEO child / skeleton-only empty stay empty.

**Non-Goals:**

- Changing Add… hydration or expanding `importUnderEntity` children.
- Hydrating Objects that have Skeleton but neither Player nor Tree.
- Serializing bones, Tree Asset, Canvas, C-ABI.
- Thumbnail GPU fingerprint tests (sharing the helper is enough).

## Decisions

1. **Keep the Player/Tree gate**  
   GEO children often have `hasSkeleton: true` and no mesh. Hydrating every empty Skeleton would warn and return false on those children. `findSkeletonForEntity` already walks to a parent with bones.  
   *Alternatives:* hydrate all empty Skeletons (noise + false failures); serialize bones (rejected, ADR 0034 / 0031).

2. **Shared `completeSceneDocumentInstantiate` after `SceneInstance::instantiate`**  
   Mesh attach must run first so GUID entities can fall back to the renderer path. Cameras stay in the same helper so Thumbnail cannot skip hydrate by copying a local attach. Behaviours stay SceneSystem-only (`mountSceneBehaviours`).  
   *Alternatives:* hydrate inside `SceneInstance::instantiate` (no AssetManager); duplicate one hydrate call in Thumbnail (drifts).

3. **GUID resolve stays on global `AssetRegistry`**  
   `meshReferenceForEntity` already maps GUID → descriptor path. Tests that use a GUID mesh MUST install a registry on `g_runtime_global_context`. Product editor/player already do.  
   *Alternatives:* pass registry into hydrate (wider API); skip GUID tests (misses Chocomel).

4. **Do not hydrate on `loadGltfScene`**  
   glTF import builds the graph from the file; that is not a scene document with empty `hasSkeleton`.  
   *Alternatives:* also hydrate after import (redundant / wrong objects).

## Risks / Trade-offs

- [Active tree samples rest before clips resolve] → Hydrate still `sampleBoundSkeleton` after fill; missing clip assets stay rest, not T-pose-from-empty-bones.
- [Thumbnail still bind/rest, not Play sampling] → Hydrated rest/bind is enough so stills are not unskinned T-pose; AnimationPlayer sampling remains a thumbnail non-goal.
- [Existing `instantiate` + manual hydrate test] → Keep it as a unit of the helper; `loadScene` is the product contract.

## Migration Plan

1. Land helper + Thumbnail call site + `loadScene` tests.
2. Authors reopen skinned scenes; no scene JSON migration.
3. Rollback: revert the helper/Thumbnail/test; `SceneSystem` hydrate can remain (already shipped) but tests would not lock it.

## Open Questions

None.
