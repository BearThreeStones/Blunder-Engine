# Manual checklist — object-active-editor-camera-fly

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the Test Project (`E:\Blunder Projects\Test`), scene `chocomel_locomotion` unless a row says otherwise.

| # | User story | Pass |
|---|------------|------|
| 1 | Pointer over Hierarchy, select an Object, press A (or click the row checkbox): only that Object's Object Active flips. The name greys or restores from Active in Hierarchy; the mesh leaves or returns in the viewport; children's checkboxes stay. Inline Rename types the letter A. | |
| 2 | Multi-select, press A or click a checkbox: if every selected Object is Active they all turn off; otherwise they all turn on (mixed becomes all on). Inspector identity checkbox is the same field; mixed selection is indeterminate. | |
| 3 | Uncheck a parent: descendants keep their own checkboxes but names grey, viewport pick misses them, no gizmo/outline, their lights do not illuminate, Behaviours do not Tick. Grey Hierarchy rows remain selectable. | |
| 4 | Uncheck an Object, Save, reopen: it stays inactive. Play does not draw it or Tick it. If Main Camera is not Active in Hierarchy, Play fails with no available Camera — no Editor Camera fallback. | |
| 5 | While Play is running, change Object Active in the editor: the Player window updates without Reload. Undo/Redo patches both sides. | |
| 6 | WASD / Q / E (Shift sprint) move the Editor Camera only while middle or right mouse was pressed in the viewport and is still held; middle-mouse pan still runs. Those keys do not fly the camera from Hierarchy or Inspector, or from viewport hover without that button. | |
