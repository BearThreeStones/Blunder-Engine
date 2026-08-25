## Context

See proposal.md for motivation. First Add… slice (`inspector-add-menu`) replaced parallel Add buttons with one picker but left Unique / Behaviours / Skeleton Modifiers as always-on foldouts. Snapshot flags already exist (`has-camera`, `has-skeleton`, `has-animation-player`, `has-animation-tree`, `behaviours`, `skeleton-modifiers`). Docked and floating Inspector share `inspector_panel.slint`. Add… catalog Unique-disable rules stay; this slice only gates property sections.

## Goals / Non-Goals

**Goals:**
- Gate each Add…-authored section (header + body + adjacent divider) on presence / list length
- Drop empty Unique placeholder strings
- Keep Add… catalog rows, including grey Unique items

**Non-Goals:**
- New snapshot properties or Commands
- Changing host cascade, hydration, or Remove rules
- Hiding Local Transform, entity name, or Mesh spawn surfaces
- Auto-scroll / focus the new section after Add…

## Decisions

### D1 — Slint `if` on existing flags
**Choice:** Wrap each Unique section in `if root.has-camera` / `has-skeleton` / `has-animation-player` / `has-animation-tree`. Wrap Behaviours in `if root.behaviours.length > 0`, Skeleton Modifiers in `if root.skeleton-modifiers.length > 0`. Include the section header, expanded body, and the divider that belonged to that block so hidden sections do not leave stacked empty lines.
**Why:** Flags already sync from `slint_system`; no native visibility helper needed.
**Rejected:** C++ “section list” model; keeping headers and only hiding bodies (still looks like empty slots).

### D2 — Behaviours / Modifiers follow Unique
**Choice:** Empty list sections hide entirely, same as Unique. Add… is the empty-state for those types.
**Why:** Same smell as `No Skeleton on entity`; screenshot still showed empty Behaviours / Skeleton Modifiers headers.
**Rejected:** Unique-only hide (leaves two empty headers under Add…).

### D3 — Catalog vs property surface
**Choice:** Do not hide Unique rows in the Add… popup. Present-only applies only to property sections below Add….
**Why:** First-slice glossary: Unique rows stay visible and disabled when present. Authors still see what can be added.
**Rejected:** Hiding Unique catalog rows once attached (would look like they vanished from Add…).

### D4 — Expand state
**Choice:** Keep existing in-out `*-expanded` defaults (`true`). When a section first appears after Add…, it shows expanded. Do not add a one-shot “force expand on add” path.
**Why:** Smallest change; default already expanded.
**Rejected:** Collapsed-on-first-show; native callback to expand after Add….

### D5 — Docs, not a new ADR
**Choice:** Add a CONTEXT glossary term (Inspector present-only sections). No ADR; ADR 0033 still covers Add… identity.
**Why:** Visibility of existing attachments is presentation, not a new runtime identity rule.
**Rejected:** ADR 0035 for foldout gating.

## Risks / Trade-offs

- [Divider gaps / double rules] → Wrap each section with the divider that currently precedes or follows it; visual-check docked + floating Inspector
- [Cascade Add Player] → Existing flags already flip together; both Skeleton and Player sections appear without extra wiring
- [No-selection] → Unique/list sections stay hidden (flags false / empty); Transform empty-state unchanged

## Migration Plan

1. Gate Unique sections in `inspector_panel.slint`; delete placeholder Text
2. Gate Behaviours / Skeleton Modifiers the same way
3. CONTEXT glossary
4. Build `engine_editor`; smoke mesh-only, Add Player, Remove Player, Add/Remove last Behaviour

Rollback: revert the `if` wrappers; Commands and Add… catalog unchanged.

## Open Questions

None.
