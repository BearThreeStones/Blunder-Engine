# Object Active is GameObject active, not Scene Visibility

Hierarchy and Inspector toggle **Object Active** (Unity `activeSelf`), persisted on the Scene Asset and honored in Play. Descendants keep their own flags; **Active in Hierarchy** is derived. Rejected: Unity Hierarchy eye / Scene Visibility (editor-only, not in the scene file) because authors asked for Play-visible on/off, and `Entity.enabled` already means Delete tombstone.

## Considered Options

- **Scene Visibility (Hierarchy eye)** — rejected; would not hide meshes in Play and would invent a second hide that is not the scene document.
- **Reuse `Entity.enabled`** — rejected; soft-delete already disables that flag. Object Active is a separate authored field.
- **Rewrite descendants' Object Active when the parent toggles** — rejected; restoring the parent would not restore locally inactive children.
