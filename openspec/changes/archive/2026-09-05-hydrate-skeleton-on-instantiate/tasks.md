## 1. Shared instantiate sequence

- [x] 1.1 Add `completeSceneDocumentInstantiate` (mesh attach, cameras, `hydrateEmptySkeletonsFromEntityMeshes`) and call it from `SceneSystem::instantiateScene` after behaviour mount.
- [x] 1.2 Scene Thumbnail instantiate uses that helper instead of a local attach-only path.

## 2. Tests

- [x] 2.1 `skeleton_hydration_test`: write a `.scene.asset` with skinned host + clips, GEO child (`hasSkeleton`, no Player/Tree), and static cube (`hasSkeleton`, no Player/Tree). `SceneSystem::loadScene` — host has named hips/spine; child empty and skins from parent; cube empty. Do not call hydrate as the test’s primary step.
- [x] 2.2 Same test binary: GUID mesh field + `AssetRegistry` on `g_runtime_global_context`; `loadScene` hydrates named bones. Reset the registry afterward.
- [x] 2.3 Build and run `skeleton_hydration_test` (`build/vs2026-debug`, Debug). Existing Add… cases stay green.

## 3. Docs

- [x] 3.1 One-line ADR 0034: Scene Thumbnail instantiate uses the same attach+hydrate sequence as `SceneSystem`.
