# browser-view-layout Specification

## Purpose
Lets authors switch the Content Browser between thumbnail icon sizes and a columnar Details table, using one session-wide View Layout shared by the status-bar slider and menu.
## Requirements
### Requirement: Browser View Layout is one session-wide state

The Content Browser SHALL have a single session-wide **Browser View Layout**. The status-bar slider and the View Layout menu SHALL write that same state. Layout SHALL NOT persist across Editor Sessions and SHALL NOT be remembered per folder. Changing the selected folder SHALL keep the current layout.

#### Scenario: Menu and slider stay in sync

- **WHEN** the user chooses Medium Icons from the View Layout menu
- **THEN** the slider SHALL sit at the Medium Icons size
- **AND** the folder contents SHALL show as a thumbnail grid at that size

#### Scenario: Layout survives folder change

- **WHEN** the Browser is in Details
- **AND** the user selects a different folder in the tree
- **THEN** the Browser SHALL remain in Details

#### Scenario: Layout resets on a new Editor Session

- **WHEN** the editor starts a new Editor Session
- **THEN** Browser View Layout SHALL NOT restore a previous session’s Details or icon size from disk

### Requirement: Slider minimum is Details

The status-bar size slider SHALL treat its minimum as Details. Dragging the slider right from Details SHALL enter Small Icons. In icon layouts the slider SHALL scale thumbnail size continuously between Small Icons and Extra Large Icons.

#### Scenario: Slider at minimum shows Details

- **WHEN** the user drags the slider to its minimum
- **THEN** the Browser SHALL show Details
- **AND** SHALL NOT show a thumbnail grid

#### Scenario: Dragging right from Details enters Small Icons

- **WHEN** the Browser is in Details
- **AND** the user drags the slider right off the minimum
- **THEN** the Browser SHALL show Small Icons

### Requirement: Named icon sizes

Icon layouts SHALL be Extra Large Icons (128px), Large Icons (104px), Medium Icons (80px), and Small Icons (48px), measured as the Content Browser Thumbnail edge length. The View Layout menu SHALL jump to those exact sizes. While in an icon layout, the menu SHALL check the nearest of those four sizes (ties prefer the larger size).

#### Scenario: Extra Large Icons menu item

- **WHEN** the user chooses Extra Large Icons from the View Layout menu
- **THEN** entries SHALL show as a thumbnail grid with 128px thumbnails
- **AND** Extra Large Icons SHALL be checked in the menu

#### Scenario: In-between slider checks nearest size

- **WHEN** the slider is at 64px (icon layout)
- **THEN** the View Layout menu SHALL check Small Icons or Medium Icons according to nearest size, preferring Medium Icons on a tie

### Requirement: View Layout menu lives on the status bar

The Content Browser SHALL expose a View Layout control on the status bar immediately left of the size slider. Choosing an item SHALL set Browser View Layout. Item context menus and empty-area context menus SHALL NOT gain a View submenu in this change. Menu labels SHALL be English: Extra Large Icons, Large Icons, Medium Icons, Small Icons, Details.

#### Scenario: Status-bar menu sets Details

- **WHEN** the user opens the status-bar View Layout menu and chooses Details
- **THEN** the Browser SHALL show Details
- **AND** the slider SHALL sit at its minimum
- **AND** Details SHALL be checked in the menu

#### Scenario: Asset context menu has no View submenu

- **WHEN** the user right-clicks a grid or Details entry
- **THEN** that menu SHALL NOT list View Layout items

### Requirement: Icon layouts show Content Browser Thumbnails

Extra Large, Large, Medium, and Small Icons SHALL show **Content Browser Thumbnails** for file entries and the Folder Editor Icon for directories. Details SHALL NOT use a Content Browser Thumbnail as the Name-column leading graphic.

#### Scenario: Mesh in Medium Icons shows a thumbnail

- **WHEN** the layout is Medium Icons
- **AND** the folder contains a Mesh Asset
- **THEN** that entry SHALL show its Content Browser Thumbnail in the grid

### Requirement: Details is a fixed-width columnar table

Details SHALL list each entry as one row with columns Name, Type, Size, and Date modified, in that order. Name SHALL take remaining width. Type, Size, and Date modified SHALL use fixed widths. Columns SHALL NOT be user-resizable. Size SHALL be the on-disk size of the Assets-root file for that entry. Date modified SHALL be that file’s last write time. Folder rows SHALL leave Size and Date modified empty.

#### Scenario: Mesh descriptor size is the Assets-root file

- **WHEN** Details shows `assets/Meshes/Sponza.mesh.yaml`
- **THEN** Size SHALL be the size of that descriptor file
- **AND** SHALL NOT be required to equal the Intermediate glTF body size under Resources

#### Scenario: Folder has blank Size and Date

- **WHEN** Details shows a folder row
- **THEN** Size and Date modified SHALL be empty
- **AND** Type SHALL be Folder

### Requirement: Details Type is the product type

Type SHALL be one of: Folder, Mesh, Scene, Texture, AnimationClip, File. Classification SHALL use directory vs descriptor suffix (Mesh: `.mesh.yaml` / `.mesh.asset`; Scene: `.scene.asset`; Texture: `.texture.yaml`; AnimationClip: `.animation.yaml`). Any other file SHALL be File.

#### Scenario: README is File

- **WHEN** Details lists `assets/README.md`
- **THEN** Type SHALL be File

#### Scenario: Scene descriptor is Scene

- **WHEN** Details lists a `.scene.asset` entry
- **THEN** Type SHALL be Scene

### Requirement: Details Name uses type-keyed Editor Icons

The Details Name column SHALL show a type-keyed **Editor Icon** (Folder, Mesh, Scene, Texture, AnimationClip, File) plus the display name. It SHALL NOT show a Content Browser Thumbnail.

#### Scenario: Mesh row uses Mesh Editor Icon

- **WHEN** Details lists a Mesh Asset
- **THEN** the Name column SHALL show the Mesh Editor Icon
- **AND** SHALL NOT show that Asset’s Content Browser Thumbnail

### Requirement: Details headers sort with folders first

Clicking a Details column header SHALL sort by that column. Clicking the same header again SHALL reverse the sort. Every sort SHALL keep all folders before all files. Default sort SHALL be Name ascending with folders first. The same entry order SHALL apply to icon layouts until the user sorts again.

#### Scenario: Sort by Size keeps folders first

- **WHEN** the user clicks the Size header
- **THEN** all folders SHALL appear before all files
- **AND** files SHALL be ordered by Size

#### Scenario: Second click reverses Size

- **WHEN** the list is sorted by Size ascending (folders first)
- **AND** the user clicks Size again
- **THEN** folders SHALL still appear first
- **AND** files SHALL be ordered by Size descending

### Requirement: Details keeps existing entry interactions

Select, multi-select, Content Browser drag, context menus, folder navigation, search filtering, Import, New Scene, and Delete SHALL work on Details rows as they do on icon-grid tiles.

#### Scenario: Drag a Mesh from Details

- **WHEN** the Browser is in Details
- **AND** the user drags a Mesh Asset onto the editor viewport
- **THEN** Content Browser drag SHALL proceed as in an icon layout

### Requirement: List Tiles and Content layouts are out of product

The Browser SHALL NOT offer Explorer List, Tiles, or Content layouts in this change.

#### Scenario: View Layout menu has five items

- **WHEN** the user opens the View Layout menu
- **THEN** it SHALL list Extra Large Icons, Large Icons, Medium Icons, Small Icons, and Details
- **AND** SHALL NOT list List, Tiles, or Content

