# Blunder Engine

Editor and runtime for a Z-up, glTF-aligned 3D engine with Blender-inspired viewport gizmos.

## Language

### Reflection & scripting

**Gameplay scripting language**:
C# is the sole first-class language for Project gameplay logic. The engine does not ship or productize other script languages (Lua, GDScript, etc.); the C-ABI bridge may still be used by non-product hosts.
_Avoid_: Multi-language official scripting, Lua/GDScript as co-equal product tracks, treating C# as DogWalk-only scaffolding

**.NET script host**:
The in-process CoreCLR host (`nethost` / hostfxr) that loads Project C# assemblies and owns Script Peers. It talks to the engine only through the C-ABI bridge. Assembly-Load-Context hot reload is a later phase on the same host, not part of the first host milestone.
_Avoid_: Mono as the product host, out-of-process `dotnet` IPC as the shipping model, bundling ALC hot reload into the first host slice, direct P/Invoke of C++ member layouts

**Engine API assembly**:
The generated managed library (working name `Blunder.Api`) produced from the API Blueprint. Project game assemblies reference it; for the .NET host MVP it ships beside the editor/runtime and is referenced by path from the Create `.csproj` template. Default target framework is `net10.0` (current .NET LTS). A NuGet package may be added later for distribution without changing the Blueprint → generator source of truth.
_Avoid_: Hand-written C# stubs as source of truth, copying generated binding sources into every Project, scraping C++ headers from C#, requiring NuGet for the first host slice, defaulting new Projects to net8/net9 as the product TFM

**ClassDB**:
The single runtime type database for engine-exposed classes, properties, and methods. It is the source of truth for both editor tooling (Inspector, serialization) and script bindings.
_Avoid_: Separate editor reflection vs script reflection, dual metadata stores, ad-hoc per-UI property maps

**C-ABI bridge**:
The stable, symbol-free C calling surface that exports ClassDB operations to other languages. Script runtimes talk only through this bridge, never via C++ class layouts or member pointers.
_Avoid_: Direct P/Invoke of C++ methods, exposing C++ member pointers to C#

**Object**:
The identity-layer citizen of ClassDB: naming, hierarchy, serialization entry, and the host for zero or more Behaviours (each with a Script Peer). Objects are not the bulk storage for hot simulation data.
_Avoid_: Making every ECS component an Object, Godot-style everything-is-Node as the only model, requiring exactly one Script Peer per Object

**ECS World**:
The data-oriented store of entities and components, plus the systems that query and update them (transform, rendering prep, physics, etc.).
_Avoid_: Script virtual dispatch inside ECS systems, storing GCHandles on components

**Entity (ECS)**:
An ID in an ECS World that owns a set of components. Distinct from today's `Entity` C++ class (TRS + name POD), which will be split/renamed as the dual track lands.
_Avoid_: Using "Entity" to mean a scriptable Object, conflating with Object

**Component (ECS)**:
A data blob (and optional native behavior) stored in the ECS World. Components may be described in ClassDB as data schemas for editor/serialization; they are not Script Peer hosts.
_Avoid_: Component-as-Object, per-component C# GCHandle

**Behaviour**:
A C# gameplay script instance attached to an Object (Unity-style). An Object may host zero or more Behaviours; each Behaviour is one Script Peer. Behaviours are not ECS Components and are not themselves Objects. An Object's Behaviours form an ordered list; duplicate types are allowed. A Behaviour reaches engine state through its host Object (generated bindings) and can query sibling Behaviours on that Object (`GetBehaviour` / `GetBehaviours`).
_Avoid_: MonoBehaviour-on-Entity as the identity model, treating Behaviour as an Object subclass in the Scene Tree, one-Behaviour-only per Object, type-as-sole-key forbidding duplicates, Behaviour inheriting Object, hostless static-only gameplay API as the default

**BehaviourId**:
The stable identity of one Behaviour instance on an Object for serialization, the C-ABI bridge, and Script Peer rebinding across hot reload. Distinct from ObjectId and EntityId; not a list index.
_Avoid_: Using list index as durable identity, conflating with ObjectId, recycling ids like dense EntityId without a generation scheme

**Script Peer**:
The managed twin of one Behaviour on an Object (C# instance + GCHandle). Bound weakly through the Object; torn down and rebound on script hot reload while the Object and its ECS entity remain. An Object may have multiple Script Peers (one per Behaviour).
_Avoid_: Strong C++ references to managed types, storing managed delegates as the sole identity of an Object, single-peer-only as the product rule, GCHandles on ECS components, non-C# peers as the product default

**Object–Entity binding**:
An Object may optionally reference one ECS Entity (`EntityId`). An ECS Entity may exist with no Object (simulation-only). An Object may exist with no Entity (identity/hierarchy-only).
_Avoid_: Mandatory 1:1 Object↔Entity, Entity-as-primary with Object bolted on only when scripted

**ObjectId**:
The stable identity of an Object for serialization, the C-ABI bridge, and Script Peer rebinding. Distinct from `EntityId`, which remains a recyclable ECS handle.
_Avoid_: Using dense EntityId as the cross-language identity, passing raw C++ Object pointers as the public identity

**Object property surface**:
The properties and methods ClassDB advertises on an Object (and subclasses). ECS component fields are reached through this surface via projected accessors (e.g. `position` reads/writes the Transform component), not by treating components as the primary script/editor API.
_Avoid_: Dual-exposed Object and component fields as competing sources of truth, Inspector editing components while scripts edit Object fields independently

**Lazy Entity materialization**:
Writing a spatial (or other ECS-backed) property on an Object that has no Entity spawns an ECS Entity with the needed component(s) and binds it to that Object. Destroying the Object despawns its Entity when present.
_Avoid_: Shadow TRS on Object separate from Transform, requiring a pre-existing Entity before any transform edit

**Lifecycle dispatch**:
Engine-owned callbacks such as Ready/Tick implemented by Behaviours. For each Object, the engine invokes each Behaviour's Script Peer in that Object's Behaviour list order (one call per Behaviour instance via PtrCall) — not a single call per Object and not per-instance native function pointers on the Object.
_Avoid_: Per-Object TickDelegate fields, one lifecycle call per Object with manual C# fan-out as the only path, global per-type batching that ignores per-Object list order, unmanaged C# doing its own full world walk as the only tick path

**Reflection kernel**:
The first deliverable of the reflection system: ClassDB, Clang-driven export for a narrow set of types, PtrCall plus a small Variant path, API Blueprint, Object/`ObjectId`, projected property accessors, and a C-ABI skeleton — without shipping a .NET host, ALC hot reload, multi-Behaviour storage, or a full ECS rewrite of `SceneInstance`. The kernel Object retains a single Script Peer slot; multiple Behaviours arrive with the .NET host MVP (ADR 0011).
_Avoid_: Treating C# hot reload, multi-Behaviour storage, or full ECS migration as part of the first reflection milestone

**.NET host MVP**:
The first script-host deliverable after the Reflection kernel: in-process CoreCLR, load one Project assembly, attach Behaviours to Objects, Ready/Tick per Behaviour list order — without ALC hot reload, without Behaviour scene serialization, and without Inspector Behaviour UX. DogWalk's first playable character slice is written as C# Behaviours on this host, not as a throwaway C++ controller to migrate later. In development, the editor invokes `dotnet build` on the Scripts root (manually or before Play) and loads the output assembly; shipping builds use a separate publish/cook path. File-watcher auto-build is out of this slice.
_Avoid_: Bundling hot reload, scene-persisted Behaviours, or full binding surface into the host MVP; starting DogWalk gameplay on a C++-only controller as the lasting path; shipping the host before the Reflection kernel contract is stable; requiring authors to build Scripts only outside the editor as the primary loop; save-triggered auto-build as an MVP blocker

**Behaviour serialization slice**:
The follow-on to the .NET host MVP: persist and restore an Object's ordered Behaviour list (BehaviourId, type, serializable fields) with the scene/document — still without requiring ALC hot reload or full Inspector authoring UX.
_Avoid_: Treating serialization as part of host MVP, requiring hot reload before Behaviours can round-trip

**Scene Tree**:
The authored parent/child hierarchy of Objects. Outline, reparenting, and naming walk this tree. When an Object has an ECS Entity, transform parenting is projected from the Scene Tree into the ECS World — the tree is not stored twice.
_Avoid_: Dual-written parents on Object and ECS, ECS Parent as the editor-facing tree of record

**Exported type**:
A C++ type deliberately marked for inclusion in ClassDB (and thus in editor and script surfaces). Unmarked types stay engine-private.
_Avoid_: Reflecting every engine type by default, silent auto-export of all headers

**API Blueprint**:
The language-neutral description of Exported types (classes, properties, methods, component schemas) produced alongside ClassDB registration. Script binding generators consume this; they do not scrape C++ headers themselves.
_Avoid_: Hand-written C# stubs as source of truth, per-language duplicate metadata

**PtrCall**:
The zero-/low-allocation dynamic call path: arguments and return values pass through typed slots (conceptually `void**`) without boxing into Variant. Used for script method calls and hot lifecycle dispatch across the C-ABI bridge.
_Avoid_: Boxing every script call into Variant, exposing C++ member pointers to managed code

**Variant**:
A small engine dynamic value used for editor-facing property get/set, serialization glue, and other type-erased tooling paths — not the primary script call ABI.
_Avoid_: Using Variant as the only interop calling convention, conflating with `std::variant` / EASTL variant

### Project management

**Project**:
A self-contained game/content workspace on disk: a root folder identified by a Project File, plus content roots (`Assets/`, `Resources/`), a Scripts root for C# gameplay code, and engine cache (`.blunder/`). Opened as a whole by the editor; not a single scene file and not the engine repository itself.
_Avoid_: Solution, workspace (as the product term), treating the Blunder-Engine git checkout as "the Project" by default, inferring a Project from `Assets/` alone without a Project File

**Project File**:
The required marker at the Project root (`project.blunder`) that identifies the folder as a Blunder Project. Content is YAML; MVP fields are minimal — at least `name` (display name). Create/import/scan/open all key off this file; Create scaffolds `Assets/`, `Resources/`, and `.blunder/`, and from the .NET host MVP onward also scaffolds the Scripts root with a minimal game `.csproj` template. Engine/version keys may appear later but are not required or enforced in the MVP.
_Avoid_: `project.godot`, JSON as the Project File format, relying only on folder layout, putting project identity only inside `.blunder/`, requiring engine-version gates in the first slice

**Scripts root**:
The Project folder `Scripts/` that holds C# gameplay sources and the game `.csproj`. It is not part of the `Assets/` / `Resources/` content pipeline; built assemblies are loaded by the .NET script host, not as Content Browser Assets. Project Create (from .NET host MVP onward) scaffolds this folder with a minimal `.csproj` template.
_Avoid_: Storing gameplay `.cs` under `Assets/`, treating scripts as Intermediate Assets, putting the game project inside the engine repo as the product default, naming this root `Source/` (reserved for DCC Source Assets under `Resources/Source/`), leaving new Projects without a Scripts scaffold once the host ships

**Project Manager**:
The standalone `project_manager` executable that lists, creates, imports, and opens Projects before a full Editor Session. Ships beside `engine_editor` in the same output directory (not a multi-version Hub product).
_Avoid_: Manager as a mode of `engine_editor`, Unity Hub multi-editor-version management, an in-editor dock that assumes a Project is already loaded

**Editor Session**:
A full editor run bound to exactly one open Project (its project root). Started by launching `engine_editor` with that Project's path (typically `--project-root`); not the Project Manager app.
_Avoid_: Multi-project tabs in one process for v1, hot-swapping project root inside a live session (deferred)

**Project Open**:
Leaving Project Manager by spawning sibling `engine_editor` with the chosen Project root (CLI such as `--project-root`), which starts an Editor Session. v1 does not re-initialize a live process onto a new root.
_Avoid_: In-process root swap as the v1 path, opening a Project as merely loading a scene, relaunching `project_manager` as if it were the editor

**Project List**:
The user-level registry of known Projects (paths and list metadata such as favorite / last opened), stored outside any Project. The Project Manager's main view. Remove from list does not delete project files on disk by default. Optional Scan discovers `project.blunder` under chosen folders and adds them to this list.
_Avoid_: Embedding the registry inside a Project, scan-only with no persisted list, treating Remove as Delete Project Contents by default

**Project Manager MVP**:
First shipping slice: persisted Project List, Create (Project File + `Assets/` / `Resources/` / `.blunder/` scaffold), Import (register an existing Project File), Project Open into an Editor Session, Remove from list, and missing-path marking. Out of this slice: Scan, Rename, Duplicate, Favorites, Tags, templates/Asset Library, Recovery Mode, multi-editor-version Hub features.
_Avoid_: Shipping Scan/Favorites/templates in the first milestone as blockers; calling the MVP "Unity Hub complete"

**Project Manager chrome (v1)**:
The Project Manager window layout mirrors Godot's Project Manager spatial rhythm (Projects header, create/import strip, project list, open/remove side actions, status) while exposing only Project Manager MVP actions. Non-MVP Godot affordances are omitted, not shown disabled. List rows show name, path, missing state, and last-opened time. UI language is English; the primary Project Open control is labeled Open. Visual colors follow the editor shell; structure and spacing follow Godot.
_Avoid_: Greyed-out Scan/Favorites/Asset Library/Settings chrome; labeling Open as Edit; a Manager UI language that diverges from the rest of the editor

**Editor entry (v1)**:
Run `project_manager` to choose/create/import Projects. Run `engine_editor` with `--project-root` for an Editor Session. Debug builds that define `BLUNDER_PROJECT_ROOT` may open that root when no `--project-root` is given. Release / packaged builds do not use compile-time root as a silent default.
_Avoid_: Embedding Manager UI inside `engine_editor`; Release silently opening the engine checkout; requiring every Debug contributor to pass CLI with no escape hatch

**Projects directory**:
The canonical parent folder for user-created Projects. On Windows this is `E:/Blunder Projects`. Project Manager Create prefills this path; new Projects are subfolders under it (when Create Folder is on). The engine checkout is not a Project and must not ship a root `project.blunder`.
_Avoid_: Creating Projects inside the Blunder-Engine git tree by default; treating the checkout as the Project List home; a second mandatory sample-project tree for MVP

**Debug Project**:
Debug builds compile in `BLUNDER_PROJECT_ROOT` (CMake `BLUNDER_DEFAULT_PROJECT_ROOT`, default `E:/Blunder Projects/Test`) so `engine_editor` can open that Project without `--project-root`. Product content lives only under the Projects directory, not in the engine checkout.
_Avoid_: Pointing Debug at the engine checkout; requiring every Debug run to go through Project Manager; shipping `Assets/` / `Resources/` inside the engine tree

**Project Create**:
From Project Manager: choose a name and path (default parent: Projects directory), require an empty target directory (or create a named subfolder under a parent via Create Folder), write `project.blunder` plus `Assets/` / `Resources/` / `.blunder/` scaffold, add the Project to the Project List, then Project Open into an Editor Session.
_Avoid_: Creating into a non-empty folder; create-and-stay-in-Manager as the default success path; overwriting an existing Project File; defaulting Create into the engine checkout

**Project Import**:
Register an existing on-disk Project (folder or `project.blunder` path) into the Project List and immediately Project Open it. Does not copy or move files. ZIP/template install is out of the MVP.
_Avoid_: Import-only-without-open as the MVP default; treating Import as Create; requiring a file copy into a Hub-managed library

### Editor icons

**Editor Icon**:
A themed vector glyph used for editor chrome and panel affordances (dock close/pin, browser search/refresh/folder, tree/breadcrumb arrows, Inspector scale-link). Sourced from Godot's `editor/icons` SVG set and shown through dedicated Slint icon components with UI colorization.
_Avoid_: Emoji or Unicode text as the source for those affordances; one-off hand-drawn geometry once a Godot glyph is adopted for the same control

**Scale-link icons**:
The Inspector Scale link toggle uses Godot's linked/unlinked chain pair: linked (proportion locked) and unlinked (axes independent) — the same pair Godot's vector property linker uses (`Instance` / `Unlinked`).
_Avoid_: Padlock Lock/Unlock for Scale link; chain emoji

### Navigate gizmo

**Positive ball**:
An axis endpoint handle on the positive side of a world axis, drawn solid in the axis color with an axis letter.
_Avoid_: Positive sphere, +handle

**Negative ball**:
An axis endpoint handle on the negative side of a world axis. Fill is a fixed neutral gray biased toward the axis color; border is the full axis color. Opaque so it can occlude arms and other balls when in front.
_Avoid_: Negative sphere, −handle, hollow ring

**Axis color**:
The full theme RGB for a world axis (X/Y/Z), used for positive balls, arms, and negative-ball borders.
_Avoid_: Primary color, main-axis color

**Muted axis fill**:
Negative-ball interior color: mix of fixed neutral gray `(0.20, 0.20, 0.20)` and axis color at mix factor `0.25`.
_Avoid_: Semi-transparent fill, view-background mix

### Transform gizmo (translate)

**Translate modal session**:
The shared interaction session for moving a selection in translate mode, whether started from a handle or from grab. Owns motion, constraints, confirm/cancel, and drag feedback.
_Avoid_: Gizmo drag only, ad-hoc translate handler

**Grab entry**:
Starting a Translate Modal Session with `G` for free view-plane translation. Confirms with LMB click (not release). Distinct from handle entry.
_Avoid_: Gizmo drag, handle press

**Constraint guide**:
Axis-colored lines through the drag-start pivot showing the active translation constraint. One line for a single axis, two for a plane, none for free center move. Guides stay pinned at drag-start while the object moves.
_Avoid_: Helpline, rubber-band, axis ray

**Drag ghost**:
A semi-transparent copy of only the active translate handle, drawn at the drag-start pose for the duration of the session.
_Avoid_: Full gizmo ghost, afterimage

**Origin dot**:
A small colored disc at the root of the active axis arrow (or on the active plane handle) during constrained translate drag. Single-axis drags use that axis color; plane drags use the plane's normal-axis color.
_Avoid_: Center disc, pivot bead (when meaning the always-visible free-move center)

**Transform-active outline**:
The temporary object outline color used while a translate modal session is active; a fixed light/off-white distinct from the normal selection outline. Restored when the session ends.
_Avoid_: Drag highlight, selection glow

**Reference axis arrows**:
The three axis arrows kept visible during translate drag as orientation reference. They follow the object's current position. Plane handles and the free-move center are hidden according to which handle is active.
_Avoid_: Inactive gizmos, leftover handles

**Constraint orientation cycle**:
Re-pressing the same axis or plane key during a translate modal session cycles constraint orientation: global → local → free. A different key starts a new global constraint for that axis or plane.
_Avoid_: Toggle mode, one-shot constrain

**MMB axis pick**:
Middle-mouse drag during an active translate modal session picks the nearest projected axis from the mouse delta (world axes when free/global; local axes when orientation is already local) and commits a single-axis constraint on release.
_Avoid_: Orbit camera, right-click axis select

**Numeric input**:
Typed digits, minus, and decimal during a translate modal session set a signed distance along the active constraint; pointer motion is ignored while numeric input is active.
_Avoid_: Inspector field, typed offset in free mode

### Inspector Transform

**Local Transform**:
The entity's Position, Rotation, and Scale relative to its parent. The Inspector Transform section edits only this; there is no Global/World transform editor in v1.
_Avoid_: World transform editor, Global Transform (as an editable Inspector section)

**Rotation Edit Mode**:
Inspector presentation of a node's rotation: either Euler or Quaternion. Changing the mode only changes how rotation is shown and edited, not how it is stored.
_Avoid_: Rotation Order UI, Basis mode (out of v1 scope)

**Authoritative rotation**:
The Quaternion stored on the entity. Euler angles in the Inspector are a derived edit view; writing an Euler axis recomposes and writes this Quaternion immediately.
_Avoid_: Dual Euler store, persistent Euler component

**Fixed Euler order**:
The single, non-configurable Euler decomposition/composition used by the Inspector Euler view. It is the engine's established SceneSerializer convention (`qz * qy * qx` on world X/Y/Z) so Inspector matches scene assets and current editor behavior. No Rotation Order dropdown in v1.
_Avoid_: User-selectable rotation order, second Inspector-only Euler convention, literal Godot Y-up YXZ on Blunder axes

**Scale link**:
Inspector toggle that keeps Scale X/Y/Z proportional when any one axis is edited: changing one axis multiplies all three by the same factor so existing ratios are preserved. If the edited axis is near zero, only that axis changes. Link state is session UI state, not scene data.
_Avoid_: Uniform scale button, copy-absolute lock, force-equal axes

**Transform field commit**:
Keyboard edits to Inspector axis fields apply to the entity only on Enter or focus loss. Slider drags and pointer scrubbing apply live. Invalid text on commit reverts the field to the authoritative value.
_Avoid_: Keystroke-live apply, defer-all-including-sliders

**Multi-edit mode**:
When multiple entities are selected, an Inspector option chooses how Transform edits apply to the whole selection: Absolute (set each selected entity's edited component to the same value) or Delta (add the same change to each selected entity's current value).
_Avoid_: Primary-only edit, silent multi-edit without a mode

**Absolute multi-edit**:
Multi-edit mode that writes the committed field value as the new local component on every selected entity (e.g. set all local X positions to 5). For mixed values across the selection, the field shows an empty/mixed placeholder until the user enters a value. Applies to Position and Scale; not used for Rotation in v1.
_Avoid_: Replace mode, set-value mode (unless meaning this)

**Delta multi-edit**:
Multi-edit mode that treats the field as an offset from zero: the displayed baseline is 0, and the committed/live delta is added to each selected entity's current local component. This is the only multi-edit path for Rotation in v1.
_Avoid_: Relative mode, incremental mode (unless meaning this)

**Mixed component**:
A Transform field state when multiple selected entities disagree on that local component; shown as a placeholder rather than a single number until Absolute edit supplies a value.
_Avoid_: Average value, primary value shown as if shared

**Focused field lock**:
While an Inspector Transform field has focus and a keyboard draft, external Transform updates (gizmo, multi-edit from other tools) must not overwrite that field's draft text. Unfocused fields may refresh. Escape discards the draft and shows the authoritative value again.
_Avoid_: External wins, disable gizmo while focused

**Quaternion normalize-on-commit**:
Editing Quaternion x/y/z/w writes a normalized Quaternion as the authoritative rotation; the fields then show the normalized components.
_Avoid_: Non-unit quaternion storage, reject-on-non-unit

**Euler delta rotation**:
Multi-select Rotation edits always apply as the same Euler-angle delta (Fixed Euler order YXZ) on top of each selected entity's current authoritative rotation, even when the Inspector is in Quaternion presentation for a single selection.
_Avoid_: Per-component quaternion absolute multi-edit, raw quat-component delta multi-edit

**Transform undo (v1)**:
Superseded by Editor History (ADRs 0006–0007 / OpenSpec `editor-history`): transform commits, spawn, and soft-delete become undoable via Document History. Until that change is implemented, Inspector/gizmo Transform edits remain non-undoable write-through.
_Avoid_: Assuming Ctrl+Z works for Transform before `editor-history` is applied

### Editor history

**Editor History**:
The editor's authorship undo system: Document History for the active scene, plus Global History for non-document editor actions. It does not record runtime gameplay or Play-mode simulation steps.
_Avoid_: Engine-wide action history shared with gameplay, treating script Tick as undoable, one undifferentiated stack for scene and settings

**Editor Command**:
A single reversible unit on Editor History (Document or Global). It exposes undo and redo. Continuous interactions (e.g. a Translate Modal Session) become one Command at confirm — not one Command per pointer move.
_Avoid_: Per-frame history entries, full-scene snapshot as the default history unit

**Document History**:
The scene-scoped Editor History for one open editable document — for v1, the active scene (`activeScenePath` / active `SceneInstance`). Opening another scene replaces or clears that history. It is not Global History and does not hold editor-preference commands.
_Avoid_: Cross-scene undo continuum, surviving history after the document instance is unloaded, stuffing settings edits into the scene stack

**Global History**:
The separate Editor History stack for non-document editor actions (preferences, layout, and similar). Independent from Document History: different filter, different jump target. This milestone ships the stack and UI filter as an empty placeholder — no Global Commands yet.
_Avoid_: Merging into Document History, treating empty Global as "not a real stack", cross-scene entity edits on the Global stack

**History Panel**:
The dock UI that lists Editor Commands from the filtered history stacks. It lives as a sibling tab to Content Browser (filesystem) in the same tab group. Clicking an entry performs a History Jump on that entry's stack.
_Avoid_: Output log, read-only audit trail, a floating panel unbound from the Content Browser tab group (for this milestone)

**History scope filter**:
The Scene / Global checkboxes on the History Panel. Scene shows Document History; Global shows Global History. Default: both checked. Both checked this milestone still shows only Scene (no merge). Both unchecked shows an empty list.
_Avoid_: Interleaved timeline merge before Global has commands; fake filters with no backing stack; starting with Global-only checked so the panel looks broken

**History Jump**:
Seeking Document History (or later Global History) to a chosen command by running undo/redo until the stack cursor matches that entry. The History Panel click path and keyboard Undo/Redo share the same stack APIs.
_Avoid_: Selection-only highlight without mutating the document; a second undo implementation only for the panel

**Command label**:
The English display string for an Editor Command in the History Panel. Prefer an action plus entity name (e.g. `Move Player`, `Delete Cube`); fall back to a type-only phrase (`Set Transform`, `Spawn Entity`, `Delete Entity`) when no name is available. The entity name is snapshotted when the command is pushed; later renames do not rewrite older labels. Not a localization key in this milestone.
_Avoid_: Chinese UI copy for this panel, code identifiers as the only label, omitting the entity name when it is known, live-resolving names so rename rewrites past history rows

**History row state**:
How a Document History (or Global History) entry appears relative to the stack cursor: applied entries (at or before the cursor) use full emphasis; the redo tail (after the cursor) is visually muted (dimmed/gray). The current position is highlighted. Muted rows remain clickable for History Jump. The panel lists entries oldest-at-top, newest-at-bottom.
_Avoid_: Hiding the redo tail, identical styling for applied and redo rows with selection as the only cue, newest-first ordering for this panel

**Command target (v1)**:
Editor Commands address scene entities by `EntityId` within the active `SceneInstance`, matching current selection/gizmo/Inspector paths. ObjectId targeting is deferred until scene editing is Object-backed.
_Avoid_: Requiring ObjectId for the first undo milestone, dual-ID on every Command

**Editor History MVP commands**:
v1 Commands cover (1) entity transform commits (gizmo drag end, Translate Modal confirm, Inspector TRS commit) and (2) entity spawn/delete. Reparent and broader Outliner hierarchy edits stay out of this milestone unless they fall out of spawn/delete.
_Avoid_: Empty history plumbing-only MVP; deferring all spawn/delete to a later change after transform-only

**Delete undo (v1)**:
Editor "delete" is a soft delete: the entity keeps its `EntityId`, is removed from the editable view and disabled (tombstone). Undo restores visibility/enabled state. Physical removal and generational/stable free-list `EntityId` allocation are deferred.
_Avoid_: Mid-vector erase that shifts dense `index+1` EntityIds; best-effort new IDs on undelete for v1

**Command coalesce (v1)**:
No timer-based merge. Commands are sealed only at interaction boundaries: gizmo drag end / Translate Modal confirm / Inspector field commit (Enter, focus loss, or spinner end). Intermediate drag/scrub values do not push history.
_Avoid_: Per-keystroke or per-mousemove history entries; coalesce windows that invent commit points separate from UI interaction ends

**Dirty vs Editor History**:
Document dirty tracks divergence from the last successful save. Saving records a history baseline cursor; undo/redo back to that cursor clears dirty, otherwise marks dirty. Explicit Save still clears dirty and refreshes the baseline.
_Avoid_: Leaving dirty true after undoing to the saved document state; treating every undo as unconditionally dirty with no baseline

**Soft-delete and save**:
Save / `exportToScene` omits tombstoned entities so the on-disk scene matches the editable document. Tombstones remain in the live `SceneInstance` only to support in-session undo. Opening a scene still clears Document History.
_Avoid_: Persisting deleted/tombstone flags in `.scene.asset` for v1; requiring reopened scenes to restore prior undo stacks

**Selection and Editor History**:
Each Editor Command stores before/after selection snapshots. Undo/redo restores the selection that belongs to that history step (not only the mutated EntityId).
_Avoid_: Leaving selection on a now-irrelevant entity after undo; treating selection as entirely outside history for v1

**History stack limit (v1)**:
Document History keeps a fixed maximum number of commands (default **100**). Pushing past the limit drops the oldest entry. No memory-budget accounting in v1.
_Avoid_: Unbounded growth for the first milestone; byte-accurate memory budgets before command sizes are measured

**Editor History input (v1)**:
Undo/Redo are reachable via editor shortcuts (Ctrl+Z; Redo accepts Ctrl+Y and Ctrl+Shift+Z) and Edit menu items, enabled from Document History canUndo/canRedo. The same Document History API backs shortcuts, menu, and History Jump on the scene stack. This milestone does not route shortcuts to Global History (Global remains panel-only until Global Commands exist and routing is decided).
_Avoid_: API-only milestone with no user-facing undo; menu-only without shortcuts; pretending focus-based Global shortcut routing ships with the empty Global stack

**Linear history (v1)**:
Document History is a single linear stack. After undoing, executing a new Editor Command discards the redo tail beyond the current cursor. No branched timelines in v1.
_Avoid_: Multi-branch undo trees; preserving redo after a divergent new edit

**Axis number field**:
Reusable Inspector control for one Transform component: axis-colored label, editable number, unit suffix, and pointer scrubbing that applies live.
_Avoid_: Plain LineEdit, slider-only row (for Position/Scale)

**Inspector Transform session state**:
Editor-session UI toggles for Rotation Edit Mode (default Euler), Scale link (default on), and Multi-edit mode (default Absolute). They persist across selection changes within a run and are not saved to the scene.
_Avoid_: Per-entity UI prefs, disk-persisted Inspector toggles

**Transform section chrome**:
Godot-like Transform layout and controls inside Blunder Inspector styling (colors, fonts, borders). Collapsible Transform header. Other Inspector sections (e.g. shading) are out of scope for this change.
_Avoid_: Godot theme skin, restyle entire Inspector

### Asset pipeline

**Source Asset**:
A DCC-native authoring file (e.g. `.blend`, `.psd`, `.fbx`) that holds the full creation data. Source Assets alone must be enough to rebuild Intermediate and Final Assets. They are not loaded by the runtime.
_Avoid_: Treating glTF/PNG as Source, calling cooked binaries "source", storing only Source under `Assets/`

**Intermediate Asset**:
A lossless (or near-lossless), readable exchange/process form between Source and Final — primarily glTF and images, plus YAML descriptors (GUID, import settings, references). Fast Path and the Build Process operate on Intermediate Assets.
_Avoid_: COLLADA as the Blunder intermediate (use glTF), opaque binary as the only intermediate, pruning/platform-optimizing at this stage

**Final Asset**:
A platform-optimized, pruned runtime form produced by the Build Process (today: `.blunder/cooked/{guid}.*bin`). The runtime prefers Final Assets when present and fresh.
_Avoid_: Hand-editing Final Assets as the source of truth, shipping DCC-native files as Final

**Assets root**:
The project folder `Assets/` that holds Intermediate **descriptors** (YAML: GUID, import settings, references). It is the Content Browser’s primary tree and the engine’s identity entry for content — not the home for glTF/PNG bodies or Source Assets.
_Avoid_: Storing Intermediate data files or Source Assets under `Assets/`, treating `Assets/` as Unity-style “everything content”

**Resources root**:
The project folder `Resources/` whose non-Source subtree holds Intermediate **data bodies** (glTF, images, audio, …) referenced by descriptors. Not scanned as the Content Browser’s primary tree.
_Avoid_: Putting descriptors under `Resources/`, using `Resources/` as the Final Asset store

**Source root**:
The subtree `Resources/Source/` reserved exclusively for Source Assets. Not loaded by the runtime and not treated as Intermediate data.
_Avoid_: Mixing Source and Intermediate files in the same folder, putting Source under `Assets/`

**Cooked cache**:
The project store `.blunder/cooked/` for Final Assets keyed by GUID. Derived, regenerable, not a source of truth.
_Avoid_: Checking in cooked data as authoritative content, editing cooked binaries by hand as the workflow

**Asset**:
The product-facing unit of content identity, keyed by GUID. One Asset binds an Asset Descriptor to Intermediate data (and optionally a Source Asset). Content Browser entries, references, Manifests, and Cook all address Assets — not raw files alone.
_Avoid_: Calling a lone glTF/PNG file “the Asset”, equating Asset with the in-memory runtime object, path-only identity without GUID

**Asset Descriptor**:
The Intermediate YAML under the Assets root that persists an Asset’s GUID, type, import settings, and references to Intermediate data (and optional Source). The durable on-disk face of an Asset.
_Avoid_: Embedding identity only in the registry with no descriptor file, putting descriptors under Resources

**Loaded Asset**:
The in-memory CPU-side resource the runtime holds for an Asset (or a slice of one), e.g. mesh/texture payloads. Distinct from the product term Asset; not what Content Browser identity means.
_Avoid_: Using “Asset” alone when you mean the runtime object, treating Loaded Asset as the source of truth

**Pull pipeline**:
The user-facing Asset Pipeline model: the editor/runtime pulls Assets by identity; Final Assets are produced on demand when missing or stale. Intermediate changes invalidate dependent Finals and trigger re-Cook. Full-project Cook remains for packaging/CI, not the daily authoring path.
_Avoid_: Push-only “run full cook then open” as the primary loop, requiring artists to manually invoke Build before every preview

**Cook**:
The Build step that turns Intermediate Assets into Final Assets for one Asset (and its direct needs). v1 Cook is incremental and demand-driven; startup whole-project cook is optional sugar, not the definition of the pipeline.
_Avoid_: Equating Cook with Import, treating Cook as only a batch CI job with no editor path

**Fast Path**:
Loading Intermediate data into a Loaded Asset for preview when Final is missing or stale, without waiting for Cook. Final may replace it later when Cook finishes. Fast Path and Final may coexist in one Editor Session.
_Avoid_: Blocking the first preview until Cook completes, treating Intermediate-only load as an error instead of an intentional path

**Asset Dependency Graph**:
A directed graph of Asset→Asset references used to invalidate Finals and to know what to Cook. v1 minimal edges: Scene→Mesh Asset; Mesh Asset→Texture Asset when the mesh records an explicit Texture Asset reference (embedded glTF textures that are not registered Assets are out of graph); each Asset→its Intermediate inputs (descriptor + `source` file) as leaves for freshness. No Material Asset nodes, animation/audio/shader edges, or Source-file parsing in v1.
_Avoid_: Hand-maintained file lists, “only this Asset’s own mtime matters” with no cross-Asset invalidation, treating a packaging Manifest as required before the graph exists, inventing Material graph nodes before material descriptors exist

**Manifest**:
The set of Assets required by a top-level product unit (e.g. a scene/level), derived by walking the Asset Dependency Graph from that root. Used later for packaging and “what is still needed”; not a separately hand-edited list.
_Avoid_: Manually curated include lists as the source of truth, shipping every file under Resources because Manifest is unknown

**Asset Reference**:
A durable cross-Asset link stored as the target Asset’s GUID (not a filesystem path). Scenes and Mesh→Texture links use Asset References. Paths remain for display and for resolving a GUID via the registry; legacy path-only scene fields migrate to GUID on save.
_Avoid_: Path-as-canonical reference, renaming descriptors without a GUID identity, treating virtual path as the stable public identity of an Asset

**Import**:
The act of registering external content as project Assets: allocate GUID, write an Asset Descriptor, place Intermediate data under the Resources root, and register the Asset. When the input is a Source Asset, Import also runs automatic export into Intermediate before registration. Import is not Cook and is not opening a DCC for manual editing.
_Avoid_: Calling Intermediate glTF/PNG “Source”, equating Import with Cook, leaving Source files as the only registered form with no Intermediate

**Source Export (v1)**:
The Import sub-step that converts a Source (or DCC exchange) file into Intermediate data. v1 supports an Assimp whitelist — primarily FBX and OBJ → glTF under the Resources root — then registers the Asset as usual. `.blend` / `.psd` automatic export is out of v1 (optional later via Blender CLI or similar). On Source Export, the original file is archived under the Source root and the descriptor may record that Source path; Cook and Fast Path always consume the Intermediate, which can be regenerated from the archived Source on Reimport.
_Avoid_: Claiming silent `.blend` export in v1, treating every Assimp format as supported without a whitelist, skipping Intermediate and cooking Source bytes directly, discarding FBX/OBJ after conversion with no Source archive

**Scene Asset**:
A first-class Asset for a level/scene document. Persisted as `.scene.asset` JSON with a GUID field; registered like other Assets. Entity mesh links are Asset References (GUID). It is a root for walking the Asset Dependency Graph and (later) Manifests.
_Avoid_: Path-only scene identity, treating scenes as non-Assets outside the registry, omitting GUID from scene documents

**Reimport**:
Re-running the Import/Source Export path for an existing Asset to refresh Intermediate from archived Source (when present) or to re-apply import settings to existing Intermediate, then invalidating Finals and dependents. Distinct from Cook.
_Avoid_: Equating Reimport with Cook, requiring delete-and-re-import to pick up Source changes

**Asset Watch**:
Editor file watching that drives Pull freshness: the Assets root and Intermediate data under the Resources root. Changes invalidate Finals (and dependents via the Asset Dependency Graph). Changes under the Source root trigger automatic Reimport for Assets that archive that Source.
_Avoid_: Watching only Assets for browser refresh with no Cook invalidation, ignoring Intermediate/Source mutations on disk
