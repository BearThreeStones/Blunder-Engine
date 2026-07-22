## 1. List data binding

- [x] 1.1 Extend `ProjectRow` / `setProjectManagerRows` to carry a last-opened display string
- [x] 1.2 Format `last_opened_unix` in `ProjectManagerApp::refreshListUi` (placeholder when unset)

## 2. Godot-shaped chrome

- [x] 2.1 Restyle `project_manager.slint`: top Projects header, Create/Import strip, center list, Open/Remove side column, status footer
- [x] 2.2 List rows: name, path, missing styling, last-opened; English labels; omit non-MVP Godot controls
- [x] 2.3 Align Create/Import modal visuals with the new shell (no behavior changes)

## 3. Validation

- [x] 3.1 Build `project_manager` and smoke: list, select, Open, Remove, Create/Import dialogs, missing row
- [x] 3.2 Confirm `CONTEXT.md` **Project Manager chrome (v1)** still matches shipped UI
