## Purpose

A Slint control kit whose inventory follows Unity Foundations but whose look is the Editor Theme, used for Shell chrome and Inspector Foldout/Add…/Remove — not for Inspector property fields.

## ADDED Requirements

### Requirement: Foundations inventory at Editor Theme metrics
The editor SHALL provide reusable controls covering Button, ToolbarButton, Tab, Foldout, Text Field, Numeric Field, Search Field, Toggle, Slider, Color Field, Object Field, List View, and Tree View, including default, hover, pressed, focus, checked/selected, and disabled states. Default type size SHALL be Inter 12px. Single-line controls SHALL be 22px tall (large 26px). These controls SHALL use Editor Theme colors, hairlines, 6px radius, ghost-then-fill hover, and **Editor accent** for primary, checked, and focus.

#### Scenario: Primary button uses accent
- **WHEN** a confirming dialog action is shown (for example Save on a dirty Play modal)
- **THEN** that action uses the accent primary Button and sibling actions do not

#### Scenario: Focus ring uses accent
- **WHEN** a Text Field has keyboard focus
- **THEN** its focus treatment uses the accent token (ring or border) rather than an unrelated color

#### Scenario: Disabled is visibly muted
- **WHEN** an Editor controls Button is disabled
- **THEN** it does not accept activation and is visually muted relative to the default state

### Requirement: Pill dock tabs
Dock tabs SHALL be pills: idle tabs have no fill; the active tab is filled with the tab-active Window-family color on Application Bar ground. Tab drag, click-to-activate, and float-move gestures SHALL NOT change.

#### Scenario: Active tab is filled, idle is not
- **WHEN** a container shows one active tab and one idle tab
- **THEN** the active tab has a filled pill background and the idle tab does not

### Requirement: Inspector property fields are not Editor controls Numeric Fields
Camera FOV / Near / Far, Light scalars and toggles, Behaviour bag bool/number/string, SkeletonModifier fields, and AxisNumberField SHALL remain compact Godot-style labeled cells (22px cell rhythm) and SHALL NOT be replaced by Editor controls Numeric Field / Toggle / Color Field.

#### Scenario: FOV stays a compact cell
- **WHEN** a selected entity has a Camera Component
- **THEN** FOV is a compact labeled cell, not an 22px Editor controls Numeric Field with inset chrome

### Requirement: std-widgets are not the product look
Editor Session chrome, Project Manager chrome, and authored Editor modals SHALL NOT present `std-widgets` Button / LineEdit / CheckBox / Slider as the visible product control. Animation Tree Canvas MAY keep local chrome until a follow-up.

#### Scenario: Application Bar buttons are Editor controls
- **WHEN** the Application Bar shows Save
- **THEN** Save is an Editor controls ghost Button, not a `std-widgets` Button
