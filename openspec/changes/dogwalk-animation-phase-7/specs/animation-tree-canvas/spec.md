## ADDED Requirements

### Requirement: AnimationTree Canvas authors Asset topology
The editor SHALL provide an **AnimationTree Canvas** that creates and edits **AnimationTree Asset** topology as the product truth source. The canvas SHALL NOT treat scene-embedded topology as the primary truth source when an Asset is referenced. Phase 4 embed-only SHALL remain valid when no Asset is set.

#### Scenario: Edit Asset on canvas
- **WHEN** an author opens an AnimationTree Asset in the Canvas and adds a BlendSpace1D point
- **THEN** saving persists that topology on the Asset body (GUID unchanged)

#### Scenario: Embed not canvas truth with Asset
- **WHEN** a scene Object references an AnimationTree Asset
- **THEN** the Canvas edits the Asset, not a full in-scene graph duplicate as the primary path

### Requirement: Dual-track with Inspector
Authors SHALL be able to edit the same AnimationTree Asset topology via **Canvas** and via **Inspector**. Shipping Canvas SHALL NOT remove Inspector topology authorship.

#### Scenario: Inspector still writable
- **WHEN** Canvas has saved a StateMachine state onto an Asset
- **THEN** Inspector can still modify that Asset's topology fields and round-trip on reload

### Requirement: Layout persists in Asset
Canvas node positions (and view layout needed to restore the graph) SHALL persist in the **AnimationTree Asset** body with topology — not only as ephemeral per-machine editor preferences.

#### Scenario: Reopen preserves layout
- **WHEN** an author positions nodes on the Canvas, saves the Asset, and reopens it on another machine with the same Asset
- **THEN** node positions restore from the Asset body

### Requirement: Dual open paths
Authors SHALL be able to open the Canvas from the Content Browser on an AnimationTree Asset **and** from the Inspector on a scene Object that references that Asset. Both paths SHALL edit the same Asset.

#### Scenario: Object Inspector opens same Asset
- **WHEN** an author selects an Object with an AnimationTree Asset reference and opens Canvas from Inspector
- **THEN** the Canvas document is that Asset GUID

### Requirement: Topology-only preview for Done
Phase 7 Canvas Done SHALL NOT require a bound live Skeleton preview character or an in-canvas mini viewport. Authors MAY validate sampling via a scene Object referencing the Asset and existing Edit scrub.

#### Scenario: Done without bound preview
- **WHEN** engineering validates Canvas authorship gates
- **THEN** absence of in-canvas live preview does not fail Phase 7 Done

### Requirement: Phase 5 node set on canvas
The Canvas SHALL support authoring **StateMachine**, **BlendSpace1D**, **BlendSpace2D**, **OneShot**, and **Add2** nodes/slots consistent with Phase 5 AnimationTree capabilities. Godot-complete extra node types SHALL NOT be required for Phase 7 Done.

#### Scenario: BlendSpace2D on canvas
- **WHEN** an author adds BlendSpace2D points on the Canvas for an Asset
- **THEN** those points round-trip on the Asset and are usable by runtime sampling
