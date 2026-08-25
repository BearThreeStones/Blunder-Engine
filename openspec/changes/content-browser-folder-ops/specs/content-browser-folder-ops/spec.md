## Purpose

Lets authors create, rename, delete, and reparent Browser Folders and rename Assets on the Content Browser Assets tree, with undo on Global History.

## ADDED Requirements

### Requirement: Browser folder context
New Folder and New Scene SHALL share **Browser folder context**. Toolbar and grid empty-area SHALL use the currently open Browser Folder. A folder row’s right-click menu on the grid or tree SHALL use that folder as the parent (create inside it) and SHALL NOT navigate the grid solely because New was chosen.

#### Scenario: Toolbar New Folder uses the open folder
- **WHEN** the grid is showing `assets/Chars/` and the author clicks toolbar **+ New Folder**
- **THEN** the new directory is created under `assets/Chars/`

#### Scenario: Right-click folder creates inside it
- **WHEN** the grid is showing `assets/` and the author right-clicks folder `Chars` and chooses New Folder
- **THEN** the new directory is created under `assets/Chars/`
- **AND** the grid remains on `assets/`

### Requirement: New Folder
The Content Browser SHALL create a new empty Browser Folder under the Browser folder context. The first on-disk name SHALL be `New Folder`, with `_1`, `_2`, … on collision. Inline Rename SHALL start immediately. Creating the directory SHALL seal one Global Command. A later Inline Rename commit that changes the name SHALL seal a second Global Command. Cancelling Inline Rename SHALL leave the auto-name and only the create command. The Assets root MAY receive New Folder. Resources and Source trees SHALL NOT receive New Folder. Primary entry: toolbar **+ New Folder**, plus folder/empty-area right-click on grid and tree.

#### Scenario: Unique auto-name then rename
- **WHEN** `assets/` has no `New Folder` and the author invokes New Folder there
- **THEN** `assets/New Folder/` exists
- **AND** Inline Rename is active on that folder

#### Scenario: Collision suffix
- **WHEN** `assets/New Folder/` already exists and the author invokes New Folder in `assets/`
- **THEN** the new directory is `assets/New Folder_1/` (or the next free `_N`)

#### Scenario: Cancel rename keeps auto-name
- **WHEN** New Folder created `assets/New Folder/` and the author cancels Inline Rename
- **THEN** `assets/New Folder/` still exists
- **AND** Global History has the create command only

### Requirement: Inline Rename
The Content Browser SHALL provide Inline Rename for a single selected Browser Folder or Asset. Entry points: F2, context-menu **Rename**, and a click on the name of an already selected entry. Inline Rename SHALL NOT start when more than one grid entry is selected. Folders SHALL edit the directory name. Assets SHALL edit the filename stem only; the typed suffix SHALL be preserved and SHALL NOT be in the edit buffer. The Assets root SHALL NOT be renamed.

#### Scenario: Asset stem preserves suffix
- **WHEN** the author Inline-Renames `Hero.mesh.yaml` to stem `Villain` and commits
- **THEN** the descriptor path ends with `Villain.mesh.yaml`
- **AND** the GUID is unchanged

#### Scenario: Multi-select does not rename
- **WHEN** two grid entries are selected and the author presses F2
- **THEN** Inline Rename does not start

### Requirement: Browser entry name legality
A Browser entry name SHALL be the directory name of a Browser Folder or the filename stem of an Asset descriptor. Sibling names SHALL be unique. After trimming leading and trailing whitespace, a name SHALL be illegal when it is empty, is `.` or `..`, contains `\ / : * ? " < > |`, is a Windows reserved device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`, case-insensitive), or ends with a space or `.`. Inline Rename commit that collides or is illegal SHALL be refused and SHALL stay in edit without writing disk. The engine SHALL NOT sanitize illegal characters into a different name. Rename SHALL NOT auto-append `_1` on collision.

#### Scenario: Illegal character refused
- **WHEN** the author commits Inline Rename with name `Hero:2`
- **THEN** the on-disk name is unchanged
- **AND** Inline Rename remains active

#### Scenario: Sibling collision refused
- **WHEN** `Chars` already exists as a sibling and the author commits Rename Folder to `Chars`
- **THEN** the source folder name is unchanged
- **AND** Inline Rename remains active

### Requirement: Asset rename stays on Assets descriptors
Asset rename SHALL change only the descriptor filename stem under the Assets root, keeping parent folder, typed suffix, and GUID. Intermediate `source` and Source `archived_source` paths and files SHALL NOT be renamed. Cross-folder moves SHALL be Browser reparent, not Asset rename.

#### Scenario: Intermediate path unchanged
- **WHEN** `assets/Chars/Hero.mesh.yaml` has `source: resources/Models/Hero/Hero.gltf` and the author renames the stem to `Villain`
- **THEN** the descriptor is `assets/Chars/Villain.mesh.yaml`
- **AND** `resources/Models/Hero/Hero.gltf` still exists at that path
- **AND** the descriptor `source` field still refers to that Intermediate path

### Requirement: Delete Folder
Delete Folder SHALL remove a Browser Folder and every Asset whose descriptor lives under it as one Global Command. The command SHALL succeed only if no Asset in that set has a non-Scene dependent outside the set. Scene references outside the set SHALL be detached as with Asset Delete. Empty Browser Folders SHALL delete with no extra confirm. A folder that contains at least one Asset SHALL ask for confirm (folder name + Asset count) before the command runs. The Assets root SHALL NOT be deleted. Multi-select Delete SHALL union selected Assets and Browser Folders into one set and one Global Command (all-or-nothing, one confirm with the Asset count). Primary entry: folder context-menu **Delete** (grid and tree) and the Delete key when a Browser Folder is selected. AnimationClip Assets whose descriptors are not in the set SHALL NOT be cascaded when a Mesh in the set is deleted.

#### Scenario: Empty folder deletes without confirm
- **WHEN** the author selects empty `assets/Tmp/` and presses Delete
- **THEN** `assets/Tmp/` is gone with no confirm dialog
- **AND** one Global Command is on Global History

#### Scenario: External texture dependent refuses the folder
- **WHEN** folder `assets/Chars/` contains Texture `HeroAlbedo` and Mesh `Other` outside the folder references that Texture
- **AND** the author confirms Delete Folder on `Chars`
- **THEN** the folder and its Assets are unchanged
- **AND** the author is informed which dependents block delete

#### Scenario: Scene dependents are detached then folder deletes
- **WHEN** folder `assets/Chars/` contains Mesh `Hero` referenced only by Scene Assets
- **AND** no non-Scene Asset outside the folder depends on GUIDs in the set
- **AND** the author confirms Delete Folder
- **THEN** those Scene references are cleared
- **AND** the folder and its descriptors are gone

#### Scenario: Multi-select unions folders and assets
- **WHEN** the author selects folder `assets/A/` and Asset `assets/B.mesh.yaml` and deletes
- **THEN** one Global Command removes both (subject to the same all-or-nothing dependent rule)

### Requirement: Browser reparent
Content Browser drag SHALL reparent an Asset or Browser Folder onto a Browser Folder target. The Assets root SHALL NOT be a drag source. Drop onto a file, into self or a descendant, or onto a colliding sibling name SHALL be refused (not-allowed cursor; no silent `_1`). Drop onto the current parent SHALL be a no-op success. Folder drop onto the viewport SHALL NOT spawn or open. Reparent SHALL be one Global Command: on-disk move under Assets, registry paths rewritten for every moved descriptor, GUIDs unchanged. Intermediate and Source files SHALL NOT move. Reparent SHALL NOT start from a multi-selection.

#### Scenario: Folder move updates nested descriptor registry paths
- **WHEN** the author drags `assets/Chars/` onto `assets/Enemies/` and the drop is legal
- **THEN** descriptors that lived under `assets/Chars/` now live under `assets/Enemies/Chars/`
- **AND** each GUID still resolves
- **AND** Intermediate `source` files are unmoved

#### Scenario: Drop into descendant refused
- **WHEN** the author drags `assets/Chars/` onto `assets/Chars/Hero/`
- **THEN** no files move

### Requirement: Open Scene follow
When Rename Folder, Browser reparent, or Asset rename changes only the path of the open Scene Asset, the editor SHALL keep that document open at the new path. GUID and Document History SHALL stay. The path change SHALL NOT by itself mark the scene dirty. Deleting that Scene Asset (alone or via Delete Folder) SHALL use the existing dirty prompt if needed, then close the document.

#### Scenario: Rename parent folder keeps the scene open
- **WHEN** the open scene is `assets/Levels/Arena.scene.asset` and the author renames `Levels` to `Maps`
- **THEN** the open document path is `assets/Maps/Arena.scene.asset`
- **AND** Document History can still undo scene edits from before the rename

#### Scenario: Delete open scene closes the document
- **WHEN** the open scene is dirty and Delete Folder would remove that Scene Asset and the author confirms the dirty prompt
- **THEN** the Scene Asset is deleted
- **AND** the editor is not left editing a missing file

### Requirement: Browser mutations are Global Commands
New Folder, Inline Rename commit, Delete Folder, Browser reparent, Asset rename, and Asset Delete SHALL record as Editor Commands on Global History, not Document History. Opening another scene SHALL NOT clear Global History. Undo/redo of a Browser mutation SHALL restore or re-apply both filesystem and registry effects of that command. History Panel Global scope SHALL list these commands with English labels. When Scene and Global filters are both on, the panel SHALL list each stack as its own group with no interleaved merge.

#### Scenario: Undo New Folder removes the directory
- **WHEN** the author creates a folder and undoes that Global Command (Content Browser focused)
- **THEN** the directory is gone
- **AND** Document History is unchanged

#### Scenario: Scene open does not drop folder undo
- **WHEN** the author created a folder and then opens another Scene Asset
- **THEN** Global History can still undo the folder create

### Requirement: Delete scene detach rides the Global Command
Clearing Scene Asset references to GUIDs removed by Asset Delete or Delete Folder SHALL be payload of that same Global Command, including on-disk Scene Assets and the live SceneInstance when the open scene is affected. It SHALL NOT push a Document Command. An affected open scene SHALL become dirty. Undo of that Global Command SHALL restore the references.

#### Scenario: Viewport undo does not restore a deleted mesh reference
- **WHEN** Delete Folder detached a Mesh from the open scene and the author then focuses the viewport and presses Ctrl+Z
- **THEN** Document History undoes the last scene edit if any
- **AND** the detached Mesh reference is not restored unless Global History is undone

#### Scenario: Global undo restores detached references
- **WHEN** the author undoes the Delete Folder Global Command
- **THEN** removed descriptors are restored
- **AND** previously detached Scene references are restored on disk and on the live instance if that scene is open
