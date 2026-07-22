## Why

Scenes can already persist and mount Behaviour declarations, but authors still cannot Add/Remove/edit Behaviours in the Inspector. DogWalk and every Project script need Edit Mode authoring without starting a product DotNetHost (ADR 0014 / 0016).

## What Changes

- Add **Inspector Behaviour UX**: list, Add from type catalog, Remove, drag reorder, bool/number/string property form
- Author Edit Mode against **Behaviour declarations** on the bound Object (property bag stored on slots); mount remains Play/Player
- Build a **Behaviour type catalog** from Scripts assembly metadata after successful `dotnet build` (no CoreCLR host for authoring)
- Wire Add / Remove / reorder / property commit as **Document History** Commands on `EntityId`
- Round-trip property bags through export/save (today Object slots drop bags on export)
- Missing catalog types show as broken entries (keep declaration; allow Remove)
- **Out of scope:** Vec3/enum/nested/asset editors, live-peer-required authoring, ALC hot reload, hand-typed CLR names as primary Add path

## Capabilities

### New Capabilities
- `inspector-behaviour`: Inspector Behaviour section, type catalog consumption, declaration edits, missing-type UI, History Commands for Add/Remove/reorder/property commit

### Modified Capabilities
- `behaviour-scene`: Live Object Behaviour slots SHALL carry the property bag so export/save preserves Inspector edits; ensure Object binding when first Behaviour is added in Edit Mode
- `scene-edit-commands`: Add Behaviour Add/Remove/reorder/property-commit Commands on `EntityId` (same History stack and sealing rules as Transform)

## Impact

- Native: Object BehaviourSlot property bag + reorder; SceneInstance ensure-bound-Object; editor Commands; catalog JSON reader; Slint Inspector section + `slint_system` sync/apply
- Managed (optional tool): small catalog scanner writing `.blunder/behaviour_catalog.json` after Scripts build
- Tests: catalog parse, bag round-trip, Commands undo/redo, Inspector ops helpers
- Docs: ADR 0016, CONTEXT Glossary already grilled
- Plan: `docs/superpowers/plans/2026-07-22-inspector-behaviour-ux.md`
