## 1. Object property bag + reorder + export

- [x] 1.1 Store property bag on `BehaviourSlot`; copy on instantiate; write on `exportToScene`
- [x] 1.2 Add `Object::moveBehaviour` (or equivalent) for list reorder
- [x] 1.3 Native tests: bag round-trip + reorder

## 2. Behaviour type catalog

- [x] 2.1 Add `Blunder.ScriptsCatalog` (`net10.0`) writing `.blunder/behaviour_catalog.json`
- [x] 2.2 Hook catalog refresh after successful `ScriptsBuilder` build
- [x] 2.3 Native catalog reader + unit test (fixture JSON)

## 3. SceneInstance ensure Object + editor Commands

- [x] 3.1 `SceneInstance::ensureBoundObject(EntityId)` / find bound Object
- [x] 3.2 Commands: Add / Remove / Reorder / SetProperty (+ factories in `editor_commands`)
- [x] 3.3 `editor_commands_test` (or new `inspector_behaviour_commands_test`) undo/redo

## 4. Inspector UX

- [x] 4.1 Slint Behaviour section: list, Add picker, Remove, drag reorder, property rows, missing-type state
- [x] 4.2 `slint_system` sync/apply wired to Commands + catalog
- [x] 4.3 Manual smoke: Add â†?edit â†?save â†?reload â†?Play mount still works

## 5. Docs / gate

- [x] 5.1 Confirm CONTEXT + ADR 0016 names match shipped UI strings
- [x] 5.2 Run focused native/managed tests green
