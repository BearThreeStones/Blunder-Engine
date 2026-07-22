## Why

The Project Manager MVP works but its Slint shell is a flat toolbar + list, unlike Godot's readable Project Manager layout. Authors need Godot-shaped chrome (tab header, create/import strip, list with last-opened, Open/Remove side actions) without expanding MVP scope into Scan, favorites, Asset Library, or multi-version Hub features.

## What Changes

- Restyle `ProjectManagerWindow` to a Godot-like partition: top Projects chrome, left Create/Import, center Project List, right Open/Remove, bottom status
- Show **last opened** time on each list row (data already on Project List entries); keep name, path, and missing styling
- Visually align Create/Import modal dialogs with the new shell (same palette and spacing); no Create/Import rule changes
- English UI labels; primary open affordance remains **Open** (domain term: Project Open)
- Omit non-MVP Godot affordances (Scan, favorites, filter/sort, Asset Library, Settings, Run, Rename, Duplicate, Tags, Remove Missing, Donate) rather than showing them disabled

## Capabilities

### New Capabilities
- `project-manager-chrome`: Godot-shaped Project Manager layout and list presentation for MVP actions only

### Modified Capabilities
- _(none — `project-manager` capability lives in the still-open `project-manager` change and is not archived under `openspec/specs/` yet)_

## Impact

- UI: `engine/src/runtime/function/slint/project_manager.slint`
- Binding: `SlintSystem` Project Manager helpers + `ProjectManagerApp` list refresh (pass `last_opened` into `ProjectRow`)
- Docs: `CONTEXT.md` glossary note for Manager chrome scope; no ADR required (easy to reverse UI layout)
- Out of scope: new Project List features, Scan, i18n, Asset Library
