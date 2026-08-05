# Phase 1–5 content Done gates — separate tracking (Phase 6 task 6.3)

DogWalk animation **Phase 6** engine and modifier content work MAY proceed while earlier phase content Done gates remain open. Those criteria **remain in force** and **SHALL NOT** be silently replaced or closed by Phase 6 acceptance alone.

**Spec:** `specs/dogwalk-animation-content/spec.md` — *Parallel with Phase 1–5 content gates*.

**Phase 6 does not claim Phase 1–5 closed.** Completing Phase 6 tasks (including 6.1–6.2 lean bars) does not check boxes in prior phase `tasks.md` files.

---

## Open content / human gates (as of Phase 6 task 6.3)

| Phase | Change folder | Task(s) | Gate | Status |
|-------|---------------|---------|------|--------|
| 1 | `dogwalk-animation-phase-1` | **6.4** | Chocomel (or agreed subset) Play: idle↔walk hard cut, real-time move, stepped facing | **Open** |
| 1 | `dogwalk-animation-phase-1` | **6.5** | Inspector: name→GUID map editable after auto-fill | **Open** |
| 1 | `dogwalk-animation-phase-1` | **7.2** | Manual checklist: Edit preview; Play Pause/Stop; CPU/GPU smoke | **Open** |
| 2 | `dogwalk-animation-phase-2` | **5.3** | Chocomel weighted idle↔walk Play + TimeScale + stepped facing | **Open** |
| 2 | `dogwalk-animation-phase-2` | **6.2** | Manual checklist: Edit scrub; weighted Play; skin under blend | **Open** |
| 3 | `dogwalk-animation-phase-3` | **5.2** | Mini Play: SYNC+CINE handoff (character + prop/partner) | **Open** |
| 3 | `dogwalk-animation-phase-3` | **5.2** | Human steps | See `dogwalk-animation-phase-3/manual-checklist.md` |
| 4 | `dogwalk-animation-phase-4` | **6.2** | Chocomel-subset Play: BlendSpace, Add2, OneShot, stepped facing | **Open** (human) |
| 4 | `dogwalk-animation-phase-4` | **6.2** | Human steps | See `dogwalk-animation-phase-4/manual-checklist.md` §B |
| 5 | `dogwalk-animation-phase-5` | **8.2** / §B | Lean Play A/C/D human feel (modifier, BlendSpace2D, Tree Asset) | **Open** (human) |
| 5 | `dogwalk-animation-phase-5` | **8.2** / §B | Human steps | See `dogwalk-animation-phase-5/manual-checklist.md` §B |

Engineering / automated lean bars for Phase 5 (**7.4–7.6**) and Phase 6 (**6.1–6.2**) are tracked in their own phase `tasks.md` and do **not** substitute for the human Chocomel / mini SYNC+CINE / lean Play rows above.

---

## Earlier phase closeout (non-content; still open if unchecked)

These are docs/validation tasks in prior phases; Phase 6 does not close them:

| Phase | Task(s) | Note |
|-------|---------|------|
| 1 | **7.1**, **7.3** | Automated test sweep; CONTEXT drift |
| 2 | — | Content gate **5.4** (track Phase 1 Chocomel) satisfied by explicit tracking in Phase 2+ |
| 3 | — | Content gate **5.3** (track Phase 2) satisfied by explicit tracking in Phase 3+ |
| 4 | **6.3** | Prior-gate tracking done in Phase 4; see that change `manual-checklist.md` §C |
| 5 | **7.7**, **8.2** §C | Prior-gate tracking + checklist authorship done; human §B/C boxes may remain open |

---

## Regression policy

When validating Phase 6 (task **7.2** manual checklist, when authored):

- Do **not** mark Phase 1–5 content tasks complete unless their own acceptance bars pass.
- Phase 6 modifier lean Play/Edit (**6.2**) does **not** satisfy Phase 4 Chocomel-subset, Phase 5 lean Play human bars, or Phase 1–3 Chocomel / SYNC+CINE gates.
- Prior phase manual checklists §C (or equivalent) remain the source of truth for “earlier gates still tracked.”

---

## Sign-off (task 6.3)

| Field | Value |
|-------|--------|
| Documented by | Task 6.3 (Phase 6) |
| Date | 2026-08-05 |
| Result | Phase 1–5 content Done gates listed and kept separate from Phase 6 Done |
