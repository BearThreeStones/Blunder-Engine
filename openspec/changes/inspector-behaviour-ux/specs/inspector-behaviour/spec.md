## ADDED Requirements

### Requirement: Inspector edits Behaviour declarations without a live peer
In Edit Mode the Inspector SHALL author an entity's ordered Behaviour declarations (type, BehaviourId, bool/number/string property bag) without requiring a mounted Script Peer or product DotNetHost.

#### Scenario: Add Behaviour with no host
- **WHEN** DotNetHost is not running and the author Adds a catalogued Behaviour type to the selected entity
- **THEN** the entity's bound Object gains a Behaviour slot with that type and a new BehaviourId, peer remains null, and the document is dirty for save

#### Scenario: Property commit without peer
- **WHEN** the author commits a bool/number/string field on a Behaviour in the Inspector
- **THEN** the value is stored on that Behaviour's property bag on the Object and survives scene export/reload

### Requirement: Add uses Behaviour type catalog
The Inspector Add Behaviour picker SHALL list concrete non-abstract Behaviour types from the Project Behaviour type catalog produced after a successful Scripts build. The catalog SHALL include public bool/number/string instance member metadata for form rows.

#### Scenario: Catalog after build
- **WHEN** Scripts build succeeds and writes/refreshes the catalog
- **THEN** Add lists those Behaviour types and property forms show catalog members for a selected declaration of a known type

#### Scenario: Missing catalog prompts build
- **WHEN** the author opens Add and no catalog is available
- **THEN** the UI prompts or triggers Scripts build rather than requiring CoreCLR ScriptHost start for authoring

### Requirement: Missing types stay visible
A declaration whose type is absent from the catalog SHALL remain listed as a missing/broken entry. The author MAY Remove it. The editor SHALL NOT silently drop it or block Save/Play solely for that reason.

#### Scenario: Broken entry after rename
- **WHEN** a scene declares type T and the catalog no longer contains T
- **THEN** Inspector shows the entry as missing, allows Remove, and Save still writes the declaration if not removed

### Requirement: Behaviour edits are History Commands
Add, Remove, drag reorder, and property commits (Enter or focus loss) SHALL each push an Editor Command on Document History addressed by EntityId. Intermediate typed drafts SHALL NOT each push a Command.

#### Scenario: Undo Add
- **WHEN** the author Adds a Behaviour and undoes
- **THEN** that Behaviour slot is removed from the Object and Inspector list

#### Scenario: Undo reorder
- **WHEN** the author drag-reorders Behaviours and undoes
- **THEN** list order matches the pre-reorder order (Ready/Tick order follows the list)

### Requirement: Property form is catalog-driven
For a declaration whose type is in the catalog, Inspector SHALL show one editor row per catalogued bool/number/string member. Values come from the property bag when present; otherwise type-appropriate defaults. Bag keys not in the catalog SHALL be retained on save but need not show as primary rows.

#### Scenario: Empty bag shows defaults
- **WHEN** a newly Added Behaviour has an empty property bag and the catalog lists members Speed (float) and Label (string)
- **THEN** Inspector shows Speed and Label with default values and committing Label writes it into the bag
