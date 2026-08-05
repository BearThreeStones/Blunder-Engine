# Phase 6 explicit OUT confirmations

Recorded for tasks **4.3** and **4.4** (authorship / persistence gate). These items are **not** required for DogWalk animation Phase 6 Done and **no product implementation** is planned in this phase.

## 4.3 — Visual AnimationTree canvas is not Phase 6 Done

**Confirmed:** Phase 6 Done does **not** require a visual node-graph / canvas editor for AnimationTree topology.

**Rationale:**

- Locked **Non-Goals** in `design.md` and **Out of scope** in `proposal.md`: *Tree canvas-as-Done*.
- [ADR 0027](../../../docs/adr/0027-animation-phase-6.md) and `CONTEXT.md` Phase 6 glossary repeat the same explicit OUT.
- Phase 6 authorship for the three **SkeletonModifier** product types is delivered via **scene serialization** (task 4.1) and **Inspector** add/remove/reorder/enable + key params (task 4.2) — see `specs/skeleton-modifier-authorship/spec.md` (*without a visual AnimationTree canvas*).
- AnimationTree canvas was already optional for Phase 4 (task 3.3) and Phase 5 D-core (Inspector topology; canvas optional). Phase 6 does not reopen that scope.
- No canvas editor code, Slint panel, or graph-authoring UI exists in this change; none is needed to meet Phase 6 Done.

**Phase 6 Done bar for tree/modifier authoring:** Inspector + scene-embedded data for PaperMouth, SkeletonAttachModifier, and LookAt — not a Tree graph canvas.

## 4.4 — Generic C# SkeletonModifier subclass hot-bridge is not Phase 6 Done

**Confirmed:** Phase 6 Done does **not** require a generic C# `SkeletonModifier` subclass **hot-bridge** (runtime registration / assembly-loaded custom modifier types).

**Rationale:**

- Locked **Non-Goals** in `design.md` (*Generic C# SkeletonModifier subclass hot-bridge*) and matching **Out of scope** in `proposal.md`.
- [ADR 0027](../../../docs/adr/0027-animation-phase-6.md) decision 5: *ClassDB + scene serialize for the three; no C# subclass bridge Done* — extension point may remain; hot C# subclass is not required.
- `specs/skeleton-look-at-product/spec.md` states Phase 6 Done SHALL NOT require this bridge.
- Shipped product path is three **engine ClassDB** types (`PaperMouth`, `SkeletonAttachModifier`, `SkeletonLookAtModifier`) with scene round-trip and Inspector authorship (tasks 4.1–4.2).
- No C# subclass bridge, `DotNetHost` modifier factory, or managed `SkeletonModifier` derivation API was added for Phase 6; none is required for Done.

**Future work:** A generic managed modifier bridge may be considered in a later phase; it is explicitly deferred from Phase 6.
