## Why

Authors cannot remove a wrongly imported Asset (e.g. incomplete Chocomel Mesh) from Content Browser without manually deleting files on disk and risking a stale registry. Delete is required for everyday Import iteration and for recovering from bad companion / Mesh Imports.

## What Changes

- Add Content Browser selection + Delete (keyboard and/or context menu) for Asset descriptors.
- Add a coordinated delete service: remove descriptor, unregister GUID, best-effort Intermediate + Final cleanup, refresh browser + dependency graph.
- Refuse delete when the dependency graph reports dependents (v1: no cascade, no silent dangling refs).
- Status / log feedback when delete is refused or partially fails.

## Capabilities

### New Capabilities
- `content-browser-asset-delete`: Content Browser UI + engine delete path for registered Assets

### Modified Capabilities
- `asset-identity`: unregister GUID as part of product delete (registry remains source of truth)
- `asset-pull-cook`: deleting Mesh/Texture marks or removes Final so Fast Path / Cook do not keep stale cooked bytes as authoritative

## Impact

- Slint Content Browser + `SlintSystem` / `UiHost` event routing
- `AssetRegistry::unregisterGuid` (currently unused in production)
- New delete helper (or `AssetImportService` / Content Browser service method)
- `AssetDependencyGraph`, `AssetCompilerService::markFinalStale`
- Tests: registry, dependency refuse, Intermediate/Final cleanup, browser refresh
