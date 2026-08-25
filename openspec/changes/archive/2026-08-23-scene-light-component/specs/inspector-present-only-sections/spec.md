## MODIFIED Requirements

### Requirement: Unique sections appear only when present
The Inspector SHALL draw the Camera, Light, Skeleton, Animation Player, and Animation Tree property sections if and only if that Unique attachment is present on the current selection. The Inspector SHALL NOT show empty Unique foldouts or placeholder copy such as `No Skeleton on entity` or `No AnimationPlayer on entity`. Absence SHALL be communicated by not drawing the section. Local Transform SHALL remain independent of this rule.

#### Scenario: Mesh-only entity hides Unique sections
- **WHEN** exactly one entity is selected and it has no Camera, no Light, no Skeleton, no AnimationPlayer, and no AnimationTree
- **THEN** the Inspector does not show Camera, Light, Skeleton, Animation Player, or Animation Tree sections
- **AND** Add… remains available

#### Scenario: Add AnimationPlayer reveals cascaded sections
- **WHEN** the author adds AnimationPlayer to a selection that had no animation hosts
- **THEN** the Inspector shows both Skeleton and Animation Player sections
- **AND** it still does not show Camera, Light, or Animation Tree unless those were also created

#### Scenario: Remove Player hides Player section
- **WHEN** the Object has Skeleton and AnimationPlayer and the author Removes AnimationPlayer
- **THEN** the Animation Player section is not shown
- **AND** the Skeleton section remains

#### Scenario: Undo Add Camera hides Camera section
- **WHEN** the author adds Camera and then undoes
- **THEN** the Camera section is not shown

#### Scenario: Undo Add Light hides Light section
- **WHEN** the author adds Light and then undoes
- **THEN** the Light section is not shown
