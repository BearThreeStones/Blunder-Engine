# Manual checklist — bindless-texture-table

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` on the Test Project (`E:\Blunder Projects\Test`). Story 2: open Mesh Preview and Camera Preview in the same session. Story 3 is capacity overflow (Headless / a test that fills the table is enough if the windowed scene cannot). Story 4: a scene or spawn that exceeds 256 mesh draws in one list still truncates as today.

| # | User story | Pass |
|---|------------|------|
| 1 | In the editor viewport, several meshes with several material textures look the same as before bindless; shadows stay the current PCF and are not in that table. | |
| 2 | In the same session, Mesh Preview and Camera Preview mesh textures stay correct (they share this device’s one table). | |
| 3 | When unique material textures exceed table capacity, extras use the fallback texture; the editor does not crash. | |
| 4 | A frame still records at most 256 mesh draws; overflow is truncated the same as today, not because bindless grew. | |
