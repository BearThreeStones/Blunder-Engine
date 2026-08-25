# hierarchy-row-icons Specification

## Purpose

Shows far-right Hierarchy entity-row icons for Local Transform, MeshRenderer, Unique attachments, Behaviours, and SkeletonModifiers so authors can scan what is on an entity without opening Inspector.

## Requirements

### Requirement: Hierarchy row icon set
Each visible Hierarchy entity row SHALL show icons at the far right for: Local Transform (always); MeshRenderer when that entity has a MeshRenderer; each present Unique attachment among Camera, Light, Skeleton, and AnimationTree; each Behaviour in list order; each SkeletonModifier in list order. Unique, Behaviour, and SkeletonModifier icons SHALL reuse **Add… kind icon** art. Local Transform and MeshRenderer MAY use row-only glyphs. The row SHALL NOT show Clip Binding icons. The row SHALL NOT show an AnimationPlayer icon.

#### Scenario: Empty entity still shows Transform
- **WHEN** a Hierarchy row is an entity with no MeshRenderer, Uniques, Behaviours, or SkeletonModifiers
- **THEN** the row still shows the Local Transform icon
- **AND** it shows no other Hierarchy row icons

#### Scenario: Present Uniques
- **WHEN** an entity has a Camera Component, a Light Component, a Skeleton, and an AnimationTree
- **THEN** the row shows Transform plus Camera, Light, Skeleton, and AnimationTree icons

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

### Requirement: Icon placement and hosts
Hierarchy row icons SHALL sit at the right of the entity row, after the name. Docked and floating Hierarchy Panel hosts SHALL show the same icons for the same row. The scene title chrome SHALL NOT show Hierarchy row icons.

#### Scenario: Floating Hierarchy matches docked
- **WHEN** the same entity row is visible in docked Hierarchy and a floating Hierarchy window
- **THEN** both show the same ordered icon set

#### Scenario: Scene title has no icons
- **WHEN** the Hierarchy Panel shows the scene display name
- **THEN** that chrome has no Hierarchy row icons
