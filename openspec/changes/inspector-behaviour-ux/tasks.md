## 1. Object property bag + reorder + export

- [ ] 1.1 Store property bag on `BehaviourSlot`; copy on instantiate; write on `exportToScene`
- [ ] 1.2 Add `Object::moveBehaviour` (or equivalent) for list reorder
- [ ] 1.3 Native tests: bag round-trip + reorder

## 2. Behaviour type catalog

- [ ] 2.1 Add `Blunder.ScriptsCatalog` (`net10.0`) writing `.blunder/behaviour_catalog.json`
- [ ] 2.2 Hook catalog refresh after successful `ScriptsBuilder` build
- [ ] 2.3 Native catalog reader + unit test (fixture JSON)

## 3. SceneInstance ensure Object + editor Commands

- [ ] 3.1 `SceneInstance::ensureBoundObject(EntityId)` / find bound Object
- [ ] 3.2 Commands: Add / Remove / Reorder / SetProperty (+ factories in `editor_commands`)
- [ ] 3.3 `editor_commands_test` (or new `inspector_behaviour_commands_test`) undo/redo

## 4. Inspector UX

- [ ] 4.1 Slint Behaviour section: list, Add picker, Remove, drag reorder, property rows, missing-type state
- [ ] 4.2 `slint_system` sync/apply wired to Commands + catalog
- [ ] 4.3 Manual smoke: Add → edit → save → reload → Play mount still works

## 5. Docs / gate

- [ ] 5.1 Confirm CONTEXT + ADR 0016 names match shipped UI strings
- [ ] 5.2 Run focused native/managed tests green
