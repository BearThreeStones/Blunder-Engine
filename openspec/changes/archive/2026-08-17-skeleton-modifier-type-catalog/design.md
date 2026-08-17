## Context

See proposal.md for motivation. Today three (plus deserialize) string switches invent modifier instances: `addSkeletonModifierByType`, `makeSkeletonModifierFromDef`, `buildSkeletonModifierTypeChoices`, and `SceneInstance::applySkeletonModifierDefinition` (unknown → `addSkeletonModifier()`). ClassDB already registers `PaperMouth` / `SkeletonAttachModifier` / `SkeletonLookAtModifier` with parent `SkeletonModifier` but has no instantiate or subclass listing (`class_db.h`). Scene JSON uses `SceneSkeletonModifierDef` as a typed union (`scene.h`); serializer parse/write is per-product-field (`parseSkeletonModifierObject` / `appendSkeletonModifierJson`). Add… Modifier rows are a hardcoded three-name list. Grill/ADR 0035: catalog beside ClassDB; first slice is this Seam only.

## Goals / Non-Goals

**Goals:**
- One register/construct/list API consumed by Inspector, scene instantiate, and tests
- Builtins registered on `ClassDB::initialize` after generated reflection, cleared on shutdown
- Missing slot + opaque extra-field round-trip without a second ClassDB schema
- Existing product serialize tests keep passing

**Non-Goals:**
- ClassDB instantiate API
- Moving editor Systems off `RuntimeGlobalContext`
- Rewriting product field capture onto ClassDB getters (PaperMouth `openAmount` switches stay until a later slice)
- Inspector editors for opaque bag keys (broken row + Remove only)
- C-ABI generic construct-by-name

## Decisions

### D1 — Hand-written catalog, not generated reflection
**Choice:** New `skeleton_modifier_catalog` (header + cpp) in `runtime/core/object/`. `ClassDB::initialize` calls `registerBuiltinSkeletonModifierTypes()` after the generated `register_*_modifier_reflection()` functions. Generated `*_registration.cpp` files stay ClassDB-only.
**Why:** Those files are Clang-export output and would wipe factory hooks on regen. Catalog can `ClassDB::hasClass` for product names after reflection is in.
**Rejected:** Putting `catalog.register` inside generated registration; static constructors (MSVC order).

### D2 — Registration is factory + `show_in_add_menu`
**Choice:** `registerType(name, factory, show_in_add_menu)` returns a disposer (or id) that removes that row. Builtins: three product types `show_in_add_menu = true`. Base `"SkeletonModifier"` is not addable. Test doubles may skip ClassDB and set `show_in_add_menu = false`.
**Why:** Matches ADR 0035; Add… and deserialize share one table; tests do not pollute the picker.
**Rejected:** Factory-only with a name blacklist; ClassDB subclass walk.

### D3 — Missing is a runtime slot, not a ClassDB product
**Choice:** A `MissingSkeletonModifier` (or equivalent) subclass stores `authored_type` + extra fields. `getTypeName()` returns the **authored** name so Save writes the original type. `apply()` is a no-op. Not registered in ClassDB. Inspector treats `!catalog.has(name)` or an explicit missing flag as broken.
**Why:** Coercing to base drops the name (today’s bug). A ClassDB class named Missing would collide with product listing and bindings.
**Rejected:** Bare `SkeletonModifier` with a side channel for the real name; failing scene load.

### D4 — Opaque bag is leftover JSON values on the def and the slot
**Choice:** Extend `SceneSkeletonModifierDef` with leftover key → raw JSON value tokens (not ClassDB Variants-as-schema). Parser: after `type`, keep every other property’s raw JSON value. Product types still fill the existing typed union for known keys; leftover keys on **unknown** types go to the bag (known product keys on a missing type also go to the bag — there is no product schema). Writer: if the slot is Missing, emit `type` plus bag keys as raw JSON; do not run PaperMouth/LookAt/Attach field branches. Nested objects/arrays round-trip because values stay raw JSON.
**Why:** Grill option 2 (key bag, not whole-object blob). Raw tokens beat bool/number/string-only Variant bags for nested forward-compat.
**Rejected:** Whole modifier object as one string blob; putting extra keys into ClassDB properties; dropping extras on Save.

### D5 — Construct path replaces switches, field path does not
**Choice:** `makeSkeletonModifierFromDef` / `addSkeletonModifierByType` / `applySkeletonModifierDefinition` call `construct(name)` then apply **existing** typed field helpers for product types. Missing: construct missing slot from def.bag. `captureSkeletonModifierDef` for Missing copies type + bag; product types keep today’s per-type capture.
**Why:** First slice kills the three factory lists, not the property switches (explicitly not a serialize-hook Seam).
**Rejected:** Per-type serialize hooks on the catalog (proposal out of scope).

### D6 — Initialize/clear with ClassDB
**Choice:** Catalog `clear()` in `ClassDB::clear()` (or immediately beside it) so `ClassDB::initialize()` remains idempotent: clear then register builtins. Player already calls `ClassDB::initialize` in `startSystems`; no Editor-only init.
**Why:** Both-hosts requirement; tests that re-init ClassDB must not duplicate rows.
**Rejected:** Lazy first-construct registration; Editor-only Slint init.

## Risks / Trade-offs

- [Raw JSON bag is another mini-parser] → Mitigation: reuse the existing bounded object scanner; store value substrings already delimited by the current parser; tests with nested object + array
- [Inspector has no broken-modifier chrome yet] → Mitigation: reuse missing-Behaviour visual language if present; otherwise type name + disabled fields + Remove. Do not block this slice on new Slint chrome
- [Export vs live Object mismatch] → Mitigation: instantiate still builds live Missing slots so export/`captureSkeletonModifierDef` round-trips from the Object, not only from the def
- [Duplicate builtin register if initialize skipped clear] → Mitigation: D6; test initialize twice yields one Add… row per product type

## Migration Plan

1. Catalog API + builtin register + unit tests (construct, disposer, Add… list)
2. Wire construct into inspector ops, scene instantiate, serializer bag
3. Missing slot + Inspector broken/Remove; product serialize tests still green
4. Delete the three hardcoded name lists

Rollback: revert catalog consumers first; leftover catalog files unused if switches restored.

## Open Questions

None — grilled into CONTEXT / ADR 0035.
