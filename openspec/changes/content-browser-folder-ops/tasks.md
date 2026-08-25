## 1. Filesystem and names

- [x] 1.1 Add `FileSystem::createDirectory`; tests for success and existing-path failure
- [x] 1.2 Shared Browser entry name validator (trim, illegal set, reserved Windows names, sibling collision)
- [x] 1.3 Tests: illegal names rejected; collision rejected; `New Folder` / `_N` unique helper

## 2. Content Browser mutations (TDD)

- [x] 2.1 `createFolder` under a parent virtual path; refresh; select new folder for Inline Rename
- [x] 2.2 Rename Folder / Asset rename (stem + preserve typed suffix); registry `registerAsset` same GUID new path + save
- [x] 2.3 Folder `reparentEntry` (allow directory sources); refuse self/descendant/collision/root; remap nested GUIDs
- [x] 2.4 Tests: create + rename + reparent; Intermediate `source` files unmoved; `findGuidForPath` old path empty

## 3. Global History

- [x] 3.1 Extract or reuse stack as Global History (second instance); `openScene` does not clear it
- [x] 3.2 `IEditorCommand::label()` (or equivalent); push Global Commands after successful mutations
- [x] 3.3 New Folder = one command; Inline Rename commit that changes name = second command
- [x] 3.4 Focus-routed Undo/Redo + Edit menu; Inline Rename text field claims Ctrl+Z
- [x] 3.5 History Panel: both filters list Scene group + Global group (no merge); Global rows History-Jump Global stack
- [x] 3.6 Tests: undo create removes dir; open scene keeps Global undo; viewport Ctrl+Z does not undo folder create

## 4. Delete Folder and Asset Delete wrap

- [x] 4.1 Delete-set builder (folder tree ∪ multi-select); refuse when a GUID has a non-Scene dependent outside the set
- [x] 4.2 Confirm when set contains ≥1 Asset; empty folder deletes with no confirm; Assets root not deletable
- [x] 4.3 Apply set via `deleteAsset` (in-set dependents allowed); then remove leftover empty dirs; one Global Command with file/registry/scene-detach snapshots
- [x] 4.4 Wrap existing Asset Delete UI in the same Command type; live SceneInstance detach + dirty when open scene affected
- [x] 4.5 Open Scene follow: path-only ops retarget `activeScenePath`; deleting the open Scene Asset uses dirty prompt then closes
- [x] 4.6 Tests: empty folder; external texture refuse; scene detach + Global undo restores refs; multi-select union all-or-nothing

## 5. Slint / UiHost

- [x] 5.1 Toolbar **+ New Folder**; grid/tree context: New Folder, New Scene, Rename, Delete; New callbacks pass Browser folder context path
- [x] 5.2 Align New Scene with folder context (create inside right-clicked folder, no navigate)
- [x] 5.3 Inline Rename overlay (F2 / menu / click selected name); stem vs folder buffer; commit/cancel; no start on multi-select
- [x] 5.4 Drag cursor not-allowed for illegal folder reparent; folder source over viewport not-allowed
- [x] 5.5 Delete confirm dialog copy (folder name + Asset count)

## 6. Validation

- [x] 6.1 Confirm CONTEXT folder-ops terms and ADR 0037/0038 match shipped behavior (no extra glossary churn)
- [x] 6.2 Build `engine_editor`; run focused content-browser / asset-import delete / editor-history tests from 1.3, 2.4, 3.6, 4.6
- [ ] 6.3 Manual: New Folder → rename Chars → drag into another folder → Ctrl+Z in Browser vs viewport; Delete Folder of a mesh used by the open scene → Global undo restores the ref
