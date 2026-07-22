## Context

`project_manager.exe` already hosts MVP Create / Import / Open / Remove via `ProjectManagerApp` + `project_manager.slint`. The shell is a single toolbar over a flat list. Grilling locked Godot-shaped chrome with MVP-only actions (structural placeholders, not greyed non-MVP controls). Project List entries already store `last_opened_unix`; the UI does not show it yet.

## Goals / Non-Goals

**Goals:**
- Restructure the Project Manager window into Godot-like regions: top Projects header, Create/Import strip, center list, Open/Remove side column, status footer
- Present name, path, missing state, and last-opened time per row
- Keep editor dark palette; borrow Godot spacing/hierarchy
- Polish Create/Import modals to match the new shell without changing validation or open-after-success behavior
- English labels; primary Project Open control labeled **Open**

**Non-Goals:**
- Scan, favorites, filter/sort, Asset Library, Settings, Run, Rename, Duplicate, Tags, Remove Missing, Donate
- Showing disabled stubs for those affordances
- i18n / Chinese UI
- Changing Project File, Project List store schema, or relaunch semantics
- Real project icons / thumbnails

## Decisions

1. **Slint-only chrome pass** — Rebuild layout in `project_manager.slint`. Extend `ProjectRow` with a display string for last opened (formatted in C++ when refreshing the list). Prefer minimal C++ churn in `ProjectManagerApp::refreshListUi` / `SlintSystem::setProjectManagerRows`.

2. **Omit non-MVP controls** — Do not draw Scan, star, version badge, Asset Library tab, Settings, or extra side actions. Top chrome shows brand text + active **Projects** affordance only.

3. **Last-opened formatting** — Format `last_opened_unix` to a local `YYYY-MM-DD HH:MM:SS`-style string in C++; empty/`0` shows an em dash or blank so rows stay aligned.

4. **Dialogs stay modal overlays** — Same Create/Import properties and callbacks; restyle padding, titles, and button alignment to match the shell.

5. **No new ADR** — Layout is easy to reverse; product scope is captured in `CONTEXT.md` (**Project Manager chrome (v1)**) and this change's specs.

## Risks / Trade-offs

- **[Risk] Over-fitting Godot visuals while using editor colors → Mitigation:** Match regions and density, not Godot hex values.
- **[Risk] Last-opened timezone/locale surprises → Mitigation:** Use a simple local timestamp string; refine later if needed.
- **[Risk] Wider side column wastes space on small windows → Mitigation:** Keep Open/Remove column narrow; window default size ~1100×700.

## Migration Plan

1. Update Slint layout + `ProjectRow` fields
2. Thread last-opened into list refresh bindings
3. Manual smoke: list, select, Open, Remove, Create/Import dialogs, missing row styling
4. Rollback: revert Slint + binding commits; list store unchanged

## Open Questions

- None for v1 chrome; optional later: simple letter/icon placeholder per row.
