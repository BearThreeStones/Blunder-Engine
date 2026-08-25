# inspector-present-only-sections Specification

## Purpose

Inspector draws Add…-authored property sections only when the current selection actually has that attachment or list entry, so empty Unique / Behaviours / Skeleton Modifiers foldouts do not sit beside the picker.

## Requirements

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

### Requirement: List sections appear only when non-empty
The Inspector SHALL draw the Behaviours section only when the selection has at least one Behaviour declaration. The Inspector SHALL draw the Skeleton Modifiers section only when the selection has at least one SkeletonModifier. Empty headers for those lists SHALL NOT be shown. Adding a Behaviour or SkeletonModifier via Add… SHALL reveal the corresponding section. Removing the last row SHALL hide it.

#### Scenario: No Behaviours hides Behaviours section
- **WHEN** the selected entity has no Behaviour declarations
- **THEN** the Inspector does not show a Behaviours section
- **AND** Behaviour types remain listed in Add…

#### Scenario: Last Modifier Remove hides section
- **WHEN** the Object has one SkeletonModifier and the author Removes that row
- **THEN** the Skeleton Modifiers section is not shown

### Requirement: Add… catalog stays complete
Present-only Inspector sections SHALL NOT hide Unique attachment rows from the Add… picker. When a Unique attachment is already present, its Add… row SHALL stay visible and disabled, as in the first Add… slice.

#### Scenario: Skeleton present still listed in Add…
- **WHEN** the selected Object has a Skeleton and the author opens Add…
- **THEN** the Skeleton row is visible and cannot be chosen
