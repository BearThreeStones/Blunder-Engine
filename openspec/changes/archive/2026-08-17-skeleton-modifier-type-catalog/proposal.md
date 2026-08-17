## Why

Adding a SkeletonModifier type today means editing at least three string switches (`addSkeletonModifierByType`, `makeSkeletonModifierFromDef`, `buildSkeletonModifierTypeChoices`) plus a fourth deserialize `else` that silently coerces unknown types to the base class and drops the name. That blocks first-party extension beside ClassDB and destroys forward-compat when an older editor opens a newer scene (ADR 0035).

## What Changes

- Introduce a **SkeletonModifier type catalog**: a reversible Seam registration table beside ClassDB (factory + Add… visibility), keyed by ClassDB class names
- Product types (PaperMouth, SkeletonAttachModifier, SkeletonLookAtModifier) register builtins at engine init; abstract `SkeletonModifier` is not an Add… item
- Construct-by-name, scene deserialize, and the Add… Skeleton Modifiers group all consume the catalog — no parallel type lists
- Unknown types load as a **Missing SkeletonModifier** (authored type name + opaque field bag); `apply` is a no-op; Inspector shows broken; Remove allowed; Save writes the bag back; does not block Save or Play
- Tests can register a factory (Add… hidden) and unregister via disposer without touching ClassDB
- ClassDB stays properties / methods / Inspector fields; the catalog does not store field schemas

**Out of scope:** ClassDB instantiate/enumerate-subclasses; Editor Command type registry; Unique-attachment Add… as a Seam; Import codec Seam; moving Registered Systems off `RuntimeGlobalContext`; Host composition YAML; third-party plugin ABI; C# SkeletonModifier subclass bridge; per-type serialize hooks in the catalog

## Capabilities

### New Capabilities
- `skeleton-modifier-type-catalog`: factory table beside ClassDB, builtin registration, Add… visibility, Missing SkeletonModifier + opaque bag, reversible test registration

### Modified Capabilities
- `skeleton-modifier`: construct-by-name and scene load of modifier slots MUST go through the catalog; unknown types MUST NOT coerce to the base class

## Impact

- New catalog API (register / construct / list Add… names / disposer) under runtime core beside `SkeletonModifier`
- `inspector_skeleton_modifier_ops.h` factory switches; `SceneInstance::applySkeletonModifierDefinition`; scene serializer unknown-key round-trip
- Add… Modifier group (`buildSkeletonModifierTypeChoices` / Slint choices)
- Builtin registration after ClassDB generated reflection (not inside generated files)
- Tests: construct product types; test-double register/unregister; missing type + bag Save round-trip; Add… list excludes hidden/base
- Docs already in CONTEXT.md and ADR 0035 — keep them aligned if names drift
