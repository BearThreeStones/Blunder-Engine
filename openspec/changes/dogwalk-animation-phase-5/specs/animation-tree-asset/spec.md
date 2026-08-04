## ADDED Requirements

### Requirement: AnimationTree Asset with Inspector authorship
The engine SHALL support a first-class **AnimationTree Asset** identified by GUID whose body stores reusable AnimationTree topology. Authors SHALL be able to edit topology via **Inspector** (visual node canvas SHALL NOT be required for Phase 5 D Done). An Object's AnimationTree MAY reference the Asset by GUID.

#### Scenario: Asset round-trip
- **WHEN** an author saves BlendSpace and StateMachine topology into an AnimationTree Asset and references it from an Object
- **THEN** reloading the project restores topology via the GUID reference without a visual canvas

### Requirement: Asset base with small scene overrides
When an AnimationTree references an AnimationTree Asset, runtime topology SHALL use the **Asset as base**. The scene MAY store the reference plus a **small instance-override** set (allowlist at apply-time). With **no** Asset reference, Phase 4 **scene-embedded** topology SHALL remain valid.

#### Scenario: No asset uses embed
- **WHEN** an AnimationTree has no Asset reference and has embedded topology
- **THEN** sampling uses the embedded topology (Phase 4 compatible)

#### Scenario: Asset preferred over full embed duplicate
- **WHEN** an AnimationTree references an Asset
- **THEN** the product path does not require a full in-scene duplicate of the graph as the primary reuse mechanism
