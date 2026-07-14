## ADDED Requirements

### Requirement: Vendored Godot editor icon pack

The repository SHALL contain Godot Engine `editor/icons` SVG files under `engine/3rdparty/godot-icons/`, accompanied by a short LICENSE (MIT) and README stating upstream source and that only Slint-referenced SVGs are embedded into the editor binary.

#### Scenario: Pack present without local Godot checkout

- **WHEN** a developer builds the editor without `E:\Dev\godot` on disk
- **THEN** all Editor Icons referenced by Slint SHALL resolve from `engine/3rdparty/godot-icons/`
- **AND** the build SHALL NOT require a path outside the Blunder repository for those glyphs

#### Scenario: Unreferenced icons stay on disk only

- **WHEN** the full Godot `editor/icons` set is vendored
- **AND** only a subset of filenames appear in Slint `@image-url` references
- **THEN** the editor binary SHALL embed that referenced subset
- **AND** SHALL NOT be required to embed every file in the vendored directory

### Requirement: Colorized Slint Editor Icon components

The editor SHALL provide reusable Slint components for Editor Icons that load Godot SVGs via `@image-url`, expose `icon-color` and `icon-size` (or equivalent), and apply Slint `Image.colorize` from `icon-color`. Components SHALL NOT use emoji or Unicode text as the glyph source for these affordances.

#### Scenario: Hover recolors icon

- **WHEN** a parent `TouchArea` binds a brighter `icon-color` on hover
- **THEN** the Editor Icon component SHALL display the SVG tinted to that color

### Requirement: Content Browser uses Editor Icons

Content Browser search, refresh, folder (tree and directory grid tiles), tree expand/collapse arrows, and breadcrumb separators SHALL use Editor Icon components (Godot SVGs) instead of emoji or Unicode symbol `Text` glyphs. Hit targets and callbacks SHALL remain unchanged.

#### Scenario: Folder row shows Folder SVG

- **WHEN** the Content Browser tree lists a folder row
- **THEN** the row SHALL show the Godot `Folder` Editor Icon
- **AND** it SHALL NOT use the folder emoji as the glyph source

#### Scenario: Tree expand uses Godot tree arrows

- **WHEN** a tree row has children and is collapsed
- **THEN** the expand control SHALL show `GuiTreeArrowRight` (or the project’s chosen collapsed arrow SVG)
- **WHEN** that row is expanded
- **THEN** the control SHALL show `GuiTreeArrowDown` (or the chosen expanded arrow SVG)

#### Scenario: Breadcrumb separator is an arrow icon

- **WHEN** the path breadcrumb shows more than one segment
- **THEN** separators between segments SHALL use an Editor Icon arrow SVG
- **AND** SHALL NOT use the Unicode `›` character as the separator glyph

### Requirement: Inspector scale-link and section chevrons use Editor Icons

The Inspector Scale link toggle SHALL use Godot `Instance` when scale link is enabled and `Unlinked` when disabled. The Transform section expand/collapse chevron SHALL use the same tree-arrow Editor Icons as the Content Browser tree. Behavior of scale linking and section expand SHALL remain unchanged.

#### Scenario: Scale link shows Instance when enabled

- **WHEN** Scale link is enabled
- **THEN** the toggle SHALL display the `Instance` Editor Icon
- **AND** SHALL NOT use the chain-link emoji as the glyph source

#### Scenario: Scale link shows Unlinked when disabled

- **WHEN** Scale link is disabled
- **THEN** the toggle SHALL display the `Unlinked` Editor Icon
