## Why

Authors cannot create or rename Browser Folders, rename Assets, or drag folders in Content Browser. They also cannot undo filesystem authorship: Asset Delete is in flight without History, Global History is still an empty placeholder, and Ctrl+Z always hits Document History. Everyday layout of `Assets/` still requires Explorer.

## What Changes

- Add **New Folder**, **Inline Rename** (Rename Folder + Asset rename), **Delete Folder**, and **Browser reparent** of Browser Folders (as well as Assets) on the Assets-root tree only
- Record those mutations (and Asset Delete) as **Global Commands**; **Focus-routed Undo** sends Ctrl+Z/Y to Global History when Content Browser has focus
- **Delete scene detach** rides the same Global Command (ADR 0038); Open Scene follow keeps the document open across path-only changes
- Align New Scene with **Browser folder context** (right-click a folder creates inside it)
- **Out of scope:** Resources/Source as Browser trees; pairing Intermediate/Source file renames; display-name fields; merged Scene/Global Ctrl+Z timeline; multi-select drag-reparent; folder drop onto the viewport

**BREAKING (authoring UX):** New Scene from a folder row’s context menu creates *inside* that folder (previously always the open grid folder). Multi-select Delete no longer skips folders. Asset Delete becomes undoable on Global History.

## Capabilities

### New Capabilities
- `content-browser-folder-ops`: Browser Folder create/rename/delete/reparent, Asset rename, Inline Rename, folder context, Global Commands for those mutations (including Asset Delete + scene detach)

### Modified Capabilities
- `document-history`: Keyboard Undo/Redo and Edit menu route by panel focus (Content Browser → Global History; otherwise Document History)
- `asset-identity`: Registry descriptor paths rewrite on rename/reparent; GUID unchanged; Intermediate/Source bodies not moved by Browser rename/reparent

## Impact

- `ContentBrowserSystem` (`createFolder`, `renameEntry`, folder `reparentEntry`, delete-set) + `FileSystem` mkdir/move
- `AssetRegistry` path remap for moved descriptors; `AssetDeleteService` (or equivalent) reused by Delete Folder
- Global History: first real Commands; History Panel Global filter lists them in groups (no timeline merge)
- Slint Content Browser: toolbar, grid/tree context menus, Inline Rename, Delete confirm, drag cursor for illegal folder targets
- `SlintSystem` / `UiHost`: focus-routed Undo, New Scene parent from menu target
- Aligns with in-flight `content-browser-asset-delete` (replace “folders skipped”; one all-or-nothing delete set)
- Docs: CONTEXT (grilled), [ADR 0037](../../../docs/adr/0037-content-browser-global-history.md), [ADR 0038](../../../docs/adr/0038-delete-scene-detach-global-command.md)
- Tests: name legality, collision refuse, folder delete dependents, reparent, registry remap, Global undo, focus routing, Open Scene follow
