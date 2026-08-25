## Context

See proposal.md for motivation. Today AnimationPlayer Inspector rows are name + editable GUID (`inspector_panel.slint`); Add clip appends `{name:"", guid:""}` via `makeSetAnimationPlayerClipBindingsCommand` (ADR 0033 D5/D6). Content Browser drop classifies only mesh/scene (`ContentBrowserDropKind`). Behaviour Inspector (active `inspector-behaviour-ux`) edits bool/number/string bags only — no clip-name mark. Play resolves by logical name through the player map; Import must not auto-fill (ADR 0031 / 0036). Glossary terms: Clip Binding, Behaviour clip name.

## Goals / Non-Goals

**Goals:**
- Replace GUID paste with AnimationClip assign (picker + location-sensitive drop)
- Enforce unique logical aliases; hide GUID on regular Inspector
- Marked Behaviour string → dropdown over co-located player names
- Load-time discard of dual-empty draft rows; History for complete bind / retarget / remove

**Non-Goals:**
- Import auto-fill; Behaviour-owned AnimationClip references; Tree/Sync clip UI rewrite
- Advanced GUID disclosure; free-text fallback for marked clip-name fields

## Decisions

### D1 — Keep name→GUID storage; change authorship surface
**Choice:** Persist Clip Bindings as today (`animation_player_clips` / name→GUID). Inspector shows logical name + AnimationClip display identity; resolve GUID via Asset Registry when assigning.
**Why:** Runtime Play API and Tree/Sync already key by name; ADR 0036 keeps two layers.
**Rejected:** Behaviour fields storing GUIDs; changing Play to GUID-primary.

### D2 — Add clip = picker modal, not empty row
**Choice:** Add clip opens AnimationClip asset picker; confirm builds complete binding (stem default); cancel no-ops. Remove empty-draft commit path from D6 of inspector-add-menu.
**Why:** Unique-name map cannot host multiple `""` keys; empty drafts taught GUID paste.
**Rejected:** One empty draft allowed; auto-suffix on collision.

### D3 — Drop append vs retarget by hit target
**Choice:** Extend Content Browser drop to AnimationClip. Hit-test: list chrome / Add clip area → append; row widget → retarget that index (keep name). Reuse clip-bindings Command for both.
**Why:** Matches grilled location-sensitive rule; enables alias-stable retarget.
**Rejected:** Append-only drops; replace-only (must Add clip first).

### D4 — Uniqueness at commit, not last-write-wins
**Choice:** Before `setClipBindings`, reject append/rename when the logical name is already used by another row. UI keeps prior value on failure (toast/status optional).
**Why:** Spec forbids silent overwrite; current hash_map last-write-wins is wrong for product.
**Rejected:** Auto `_2` suffix; merge rows on collision.

### D5 — Behaviour clip-name mark in catalog metadata
**Choice:** Extend Behaviour type catalog (or member metadata) so marked members (C# attribute or agreed mark) render as logical-name dropdown. Bag value remains string. Invalid if name ∉ map (or empty player).
**Why:** Explicit mark from grill; avoids treating every string as a clip name.
**Rejected:** All strings as dropdowns; hard-code only `IdleClip`/`WalkClip` in engine.

### D6 — Weak refs, no cascade
**Choice:** Rename/remove binding does not rewrite Behaviour bags. Inspector paints invalid; Play no-ops empty / unresolved names per existing resolve failure.
**Why:** Avoid guessing which bag keys are clip names without the mark; Unity-style weak strings.
**Rejected:** Cascade rename; block rename while referenced.

### D7 — Load filter dual-empty only
**Choice:** On deserialize / apply bindings, drop entries with empty name and empty GUID. Keep half-filled for repair UI.
**Why:** Clears legacy Add clip drafts without deleting salvageable data.
**Rejected:** Save-time-only cleanup; permanent empty-row compatibility.

## Risks / Trade-offs

- [No AnimationClip picker UI yet] → Mitigation: reuse Content Browser selection dialog if one exists; otherwise minimal modal listing AnimationClip assets from registry
- [Drop hit-testing in Slint] → Mitigation: per-row DropArea + list DropArea; reject non-clip kinds with not-allowed cursor
- [Catalog mark requires Scripts rebuild] → Mitigation: document attribute + catalog field; unmarked fields stay free text until authors mark
- [inspector-behaviour-ux may still be landing] → Mitigation: land Player Clip Binding first if needed; Behaviour dropdown depends on Behaviour Inspector property rows existing
- [Tests expect empty Add clip row] → Mitigation: update `inspector_add_menu_commands_test` / clip-map tests to picker-confirm / uniqueness

## Migration Plan

1. Uniqueness + load discard + hide GUID / show clip identity (keep temporary assign path if picker not ready)
2. AnimationClip picker + Add clip / per-row retarget Commands
3. Content Browser AnimationClip drop (append / retarget)
4. Behaviour catalog mark + dropdown
5. Manual smoke: Chocomel idle/walk bind → PlayerMove IdleClip/WalkClip dropdown → Play

Rollback: revert Inspector surfaces; storage format unchanged.

## Open Questions

None — grilled into CONTEXT / ADR 0036.
