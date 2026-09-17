## MODIFIED Requirements

### Requirement: Hierarchy row icon set
Each visible Hierarchy entity row SHALL show icons at the far right for: Local Transform (always); MeshRenderer when that entity has a MeshRenderer; each present Unique attachment among Camera, Light, Fog, Skeleton, and AnimationTree; each Behaviour in list order; each SkeletonModifier in list order. Unique, Behaviour, and SkeletonModifier icons SHALL reuse **Add… kind icon** art. Local Transform and MeshRenderer MAY use row-only glyphs. The row SHALL NOT show Clip Binding icons. The row SHALL NOT show an AnimationPlayer icon.

#### Scenario: Empty entity still shows Transform
- **WHEN** a Hierarchy row is an entity with no MeshRenderer, Uniques, Behaviours, or SkeletonModifiers
- **THEN** the row still shows the Local Transform icon
- **AND** it shows no other Hierarchy row icons

#### Scenario: Present Uniques
- **WHEN** an entity has a Camera Component, a Light Component, a Fog Component, a Skeleton, and an AnimationTree
- **THEN** the row shows Transform plus Camera, Light, Fog, Skeleton, and AnimationTree icons

#### Scenario: No AnimationPlayer icon
- **WHEN** the bound Object still has an AnimationPlayer in the engine
- **THEN** the Hierarchy row does not show an AnimationPlayer icon

#### Scenario: No Clip Binding icons
- **WHEN** the entity’s AnimationTree clip map has one or more Clip Bindings
- **THEN** the Hierarchy row does not show Clip Binding icons
- **AND** the AnimationTree Unique icon is shown if the Tree Unique is present

#### Scenario: Behaviour order
- **WHEN** an entity has two Behaviours in Inspector list order A then B
- **THEN** the row shows two Behaviour kind icons in that same order

#### Scenario: MeshRenderer when present
- **WHEN** an entity has a MeshRenderer
- **THEN** the row shows a MeshRenderer icon in addition to Transform
