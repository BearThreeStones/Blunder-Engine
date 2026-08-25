## MODIFIED Requirements

### Requirement: First-slice menu is a flat three-item list
The Create… items SHALL remain a flat English list of `Empty`, `Camera`, and `Light`. The menu SHALL NOT use a nested Create submenu. On a visible entity row that same menu SHALL continue with a separator then Delete (see **hierarchy-delete**). Empty-area and scene-title menus SHALL keep only the three Create items. This slice SHALL NOT include Duplicate, Rename, Mesh, Skeleton, AnimationPlayer, or AnimationTree.

#### Scenario: Menu shows three items
- **WHEN** the author opens Create… on a Hierarchy row
- **THEN** the menu lists Empty, Camera, and Light, then a separator and Delete, and does not list Duplicate or a Create submenu

#### Scenario: Empty-area menu stays Create-only
- **WHEN** the author right-clicks empty Hierarchy area
- **THEN** the menu lists Empty, Camera, and Light only
