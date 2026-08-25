## Context

See proposal.md for motivation. Grilling locked CONTEXT **Add… kind icon**. Inspector chrome already uses `editor_icons.slint` (Godot SVGs + `colorize`). Add… picker and Unique/Behaviour/Modifier sections live in `inspector_panel.slint`, shared by docked and floating Inspector. No new snapshot fields: kinds are compile-time; missing is `behaviour.missing`; Unique-present is `has-camera` / `has-skeleton` / `has-animation-player` / `has-animation-tree`.

## Goals / Non-Goals

**Goals:**
- Six `EditorIcon*` wrappers over existing `godot-icons` SVGs
- Place them in Unique headers, Behaviour/Modifier rows, and Add… kind rows
- `colorize` from the same color as the adjacent text

**Non-Goals:**
- Catalog-driven icon paths
- New C++ models or Commands
- Hierarchy / clip / Transform icons
- ADR (reversible chrome)

## Decisions

### D1 — Godot 3D SVGs, one wrapper each
**Choice:** `Camera3D.svg`, `Skeleton3D.svg`, `AnimationPlayer.svg`, `AnimationTree.svg`, `Script.svg`, `SkeletonModifier3D.svg`. Same wrapper pattern as `EditorIconMesh`. Size 14px to match Hierarchy `EditorIconScene`; Unique expand arrows stay 12px.
**Why:** Engine is 3D; Script is the Behaviour stand-in; no per-type art in the Behaviour catalog.
**Rejected:** 2D Camera/Skeleton; `Animation.svg` for Player; `PluginScript`; drawing custom icons.

### D2 — Colorize, do not swap
**Choice:** `icon-color` binds to the label color already used on that row (`#d0d0d0` normal, `#808080` Unique disabled in Add…, `#ff9a9a` missing). Keep `Script.svg` for missing.
**Why:** Matches the grilled visual rule; one asset per kind.
**Rejected:** `ScriptRemove.svg` for missing; hiding Unique icons when present.

### D3 — Slint layout only
**Choice:** Insert icon components next to existing Text. Add… Unique rows currently absolutely-position Text at `x: 8px`; switch those rows to a HorizontalBox (icon + text) so padding stays even. Behaviour/Modifier Add… `for choice` rows the same.
**Why:** Absolute `x: 8px` would overlap a 14px icon.
**Rejected:** Passing an icon index through `BehaviourRow` (unnecessary when kind is fixed per loop).

### D4 — Docs already done
**Choice:** CONTEXT term landed during grilling. No ADR.
**Why:** Domain modeling writes the glossary when the term crystallises.

## Risks / Trade-offs

- [Add… row overlap] → Replace absolute Text with HorizontalBox on picker kind rows
- [Six near-duplicate wrappers] → Same as existing editor_icons.slint; keep copies, no generic `source` property (Slint `@image-url` is static)
- [Colorize washes original SVG hues] → Accept; Browser icons already colorize

## Migration Plan

1. Add six wrappers in `editor_icons.slint`
2. Unique headers + Add… Unique rows
3. Behaviour / Modifier rows + Add… catalog rows
4. Build `engine_editor`; visual smoke

Rollback: revert Slint; no scene/format change.

## Open Questions

None.
