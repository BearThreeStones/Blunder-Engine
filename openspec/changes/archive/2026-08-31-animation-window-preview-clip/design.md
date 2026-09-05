## Context

See `proposal.md` for why. Grill locked **Preview clip** in CONTEXT. The Animation Window combo is session state `m_fire_target`. `play()` Clip Plays `m_default_clip_name` (scene default). `defaultFireTargetFromBindings()` picks `fireClipNames().front()`, which can differ from the default (Chocomel: combo idle, Play walk). `setFireTarget` only stores the name. `stop()` calls `tree->clearClipPlay()`. `clipPlay` fails if the tree is inactive. `bindSelection` on a new Tree calls `haltBoundSession()` then `bindObject`; same-object bind early-returns and must not re-Clip Play every UI tick.

Sibling change `animation-window-clip-anatomy` (unarchived) owns Clip anatomy; this change does not redraw lanes.

## Goals / Non-Goals

**Goals:**

- Make Preview clip the name Play, bind, dropdown change, and Fire all address.
- Keep one combo. Keep Fire as insert-on-Fire-slot.
- Tests cover Play vs default, dropdown change in each transport state, Stop keeps override, bind Clip Play, Fire occupying + retarget, rebind clears the previous tree.

**Non-Goals:**

- A second dropdown or a new preview Unique.
- Changing AnimationTree `clipPlay` semantics (still hard-cut, still fails when inactive).
- Serializing Clip Play override.
- An ADR (Grill did not park a hard-to-reverse trade-off).

## Decisions

### D1 — Keep one session string; Play reads it
**Choice:** Treat `m_fire_target` as Preview clip (rename the member and Slint property if the diff stays local). `play()` with an empty argument Clip Plays Preview clip, not `m_default_clip_name`. Scene default is only the bind-time initial Preview clip when that name exists on the tree; otherwise the first binding name.
**Why:** One combo already lists Clip Binding names. The idle/walk bug is Play ignoring that string.
**Rejected:** A second Clip Play combo; Play keeping the hidden default.

### D2 — `setPreviewClip` Clip Plays and does not touch transport
**Choice:** Changing the combo calls one setter: store the name, activate the tree if inactive, `clipPlay` the name (clock 0). Do not set Playing / Paused / Stopped. Same-name Clip Play still hard-cuts (existing tree rule) if the UI emits a change.
**Why:** Grill D2/D3: every transport state hard-cuts; only Playing continues advancing.
**Rejected:** Deferring Clip Play until the next Play click.

### D3 — Bind Clip Plays once; unbind still `clearClipPlay`
**Choice:** When the bound Object *changes* to a Tree, set Preview clip to the scene default, `setActive(true)` if needed, `clipPlay` at 0, leave transport Stopped. Same-object `bindSelection` refresh does not Clip Play again. `stop()` seeks 0, `clearOneShot`, Ends CINE, and does **not** `clearClipPlay`. `haltBoundSession` / `clearTarget` still `clearClipPlay` on the tree being left so Hierarchy unbind matches the spec.
**Why:** `haltBoundSession()` today is `stop()`. After Stop stops clearing override, unbind must keep a separate clear.
**Rejected:** Stop clearing override; Clip Playing on every bindSelection tick.

### D4 — Fire occupying: setter still Clip Plays
**Choice:** No special-case skip. `clipPlay` retargets the base; the Fire slot stays; ruler stays on the insert (`rulerClipName` already prefers OneShot).
**Why:** Existing sample stack. Grill D2 Fire clause.
**Rejected:** Clearing Fire when Preview clip changes; changing the ruler to Preview clip while Fire occupies.

## Risks / Trade-offs

- [Same-object bindSelection every frame] → Early-return in `bindObject` must stay; only Object change Clip Plays.
- [`clipPlay` no-ops on inactive tree] → Bind and the Preview clip setter activate first, then Clip Play, without starting transport.
- [Two unarchived animation-window deltas] → This delta restates TimeScale dirty-exclusions including Clip anatomy filter/fold so archive does not drop them.
- [Tests named `stop clears override`] → Flip the assertion; add bind-default and dropdown-change cases.

## Migration Plan

1. Controller + tests first (Play, setter, Stop, bind, unbind, Fire-occupying).
2. Slint property rename if the combo still says fire-target.
3. No content migration.

Rollback: revert; combo is Fire-only again; Play uses scene default; Stop clears override.

## Open Questions

None.
