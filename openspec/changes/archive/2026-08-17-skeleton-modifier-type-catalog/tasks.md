## 1. Catalog kernel

- [x] 1.1 Add `skeleton_modifier_catalog` (register / construct / Add… names / disposer / clear) under `runtime/core/object/`; wire into `engine_runtime` CMake
- [x] 1.2 Clear the catalog from `ClassDB::clear`; register three product builtins after generated modifier reflection in `ClassDB::initialize`
- [x] 1.3 Tests: construct PaperMouth / Attach / LookAt by name; initialize twice does not duplicate Add… rows; base type is not addable

## 2. Reversible test registration

- [x] 2.1 Allow register without ClassDB; `show_in_add_menu = false` omitted from Add… list
- [x] 2.2 Tests: register `CatalogTestDouble`, construct, unregister, construct fails and Add… list does not include it

## 3. Missing slot and opaque bag

- [x] 3.1 Missing SkeletonModifier: authored `getTypeName()`, no-op `apply`, stores leftover JSON fields; not a ClassDB product type
- [x] 3.2 Extend `SceneSkeletonModifierDef` + parse/write so unknown types keep leftover key → raw JSON values (nested object/array included)
- [x] 3.3 `applySkeletonModifierDefinition` / `makeSkeletonModifierFromDef` construct via catalog; unknown → Missing, never bare `"SkeletonModifier"`
- [x] 3.4 Tests: load unknown type keeps authored name; Save round-trips extra string/number and a nested object; stage 4 apply does not change pose; Save succeeds

## 4. Inspector and call sites

- [x] 4.1 `buildSkeletonModifierTypeChoices` and Add… dispatch construct from the catalog
- [x] 4.2 Inspector shows Missing as broken (type name visible, fields not product-edited, Remove allowed); do not block Save/Play
- [x] 4.3 Tests: Add… list is the three product names; existing modifier command + phase6 serialize tests still pass

## 5. Validation

- [x] 5.1 CONTEXT / ADR 0035 remain aligned with shipped names (prefer no churn)
- [x] 5.2 Build `engine_editor`; run catalog tests plus `dogwalk_phase6_modifier_serialize_test` and `inspector_skeleton_modifier_commands_test`
