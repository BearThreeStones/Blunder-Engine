# skeleton-modifier-type-catalog Specification

## Purpose
Lets first-party engine code register SkeletonModifier types once, then construct, deserialize, and list them for Add… without parallel string switches or a second reflection database.
## Requirements
### Requirement: Catalog constructs by ClassDB type name
The engine SHALL maintain a SkeletonModifier type catalog beside ClassDB. Each registration SHALL bind a ClassDB type name to a factory and an Add… visibility flag. Construct-by-name SHALL return an instance of that type when the name is registered. The catalog SHALL NOT store Inspector property schemas; ClassDB remains the source of properties and methods.

#### Scenario: Construct PaperMouth by name
- **WHEN** builtins are registered and the catalog constructs `"PaperMouth"`
- **THEN** the instance reports type name `"PaperMouth"` and is a PaperMouth modifier

#### Scenario: Catalog is not a property database
- **WHEN** Inspector edits PaperMouth `openAmount`
- **THEN** that field is read and written through ClassDB, not through catalog schemas

### Requirement: Reversible registration
A catalog registration SHALL be removable. After unregister, construct-by-name for that type SHALL fail (unknown), and the type SHALL NOT appear in the Add… Skeleton Modifiers list. Unregister SHALL NOT tear down ClassDB or the Object that owns a modifier chain.

#### Scenario: Test double unregister
- **WHEN** a test registers type `"CatalogTestDouble"` with Add… hidden, then unregisters it
- **THEN** construct-by-name for `"CatalogTestDouble"` fails and Add… does not list it

### Requirement: Add… lists visible catalog types
The Add… Skeleton Modifiers group SHALL list catalog type names whose Add… visibility is enabled, and SHALL NOT list hidden registrations or the abstract base type as an addable product row.

#### Scenario: Product types appear
- **WHEN** builtins are registered and the author opens Add… Skeleton Modifiers
- **THEN** the list includes PaperMouth, SkeletonAttachModifier, and SkeletonLookAtModifier

#### Scenario: Hidden types omitted
- **WHEN** a type is registered with Add… visibility disabled
- **THEN** that type is absent from the Add… Skeleton Modifiers group

### Requirement: Unknown type becomes Missing SkeletonModifier
When scene load or construct-by-name meets a type name that is not in the catalog, the engine SHALL insert a Missing SkeletonModifier slot that preserves the authored type name. It SHALL NOT coerce the slot to the abstract base SkeletonModifier. Missing slots SHALL NOT apply a pose, SHALL show as broken in the Inspector, MAY be removed, and SHALL NOT block Save or Play. Missing SHALL NOT appear as an Add… item and SHALL NOT be a product ClassDB type.

#### Scenario: Unknown type keeps its name
- **WHEN** a scene entity has a skeletonModifiers entry whose type is not in the catalog
- **THEN** the loaded chain slot reports that authored type name and does not report the abstract base type name as if it were a product slot

#### Scenario: Missing does not apply
- **WHEN** a Missing SkeletonModifier is in the chain during Animation Pipeline stage 4
- **THEN** it does not change Skeleton pose

#### Scenario: Missing does not block Save
- **WHEN** the open scene contains a Missing SkeletonModifier and the author saves
- **THEN** Save succeeds

### Requirement: Missing slots round-trip unknown fields
A Missing SkeletonModifier SHALL keep an opaque bag of fields other than `type` from the authored JSON object. Save SHALL write those fields back. The bag SHALL NOT be interpreted as ClassDB properties.

#### Scenario: Extra keys survive Save
- **WHEN** an unknown type object includes extra keys (for example a string and a number) and the scene is saved without removing that slot
- **THEN** the saved JSON for that slot still has the same type name and those extra keys with the same values

#### Scenario: Nested extra values survive Save
- **WHEN** an unknown type object includes a nested JSON object or array field and the scene is saved without removing that slot
- **THEN** the saved JSON still contains that nested field

### Requirement: Both hosts use the same catalog
Editor Session and Player SHALL construct and deserialize SkeletonModifiers through the same catalog. The catalog MUST NOT be Editor-only.

#### Scenario: Player loads product modifiers
- **WHEN** Player loads a scene that contains PaperMouth
- **THEN** the Object chain has a PaperMouth slot constructed from the catalog

