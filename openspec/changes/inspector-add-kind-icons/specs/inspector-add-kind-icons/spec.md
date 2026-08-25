## Purpose

Inspector and Add… picker mark each Add… kind with one editor icon so authors can tell Camera, Skeleton, AnimationPlayer, AnimationTree, Behaviour, and SkeletonModifier apart without reading names.

## ADDED Requirements

### Requirement: One icon per Add… kind
The Inspector SHALL show one icon per Add… kind: Camera, Skeleton, AnimationPlayer, AnimationTree, Behaviour, and SkeletonModifier. All Behaviour declarations SHALL share the Behaviour icon. All SkeletonModifiers SHALL share the SkeletonModifier icon. Clip rows, Local Transform, Mesh, Shading, and Hierarchy entities SHALL NOT gain an Add… kind icon.

#### Scenario: Two Behaviours share one kind icon
- **WHEN** the selection has two Behaviour declarations of different CLR types
- **THEN** both rows show the same Behaviour kind icon

#### Scenario: Clip row has no kind icon
- **WHEN** an AnimationPlayer section shows a clip binding row
- **THEN** that row has no Add… kind icon

### Requirement: Icon placement
Unique attachment icons SHALL appear on the section header between the expand arrow and the title. Behaviour and SkeletonModifier icons SHALL appear on each row to the left of the type name, and SHALL NOT appear on the Behaviours or Skeleton Modifiers section titles. Add… picker kind rows SHALL show the same kind icon; group labels (Unique attachments, Behaviours, Skeleton Modifiers) SHALL NOT.

#### Scenario: Camera header shows the Camera icon
- **WHEN** the selection has a Camera and the Camera section is visible
- **THEN** the Camera kind icon sits between the expand arrow and the word Camera

#### Scenario: Add… Camera row shows the Camera icon
- **WHEN** the author opens Add… on one entity
- **THEN** the Camera picker row shows the Camera kind icon
- **AND** the Unique attachments group label has no kind icon

#### Scenario: Behaviours section title has no kind icon
- **WHEN** the selection has at least one Behaviour declaration
- **THEN** each Behaviour row shows the Behaviour kind icon
- **AND** the Behaviours section title does not

### Requirement: Icon color follows the label
The kind icon SHALL remain visible whenever its row or Unique header is shown. Its color SHALL match the adjacent label: the normal Inspector label color when enabled, the disabled Unique Add… label color when that Unique attachment is already present, and the missing-Behaviour label color when the declaration’s type is missing from the catalog. The Inspector SHALL NOT swap to a different icon asset for those states.

#### Scenario: Unique already present greys the Add… icon
- **WHEN** the selected entity has a Camera and the author opens Add…
- **THEN** the Camera picker row still shows the Camera kind icon
- **AND** that icon uses the same grey as the disabled Camera label

#### Scenario: Missing Behaviour tints the icon
- **WHEN** a Behaviour declaration’s type is missing from the catalog
- **THEN** that row still shows the Behaviour kind icon
- **AND** that icon uses the same color as the missing type-name label
