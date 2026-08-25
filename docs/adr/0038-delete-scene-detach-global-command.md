# Scene detach on Asset/Folder delete rides the Global Command

Asset Delete and Delete Folder may clear Scene Asset references to removed GUIDs. That detach is payload of the **same Global Command** as the filesystem removal — including on-disk Scene Assets and the live SceneInstance when the open scene is affected (the open document becomes dirty). It is not a Document History command. Undo/redo of that Global Command restores or re-applies both files and references. Viewport-focused Ctrl+Z does not undo a Browser delete (ADR 0037 focus routing).

Rejected: splitting detach onto Document History (two undos, split truth); refusing delete merely because the open scene references an Asset in the set.
