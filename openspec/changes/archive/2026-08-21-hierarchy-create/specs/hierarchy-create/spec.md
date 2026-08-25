## Purpose

Lets authors spawn a new Hierarchy child (or Scene Tree root) with Empty, Camera, or Light from a Hierarchy Panel right-click, without using Inspector Add… on the clicked row.

## ADDED Requirements

### Requirement: Create… spawns a new entity
Hierarchy **Create…** SHALL spawn a new entity. It SHALL NOT attach Camera or Light to the right-clicked row. Mesh spawn SHALL remain Content Browser spawn. Skeleton, AnimationPlayer, and AnimationTree SHALL remain Inspector **Add…**.

#### Scenario: Create Light does not attach to the clicked row
- **WHEN** the author right-clicks an entity that has no Light and chooses Light
- **THEN** that entity still has no Light, and a new child entity has the Light Component

### Requirement: Parent is the right-clicked row
When the menu is invoked on a visible entity row, the new entity SHALL be a child of that row.

#### Scenario: Child of the clicked row
- **WHEN** the author right-clicks entity A and chooses Empty
- **THEN** the new entity’s parent is A

### Requirement: Empty area, scene title, and empty scene create a root
When the menu is invoked on empty Hierarchy area, the scene title chrome, or an empty scene, the new entity SHALL be a Scene Tree root (no parent). The scene title SHALL NOT become a tree parent.

#### Scenario: Empty area creates a root
- **WHEN** the author right-clicks empty Hierarchy area (not on an entity row) and chooses Camera
- **THEN** the new Camera entity has no parent

#### Scenario: Scene title creates a root
- **WHEN** the author right-clicks the scene display name chrome and chooses Light
- **THEN** the new Light entity has no parent and the scene title is not its parent

#### Scenario: Empty scene still Create…
- **WHEN** the active scene has no entities and the author Create… Empty from the Hierarchy Panel
- **THEN** a root entity named `Empty` exists

### Requirement: First-slice menu is a flat three-item list
The Create… menu SHALL be a flat list of English items `Empty`, `Camera`, and `Light`. It SHALL NOT use a nested Create submenu. It SHALL NOT include Duplicate, Rename, Delete, Mesh, Skeleton, AnimationPlayer, or AnimationTree in this slice.

#### Scenario: Menu shows three items
- **WHEN** the author opens Create… on a Hierarchy row
- **THEN** the menu lists Empty, Camera, and Light and does not list Duplicate or a Create submenu

### Requirement: Create… Light is one row with Directional default
Create… Light SHALL create one Light Component whose type is **Directional Light**. Type remains a field on that Unique attachment.

#### Scenario: Create Light is Directional
- **WHEN** the author chooses Light from Create…
- **THEN** the new entity has a Light Component of type Directional Light

### Requirement: Default names and collision suffix
Default entity names SHALL be `Empty`, `Camera`, and `Light`. If that name is already used in the scene, the editor SHALL append `_1`, `_2`, … until unique. Create… SHALL NOT open a naming dialog. Create… Camera SHALL NOT be named `Main Camera`. Create… Light SHALL NOT be named `Directional Light`.

#### Scenario: Second Light is Light_1
- **WHEN** the scene already has an entity named `Light` and the author Create… Light
- **THEN** the new entity is named `Light_1`

### Requirement: Identity local TRS
The new entity’s local position SHALL be the origin, local rotation identity, and local scale 1. Create… Light SHALL NOT copy the New Scene Directional world pose.

#### Scenario: Child Light sits at the parent origin
- **WHEN** the author Create… Light on a row whose world pose is not identity
- **THEN** the new Light entity’s local TRS is identity

### Requirement: Create… Camera is not Main Camera
Create… Camera SHALL set `isMain` false. It SHALL NOT steal **Main Camera** from an existing Main.

#### Scenario: New Scene Main stays Main
- **WHEN** a New Scene with Main Camera is open and the author Create… Camera
- **THEN** the original Main Camera remains Main and the new Camera is not Main

### Requirement: Selection after Create…
After a successful Create…, the new entity SHALL be the selection (single). If the parent row was collapsed, it SHALL expand so the new row is visible.

#### Scenario: Inspector follows the new Camera
- **WHEN** the author Create… Camera
- **THEN** the new Camera entity is selected

### Requirement: Right-click selects the row then opens the menu
Right-click anywhere on a visible row (name, Hierarchy Line gutter, expand chevron) SHALL select that entity as a single selection and then open the menu. Canceling the menu SHALL leave that selection. Empty area and scene title SHALL NOT change selection until a Create… item runs. Right-click SHALL NOT keep a prior multi-select. Create… SHALL NOT spawn one child under each previously selected entity.

#### Scenario: Right-click replaces multi-select
- **WHEN** two entities are selected and the author right-clicks a third row
- **THEN** only that third entity is selected and the Create… menu is open

#### Scenario: Cancel menu keeps the row
- **WHEN** the author right-clicks a row and dismisses the menu without choosing an item
- **THEN** that row remains the selection

### Requirement: Host is Hierarchy Panel only
Create… SHALL be available on the Hierarchy Panel when docked and when floating. This slice SHALL NOT add Create… to the viewport or the editor top bar.

#### Scenario: Floating Hierarchy Create…
- **WHEN** Hierarchy is in a floating window and the author Create… Empty on a row
- **THEN** a new child entity is spawned the same as from the docked Hierarchy Panel
