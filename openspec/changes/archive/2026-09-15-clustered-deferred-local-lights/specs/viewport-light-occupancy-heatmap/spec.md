## Purpose

Give editor users a Viewport-only toggle overlay that visualizes how many lights landed in each froxel, so clustering behavior can be visually accepted or flagged as wrong before trusting the lit result.

## ADDED Requirements

### Requirement: Viewport toggle for the occupancy heatmap overlay
The editor Viewport SHALL provide a toggle that enables or disables an occupancy heatmap overlay. The overlay SHALL be off by default and SHALL NOT persist as enabled across a fresh editor launch.

#### Scenario: Toggle enables the overlay
- **WHEN** the author enables the occupancy heatmap toggle in the Viewport
- **THEN** the heatmap overlay draws over the Viewport

#### Scenario: Toggle disables the overlay
- **WHEN** the author disables the occupancy heatmap toggle after it was enabled
- **THEN** the Viewport returns to its normal clustered-shaded appearance with no heatmap draw

### Requirement: Heatmap colors each froxel's screen tile by assigned-light count
While enabled, the overlay SHALL color each froxel's screen-space tile region using that froxel's post-cap assigned-light count (from the froxel grid built for that frame), so tiles with more assigned lights are visually distinguishable from tiles with fewer.

#### Scenario: Denser froxel reads differently than sparser froxel
- **WHEN** the heatmap overlay is enabled and one froxel has more assigned lights than an adjacent froxel at the same screen tile depth range
- **THEN** the two froxels' screen tile regions are colored differently

#### Scenario: Heatmap reflects the same cap as shading
- **WHEN** a froxel has overflowed past the 64-light cap
- **THEN** the heatmap colors that froxel's tile using the capped count (64), not the pre-cap overlap count

### Requirement: Heatmap overlay is editor-only
The occupancy heatmap overlay SHALL draw only in the editor Viewport. It SHALL NOT draw in the Player, Camera Preview, Placement Preview, Mesh Preview, or Scene Thumbnail render paths, regardless of toggle state.

#### Scenario: Player never shows the heatmap
- **WHEN** the occupancy heatmap toggle is enabled in the editor and the scene is run as Player
- **THEN** the Player's rendered frame has no heatmap overlay
