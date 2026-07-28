## ADDED Requirements

### Requirement: Camera Gizmo draw

In Edit Mode with Editor Overlays enabled, the editor SHALL draw a **Camera Gizmo** for every entity that has a **Camera Component**. The gizmo SHALL use Blender-like wire geometry: origin point, four frustum edges, a **view frame** rectangle, and an **up triangle** on the top edge of the frame. Frame aspect SHALL follow the current editor viewport aspect. Frame depth SHALL be a fixed local display distance. Unselected cameras SHALL use a muted color; a single selected camera SHALL use the selection color.

#### Scenario: All cameras visible

- **WHEN** the active scene has two entities with Camera Components and none is selected
- **THEN** both Camera Gizmos are drawn in the muted style

#### Scenario: Selected camera uses selection color

- **WHEN** exactly one Camera entity is selected
- **THEN** that Camera Gizmo uses the selection color

### Requirement: FOV and clip handles on single selection

When exactly one entity with a Camera Component is selected, the Camera Gizmo SHALL expose interaction handles to edit vertical FOV and near/far clip. Multi-select SHALL NOT show those handles. Handle drags SHALL update the Camera Component live and SHALL seal a Document History Command on release (not per-move).

#### Scenario: Single selection shows handles

- **WHEN** exactly one Camera entity is selected
- **THEN** FOV and clip handles are available on its Camera Gizmo

#### Scenario: Multi-select hides handles

- **WHEN** more than one entity is selected
- **THEN** FOV and clip handles are not shown on Camera Gizmos

### Requirement: Camera Gizmo pick priority

Pointer hits on a Camera Gizmo (body, frame, or handles) SHALL be handled before mesh viewport pick.

#### Scenario: Click camera over mesh

- **WHEN** the pointer clicks a Camera Gizmo that overlaps a mesh in screen space
- **THEN** the Camera entity is selected (or a handle drag starts) and mesh pick does not claim the click

### Requirement: Align View to Camera

The editor SHALL provide **Align View to Camera**: move the Editor Camera to the target Camera Component's pose and vertical FOV (not near/far). Target is the single selected Camera entity when exactly one such selection exists; multi-select is invalid. With no selection, target is Main Camera if present, else the first valid Camera by stable EntityId order. With no Camera, the action fails. The action SHALL NOT push Document History.

#### Scenario: Align view with no selection uses Main then first

- **WHEN** no entity is selected and the scene has a Main Camera
- **THEN** Align View to Camera matches the Editor Camera to that Main Camera pose and FOV

### Requirement: Align Camera to View

The editor SHALL provide **Align Camera to View**: write the Editor Camera pose and vertical FOV into the target Camera Component (not near/far). Target rules match Align View to Camera. The action SHALL seal a Document History Command.

#### Scenario: Align camera writes history

- **WHEN** the author runs Align Camera to View on a valid target
- **THEN** the Camera entity pose and FOV update and Document History can undo the change

### Requirement: Align shortcuts and menu

Align View to Camera and Align Camera to View SHALL be reachable from an editor menu and from shortcuts: Numpad 0 and Ctrl+Alt+Numpad 0 respectively, plus laptop fallbacks that do not require a numpad.

#### Scenario: Menu without numpad

- **WHEN** the author invokes Align View to Camera from the menu
- **THEN** the action runs without requiring a numpad key
