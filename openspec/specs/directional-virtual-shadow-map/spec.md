# directional-virtual-shadow-map Specification

## Purpose

Give the one shadowing Directional Light Unreal-style virtual shadow map clipmap pages (page table plus physical pool) filled by Blunder mesh shaders, without classic CSM cascades and without putting local lights into that virtual map.

## Requirements

### Requirement: Directional uses clipmap VSM pages, not CSM
The one shadowing Directional Light (first Light-enabled Directional whose contribution includes shadows, stable EntityId order) SHALL cast through a camera-centered clipmap virtual shadow map: 128×128-texel pages, a page table, and a physical page pool. The system SHALL NOT add a classic cascaded shadow-map cascade count or pack ortho cascades into an atlas as this slice's directional topology.

#### Scenario: Clipmap not cascade atlas
- **WHEN** Viewport or Player renders a shadowing Directional with mesh shaders available
- **THEN** that Directional's shadow is stored as clipmap pages in a physical pool addressed by a page table
- **AND** the pass does not allocate a CSM cascade atlas for that light

#### Scenario: One shadowing Directional
- **WHEN** two Light-enabled Directionals both have contribution Illuminate and shadows
- **THEN** only the earlier EntityId Directional fills VSM pages

### Requirement: Mesh shaders fill physical pages, not Nanite visbuffer
Opaque cooked meshlets SHALL rasterize depth-only into the marked physical pages of that clipmap using the mesh-shader shadow raster. The system SHALL NOT fill pages via a Nanite visbuffer or a visbuffer-emit copy.

#### Scenario: Meshlets write page depth
- **WHEN** a marked physical page overlaps an opaque meshlet caster
- **THEN** the mesh-shader depth-only path writes depth into that physical page

### Requirement: Local lights are not in VSM
Point Lights and Spot Lights SHALL NOT allocate VSM pages, clipmap levels, or entries in the directional page table this slice.

#### Scenario: Point does not consume directional pages
- **WHEN** the scene has a shadowing Directional and a shadowing Point Light
- **THEN** the Point Light does not occupy directional VSM physical pages

### Requirement: Page mark is not Froxel occupancy
Pages SHALL be marked from the view's GBuffer (or equivalent scene depth) receivers. The system SHALL NOT use Unreal `Froxel::` occupancy to mark VSM pages and SHALL NOT bind clustered deferred light lists to page mark.

#### Scenario: Clustered list is not the mark source
- **WHEN** a clustered froxel light list exists for the Viewport
- **THEN** directional page mark does not read that list to decide which pages to fill

### Requirement: Unmarked or overflowed pages do not crash shading
Sampling a clipmap UV whose page is unmarked or was dropped for physical-pool overflow SHALL treat the receiver as unshadowed for that tap. Overflow SHALL be logged.

#### Scenario: Missing page is lit
- **WHEN** a shaded pixel's clipmap page was not resident this frame
- **THEN** that pixel is not forced to full shadow from the missing page
- **AND** the process continues
