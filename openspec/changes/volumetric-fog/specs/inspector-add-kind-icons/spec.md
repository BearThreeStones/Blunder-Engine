## MODIFIED Requirements

### Requirement: One icon per Add… kind
The Inspector SHALL show one icon per Add… kind: Camera, Light, Fog, Skeleton, AnimationPlayer, AnimationTree, Behaviour, and SkeletonModifier. All Behaviour declarations SHALL share the Behaviour icon. All SkeletonModifiers SHALL share the SkeletonModifier icon. Clip rows, Local Transform, Mesh, Shading, and Hierarchy entities SHALL NOT gain an Add… kind icon.

#### Scenario: Two Behaviours share one kind icon
- **WHEN** the selection has two Behaviour declarations of different CLR types
- **THEN** both rows show the same Behaviour kind icon

#### Scenario: Clip row has no kind icon
- **WHEN** an AnimationPlayer section shows a clip binding row
- **THEN** that row has no Add… kind icon

#### Scenario: Fog header shows the Fog icon
- **WHEN** the selection has a Fog Component and the Fog section is visible
- **THEN** the Fog kind icon sits between the expand arrow and the word Fog
