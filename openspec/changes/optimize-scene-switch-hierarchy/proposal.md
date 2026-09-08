## Why

Opening or switching to a scene rebuilds the Hierarchy synchronously on the UI thread. The current implementation discovers children by scanning every entity for every visible row, so large imported scenes can freeze the editor for seconds.

## What Changes

- Build the Hierarchy parent-to-children lookup in one pass and preserve the existing row order, expansion, tombstone, line-guide, and icon behavior.
- Resolve per-row bound Object attachments through an entity index instead of scanning every bound Object.
- Add regression coverage for large flat and nested scene trees and for scene-switch refresh behavior.
- Record before/after scene-switch benchmark evidence using the editor validation path.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `hierarchy-panel`: Require scene activation to rebuild visible Hierarchy rows with work that scales linearly with scene size.

## Impact

Affected code is limited to the editor Hierarchy model, SceneInstance bound-Object lookup, and focused first-party tests. Scene serialization, EntityId stability, render submission, asset loading, Slint fork code, and public engine APIs remain unchanged.
