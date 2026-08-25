## Context

See proposal.md for motivation. Content Browser lists Assets-root directories and descriptors (`ContentIndex::scan` Assets only). `reparentEntry` moves files but rejects directories and does not remap the Asset Registry. There is no mkdir/rename/folder-delete product path. `AssetImportService::deleteAsset` exists (in-flight `content-browser-asset-delete`) without History. Editor History is one `DocumentHistory` stack; `IEditorCommand` has undo/redo and selection snapshots but no label. History Panel UI has Scene/Global filters; Global has no backing stack. Vocabulary and ADRs: CONTEXT.md; [ADR 0037](../../../docs/adr/0037-content-browser-global-history.md); [ADR 0038](../../../docs/adr/0038-delete-scene-detach-global-command.md).

## Goals / Non-Goals

**Goals:**
- Product APIs on `ContentBrowserSystem` + Global Command wrappers
- Registry path remap on move/rename; reuse `deleteAsset` for each Asset in a Delete Folder set
- Focus-routed Undo; History Panel Global group lists real commands
- Slint: New Folder, Inline Rename, menus, confirm, folder drag

**Non-Goals:**
- Resources/Source Browser trees; moving Intermediate/Source on rename
- Merged Ctrl+Z timeline; multi-select drag-reparent
- A second command type registry / Seam

## Decisions

### D1 — Second `DocumentHistory` instance as Global History
**Choice:** Reuse the linear stack class (`DocumentHistory` or a renamed `EditorHistoryStack`) for Global History. Session-scoped; `openScene` does not clear it. `IEditorCommand::label()` (English) for History Panel. Global Commands use empty selection snapshots.
**Why:** Same cursor/truncate/depth behavior; fills ADR 0008’s empty stack without a merged timeline.
**Rejected:** Stuffing Browser ops into the scene stack; a one-off Browser-only undo buffer.

### D2 — Mutation then push
**Choice:** Apply filesystem + registry (and scene detach) first; on success push an already-applied Global Command that can invert it. Failure does not push.
**Why:** Matches existing Document History (`push` after apply).
**Rejected:** Command execute() that also does the first apply (two code paths).

### D3 — Delete Folder = validate set, then `deleteAsset` per GUID
**Choice:** Build the delete set (folder tree descriptors). Refuse if any GUID has a non-Scene dependent whose descriptor is **outside** the set. Confirm when the set contains ≥1 Asset. Then delete Assets (skip the outside-dependent check for in-set dependents), then remove leftover empty directories. One Global Command snapshots enough to restore descriptors, registry, removed Intermediate/Final side effects, and scene detach before/after (disk + live instance if open).
**Why:** Reuses in-flight delete; matches grilled all-or-nothing rule.
**Rejected:** `rm -rf` the directory; calling `deleteAsset` independently per multi-select item (partial success).

### D4 — Delete undo snapshots files, not a trash folder
**Choice:** The Command holds copies of removed descriptor/Intermediate bytes and scene-file before blobs (plus GUID/path maps). Undo writes them back and re-registers.
**Why:** No extra `.blunder/trash` product; v1 folder sizes are small.
**Rejected:** OS Recycle Bin; tombstone-only undo that cannot restore bytes.

### D5 — `FileSystem::createDirectory` + existing `movePath`
**Choice:** Add an explicit create-directory API next to `ensureParentDirectory`. Rename/reparent use `movePath`. Suppress watch + `refresh()` as `reparentEntry` already does.
**Why:** All other engine IO goes through FileSystem.
**Rejected:** Raw `std::filesystem` in Content Browser.

### D6 — Registry remap via `registerAsset` / scan
**Choice:** After a successful move, update GUID→path for every moved descriptor (`registerAsset` same GUID, new path) and `save()`. Undo restores old paths the same way. Do not allocate new GUIDs.
**Why:** Registry is the path map; identity stays GUID.
**Rejected:** `rebuildFromScan` as the only remap (racy with watch, weaker undo).

### D7 — Inline Rename in Slint; commit calls engine
**Choice:** Overlay/edit field on the selected row. Commit sends stem or folder name; engine validates Browser entry name and writes. Esc cancel does not call rename. While the field is focused, Ctrl+Z is text-undo, not Global History.
**Why:** Matches grilled gesture; avoids a modal.
**Rejected:** OS rename dialog; editing the full `*.mesh.yaml` string.

### D8 — Folder context path on the New callbacks
**Choice:** New Folder / New Scene callbacks take the parent virtual path (open folder vs right-clicked folder). Toolbar/empty pass `selectedFolder()`. Folder-row menus pass that row’s path without `setSelectedFolder`.
**Why:** Spec requires create-inside-clicked-folder without navigating.
**Rejected:** Always `selectedFolder()` (current New Scene).

### D9 — Focus routing from Content Browser panel focus
**Choice:** If the Content Browser (docked or floating) is the focused Slint region and Inline Rename is inactive, Undo/Redo + Edit menu target Global History; else Document History. History Panel click still History-Jumps the row’s own stack.
**Why:** ADR 0037.
**Rejected:** Last-pushed stack; always Document.

### D10 — Wrap existing Asset Delete UI in the same Global Command type
**Choice:** Grid Delete of descriptors uses the same delete-set Command (set size 1). Scene detach + live dirty stay inside that Command (ADR 0038).
**Why:** One undo model; folder-ops must not leave Asset Delete irreversible.
**Rejected:** Asset Delete stays non-undoable beside undoable Folder Delete.

## Risks / Trade-offs

- [Delete undo memory for large trees] → Mitigation: snapshot only files the command actually removed; refuse/log if restore write fails
- [Live scene dirty without Document Command] → Accepted (ADR 0038); Global undo restores refs
- [Watch refresh races mkdir/rename] → Mitigation: existing suppress window on `refresh()`
- [history-panel Global filter still Scene-only when both checked] → Mitigation: this change lists both stacks as separate groups
- [deleteAsset header vs tests on dependents] → Mitigation: implement the grilled/spec rule (scene detach, non-scene outside-set refuse), not the stale “refuse any dependent” comment
- [Concurrent `content-browser-asset-delete`] → Mitigation: land folder-ops delete-set on top of `deleteAsset`; replace “skip folders” / independent multi-select

## Migration Plan

1. FileSystem mkdir + ContentBrowser folder create/rename/reparent + registry remap + tests
2. Global History stack + command labels + focus-routed Undo + wrap Asset Delete
3. Delete Folder set + confirm + scene detach snapshots
4. Slint menus, Inline Rename, drag cursor, New Scene parent path
5. Manual smoke: New Folder → rename → reparent → undo in Browser vs viewport; Delete Folder with a scene mesh ref

Rollback: revert Browser UI and Global stack; descriptors/registry remain GUID-based.

## Open Questions

None — grilled into CONTEXT / ADR 0037 / ADR 0038.
