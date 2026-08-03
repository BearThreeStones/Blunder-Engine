# Blunder Engine

Editor and runtime for a Z-up, glTF-aligned 3D engine with Blender-inspired viewport gizmos.

## Language

### Reflection & scripting

**Gameplay scripting language**:
C# is the sole first-class language for Project gameplay logic. The engine does not ship or productize other script languages (Lua, GDScript, etc.); the C-ABI bridge may still be used by non-product hosts.
_Avoid_: Multi-language official scripting, Lua/GDScript as co-equal product tracks, treating C# as DogWalk-only scaffolding

**.NET script host**:
The in-process CoreCLR host (`nethost` / hostfxr) that loads Project C# assemblies and owns Script Peers. It talks to the engine only through the C-ABI bridge. Assembly-Load-Context hot reload is a later phase on the same host, not part of the first host milestone. For Play Mode, the host runs inside the Player process; Edit Mode does not start it for normal authorship.
_Avoid_: Mono as the product host, out-of-process `dotnet` IPC as the shipping model, bundling ALC hot reload into the first host slice, direct P/Invoke of C++ member layouts; treating an editor-process host as required for Play Mode

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

**Message**:
A directed gameplay communication addressed to an Object by ObjectId. Identified by a MessageId and carrying a small Variant argument bag of at most four arguments. Authors send via a global Message facade (`Message.Send` / `Message.Register`), not via Object instance methods as the primary API. Delivery is synchronous by default and fans out to every Behaviour on that Object in list order; each Behaviour may handle or ignore it (no consume/stop-propagation). During Send, the engine snapshots that Object's Behaviour list and skips destroyed/tombstoned peers; deeper reentrancy safety is by convention, not a full deferred-mutation transaction. The engine receive path for Behaviours is a single entry (`OnMessage`); per-name handler methods are optional authoring sugar later, not the MVP bridge contract. The MVP Send surface is C# (Behaviour-to-Object); native/C++ Send uses the same delivery path but ships later. Used only for cross-Object notifications and commands — not for queries/reads (those use the Object property surface or sibling Behaviour access), not for Lifecycle dispatch, and not as a substitute for ordinary property get/set.
_Avoid_: Signal (subscription / emit-to-unknown-listeners) as this primitive, SendMessage-to-BehaviourId as the primary address, routing Ready/Tick/Draw through Message, GETPOS-style query Messages as the product pattern, Win32-style raw int/pointer parameter pairs as the product ABI, default deferred/queued delivery, Handled-stops-siblings as the default fan-out rule, requiring Handler-time Destroy to be deferred as the MVP rule, requiring named `OnDamage`-style methods as the only receive path for MVP, unbounded Variant arrays as the Message payload, requiring physics/native systems to Send in the Message MVP, Object.Send as the primary product API

**MessageId**:
The stable identity of a Message kind. Games register string names that resolve to MessageIds; the engine does not ship a fixed gameplay vocabulary (Damage/Hit/…) as builtins. Receivers dispatch on MessageId; unknown ids are safely ignored.
_Avoid_: Overloading ObjectId/BehaviourId, requiring every MessageId to be an engine-built-in enum, stringly-typed send with no id resolution as the only path, treating engine-owned Hit/Damage/Die ids as the MVP product surface

**Reflection kernel**:
The first deliverable of the reflection system: ClassDB, Clang-driven export for a narrow set of types, PtrCall plus a small Variant path, API Blueprint, Object/`ObjectId`, projected property accessors, and a C-ABI skeleton — without shipping a .NET host, ALC hot reload, multi-Behaviour storage, or a full ECS rewrite of `SceneInstance`. The kernel Object retains a single Script Peer slot; multiple Behaviours arrive with the .NET host MVP (ADR 0011).
_Avoid_: Treating C# hot reload, multi-Behaviour storage, or full ECS migration as part of the first reflection milestone

**.NET host MVP**:
The first script-host deliverable after the Reflection kernel: in-process CoreCLR, load one Project assembly, attach Behaviours to Objects, Ready/Tick per Behaviour list order — without ALC hot reload, without Behaviour scene serialization, and without Inspector Behaviour UX. The **DogWalk character slice** is written as C# Behaviours on this host, not as a throwaway C++ controller to migrate later. In development, the editor invokes `dotnet build` on the Scripts root (manually or before Play) and loads the output assembly; shipping builds use a separate publish/cook path. File-watcher auto-build is out of this slice.
_Avoid_: Bundling hot reload, scene-persisted Behaviours, or full binding surface into the host MVP; starting DogWalk gameplay on a C++-only controller as the lasting path; shipping the host before the Reflection kernel contract is stable; requiring authors to build Scripts only outside the editor as the primary loop; save-triggered auto-build as an MVP blocker

**DogWalk character slice**:
The first DogWalk content milestone on Blunder: a Blunder Project whose Play Mode proof is horizontal Move via a single C# Behaviour (`PlayerMove`) driven by Gameplay Input — not a Godot DogWalk port and not a full DogWalk feature set (no Jump requirement, no physics, no animation, no camera follow, no dual player entities). Delivery is content-primary: Project Scripts, scene, and Behaviour authoring — not new engine capabilities.
_Avoid_: Treating the Godot DogWalk tree as the Blunder Project; shipping Chocomel/Pinda parity as this milestone; bundling asset pipeline port or narrative systems into the first slice; requiring a newly named DogWalk Project before the Move proof can land; splitting the first Move proof across multiple collaborating Behaviours; treating new engine features as part of this milestone

**Behaviour serialization slice**:
Follow-on to the .NET host MVP (requires single-ObjectDB / NativeAbi from `unify-script-objectdb`): persist and restore an Object's ordered Behaviour list on scene entities (`behaviours`: BehaviourId, type, optional bool/number/string property bag). Load binds Objects and restores slots offline (null peers); mount AttachBehaviour when DotNetHost is running; export writes type + id from bound Objects. Inspector authoring of that declaration list is a separate follow-on (**Inspector Behaviour UX**, ADR 0016), not part of this serialization slice itself.
_Avoid_: Treating serialization as part of host MVP, requiring hot reload before Behaviours can round-trip; treating serialization deliverable as including full Inspector Behaviour UX

**Inspector Behaviour UX**:
Edit Mode authoring of an entity's Behaviour list and property bag in the Inspector. The authoritative Edit Mode surface is the scene **Behaviour declaration** (ordered type + BehaviourId + bag), not a live Script Peer. Mounting peers remains a Play/Player (or env-gated debug host) step, not a prerequisite for Inspector edits. Available types for Add come from a **Behaviour type catalog** produced by scanning the Project Scripts assembly after `dotnet build` (metadata only; no DotNetHost required). The first UX slice edits only **bool / number / string** public instance fields or properties — the same narrow bag already persisted and applied on mount. Add, Remove, reorder (drag), and property commits (Enter / focus loss) are Document History Commands addressed by `EntityId`, same interaction-boundary sealing as Transform. A declaration whose type is missing from the catalog stays in the list as a visible missing/broken entry (remove allowed; not auto-deleted).
_Avoid_: Requiring editor DotNetHost for normal Behaviour authoring; treating live peers as the Edit Mode source of truth; conflating Add/Remove Behaviour UI with AttachBehaviour mount; hand-typed CLR names as the primary Add path; starting CoreCLR solely to populate the Add list; shipping Vec3/enum/nested/asset-reference Behaviour editors in the first Inspector Behaviour UX slice; silent non-undoable Behaviour edits in that slice; silently dropping missing-type declarations; blocking Save/Play solely because a Behaviour type is missing from the catalog; treating list order as non-authoritative relative to Ready/Tick order

**Behaviour type catalog**:
An editor-facing list of concrete non-abstract Behaviour CLR types discovered from the Project's built Scripts assembly metadata after `dotnet build`. Each entry includes the type's public bool/number/string instance members so Inspector can draw a full property form without a live peer. The catalog refreshes after a successful Scripts build (same path as the Play Scripts gate); opening Add with a missing catalog prompts or triggers that build. Used by Inspector Add Behaviour and property rows; not the live Script Peer table and not ClassDB.
_Avoid_: Runtime PeerTable as the type picker source; ClassDB registration of game Behaviours; requiring AttachBehaviour to discover types; showing only keys already present in the scene property bag as the primary form; free-form bag keys unbound from CLR members as the primary authoring path; rescanning the assembly on every Add-menu open as the primary refresh policy; manual-only catalog refresh with no build hook

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
The editor's authorship undo system: Document History for the active scene, plus Global History for non-document editor actions. It does not record runtime gameplay or Play Mode simulation steps.
_Avoid_: Engine-wide action history shared with gameplay, treating script Tick as undoable, one undifferentiated stack for scene and settings

### Play

**Play Mode**:
A session in which the open Project runs as a game for the author to try, separate from scene authorship editing. Gameplay simulation does not share the editor's authorship SceneInstance as its live world.
_Avoid_: In-editor Play that mutates the editable document in place as the product model; treating ScriptHost-on as synonymous with Play Mode

**Edit Mode**:
The normal editor session for authoring scenes, assets, and project settings — not currently running a Play Mode session.
_Avoid_: Calling idle editor "stopped Play" without a Play Mode concept

**Play Process**:
The separate OS process that runs Play Mode for the open Project. It owns the live gameplay world and script host for that session; the editor process remains in Edit Mode and does not host that gameplay ObjectDB.
_Avoid_: Second editor window in the same process as the product Play boundary; treating env-gated in-editor DotNetHost as Play Mode

**Player**:
The dedicated Play Process executable (`engine_player`) — a thin entrypoint over the shared engine runtime, not the editor shell. It runs Play Mode only; it is not an authorship UI. In the Player, only Gameplay Input (plus system window chrome such as close) is accepted; authorship input (Editor Camera, Editor Overlays / gizmos, viewport pick, Editor Commands) is off.
_Avoid_: Reusing `engine_editor` with a play flag as the long-term Player; a fully forked second engine tree for Play; treating Player as a second editor viewport

**Editor Overlay**:
Authorship-only viewport chrome that is drawn and hit-tested in the editor viewport — ground grid, Transform gizmo, Navigate gizmo, selection outline, world axes, origins, wireframe, Camera Gizmo (scene Camera Component visualization), and similar tools. The Player never shows or interacts with Editor Overlays in Play Mode (including while Play Pause is active). The editor viewport keeps Editor Overlays while a Play Session runs.
_Avoid_: Game HUD; debug draw as product Overlay; “game mode chrome”; hiding editor-viewport overlays merely because a Play Session is open; a Player debug toggle to force Editor Overlays on in the first slice; treating Editor Camera orbit as Player gameplay

**Camera Gizmo**:
The Editor Overlay that visualizes and interacts with a scene **Camera Component** in the editor viewport. Visual language matches Blender’s camera wire: origin point, four frustum edges, a **view frame** rectangle, and an **up triangle** on the frame’s top edge. Unselected cameras draw the same shape in a muted color; a **single** selected camera uses the selection color and exposes FOV / clip interaction handles (multi-select draws bodies/frames but not those handles). Frame aspect follows the current editor viewport; frame depth is a fixed local display distance (not a stored sensor aspect). Hit-testing the Camera Gizmo takes priority over mesh viewport pick. It is not the **Editor Camera** and is never shown or driven in the Player.
_Avoid_: Editor Camera widget; Play view HUD camera; treating Navigate gizmo as the Camera Gizmo; inventing a separate non-Blender camera icon language for the first slice; FOV/clip handles on a multi-camera selection

**Camera Preview**:
An authorship-only floating panel over the editor viewport that shows a live view through a selected scene **Camera Component** (pose + FOV + near/far). It is Slint chrome plus a dedicated preview image, not an OverlaySystem draw into the main viewport offscreen, and never appears in the Player.
_Avoid_: Game View dock; Play window; Editor Camera widget; baking the PiP into `viewport-image`; using Camera Preview to preview a Mesh Asset (that is **Mesh Preview**)

**Mesh Preview Render**:
The shared authorship render path that draws a **Mesh Asset** (Final preferred when fresh, otherwise Fast Path Intermediate) with automatic bounds framing and fixed studio lighting into a dedicated offscreen target — not the main viewport offscreen and not the **Camera Preview** target. It produces still frames for **Content Browser Thumbnails** and live frames for **Mesh Preview**. Skinned meshes use bind-pose (or equivalent rest) stills in the first slice; it does not play AnimationPlayer clips for thumbnails.
_Avoid_: Using a material base-color texture alone as the Mesh thumbnail; sharing the Camera Preview or main viewport render target for Mesh Asset frames; requiring Cook before any 3D Mesh thumbnail; embedding AnimationPlayer playback into thumbnail generation as the first slice

**Content Browser Thumbnail**:
The cached still image shown for a Content Browser grid entry. For Mesh Assets the product image is a **Mesh Preview Render** frame written into the project thumbnail cache; for **Scene Assets** it is a **Scene Thumbnail Render** frame; Texture Assets keep image thumbnails; remaining types use placeholders (including a Scene placeholder when scene still generation fails).
_Avoid_: Treating Mesh thumbnails as texture atlases by default; synchronous GPU generation that blocks the whole Browser refresh as the product path (generation is asynchronous with visible-item priority); using a live dirty editor SceneInstance as the Scene thumbnail source

**Scene Thumbnail Render**:
The authorship-only still path that, for an on-disk **Scene Asset**, temporarily instantiates that scene (including recursive **childScenes** like normal load), resolves a camera with the Play rule (Main Camera, else first stable Camera), draws one frame at square aspect into a **dedicated** offscreen target (not the main viewport, not **Camera Preview**, not **Mesh Preview Render**), and CPU-readback writes the **Content Browser Thumbnail** cache. No Editor Overlays. Skinned content stays bind/rest pose (no AnimationPlayer sampling in this slice). Prefer scene lights when present; otherwise fall back to fixed studio lighting. Cache invalidation uses the scene file mtime plus a fingerprint of direct Mesh Asset References on the root and recursive child scenes. On failure (no camera, render error, etc.) show a Scene placeholder.
_Avoid_: Borrowing the Camera Preview or main viewport RT for Browser scene thumbs; first-mesh-only proxy as the product Scene thumbnail; sampling AnimationPlayer clips for scene thumbs in this slice; square thumbs that use the live editor viewport aspect; requiring authored cover images for v1

**Mesh Preview**:
An authorship-only interactive view of a **Mesh Asset** embedded in the Inspector when that Asset is selected in the Content Browser (**Asset Inspector** mode): orbit, zoom, and reset to default framing. Orbit orientation is session-ephemeral (not persisted). It consumes **Mesh Preview Render** and never appears in the Player.
_Avoid_: Floating Camera Preview panel for Mesh Assets; requiring a scene Entity selection to preview a Mesh Asset; persisting orbit angles as Asset or scene data in the first slice; middle-mouse pan as a first-slice requirement

**Asset Inspector**:
Inspector presentation when the selection is a Content Browser **Asset** rather than a scene Entity. The first slice covers Mesh Assets: **Mesh Preview** plus read-only identity (display name, GUID, type, Intermediate `source` path). It is not the Import-settings editor and not a dependency-graph browser.
_Avoid_: Equating Asset Inspector with full Import/Reimport UX; requiring Asset Inspector for every Asset type in the first slice; driving Mesh Preview only from MeshRenderer Entity selection

**View frame**:
The rectangular wire on a **Camera Gizmo** that represents the camera’s imaged bounds at the gizmo’s fixed display distance, sized from vertical FOV and the editor viewport aspect.
_Avoid_: Sensor plane as a separate authored asset in this slice; near-plane-only frame as the sole body

**Up triangle**:
The filled triangle on the top edge of the **view frame** that marks the Camera Component’s local up.
_Avoid_: Roll arrow; separate billboard icon

**Align View to Camera**:
A one-shot Edit Mode action that moves the **Editor Camera** to match a target **Camera Component**'s pose and vertical FOV. Target is the single selected Camera entity when exactly one such selection exists; multi-select is invalid. With no selection, target follows the same resolve rule as Play: **Main Camera**, else first valid Camera (stable EntityId order). With no Camera in the scene, the action fails. Does not copy near/far. Does not push **Document History** (Editor Camera is not scene document state).
_Avoid_: Forcing the editor viewport to always follow Main Camera; live lock to Play view; treating Align View as an undoable scene Command; silently syncing clip planes; aligning under multi-select

**Align Camera to View**:
A one-shot Edit Mode action that writes the current **Editor Camera** pose and vertical FOV into a target **Camera Component**. Target rules match **Align View to Camera** (single Camera selection, else Main then first, else fail). Does not write near/far. Seals a **Document History** Command at the action boundary.
_Avoid_: Silently rewriting every Camera; using this as the only way to author Camera pose; skipping history for scene Camera writes; overwriting clip planes from the Editor Camera; aligning under multi-select

**Editor Camera**:
The Edit Mode free-look / pan / zoom view used to author the scene in the editor viewport. It is not a scene Camera Component and does not drive the Player view. The Player does not accept Editor Camera interaction (including while Play Pause is active).
_Avoid_: Using Editor Camera as the product Play view; Pause-time orbit of the Player window; calling Editor Camera a Gameplay Camera

**Camera Component**:
A native scene Component (like MeshRenderer) on an entity: pose follows that entity’s TRS; stores projection parameters (at least FOV, near, far) and whether it is the Main Camera. It is not a C# Behaviour. The Player renders only through a resolved scene Camera; Edit Mode keeps using Editor Camera for authorship.
_Avoid_: Camera-as-Behaviour for the first Play view; requiring camera-follow gameplay for the Camera Component MVP; forcing the editor viewport to track Main Camera

**Main Camera**:
The Camera Component marked as the primary Play view. When several Cameras exist, the Player prefers the Main Camera; if none is marked, it uses the first valid Camera in the scene. Play preflight fails if the Play entry scene has no valid Camera.
_Avoid_: Name-only matching as the sole rule; auto-spawning a hidden default Camera on Play; silent black screen with no preflight error when Camera is missing

**Gameplay Input**:
The product-facing input state that Project Behaviours read to drive gameplay (e.g. move, jump). Its authoritative source is the Player process during Play Mode. Outside Play Mode — including Edit Mode and the env-gated editor ScriptHost — Gameplay Input is not a product signal (reads as inactive / unavailable). While **Play Pause** is active, Gameplay Input does not accumulate Action edges for later Tick (a Jump pressed only during Pause is discarded); Move reads as idle when simulation is paused. When the Player window does not have OS focus, Gameplay Input is idle (Move zero, Jump false).
_Avoid_: Treating editor camera shortcuts as Gameplay Input; requiring Edit Mode ScriptHost for gameplay controls; reading the editor window's keys as the product Play path; buffering Pause-time Jump into the first Resume Tick; driving gameplay from global keyboard while the Player is unfocused

**Gameplay Action**:
A named, gameplay-meaningful control Behaviours poll (e.g. move axes, jump) — not a keyboard scancode and not an Editor Command. The first Gameplay Input slice exposes Actions only; physical key/gamepad binding stays engine-side in the Player. The DogWalk-facing starter set is a 2D **Move** axis plus a **Jump** Action. **Jump** is pressed-edge for the simulation frame: every Behaviour that polls Jump in that Tick sees the same true/false (not one-reader-consumes). Behaviours access Gameplay Input through a static `Input` façade on `Blunder.Api`, not via Object properties. Built-in defaults (not user-remappable in this slice): WASD → Move, Space → Jump; opposing keys cancel; diagonal Move is normalized so length ≤ 1. Move's `(x, y)` maps to world **+X (right)** and **+Y (forward)** on the horizontal plane (Z-up); +Z is not part of Move.
_Avoid_: GameCommand (collides with Editor Command; legacy native bitfield name), Input Action as a second product synonym, raw KeyCode as the primary Behaviour API; treating Jump as a held level signal in the first slice; latching Jump so only the first reader sees it; hanging Gameplay Input on Object/ClassDB properties; shipping remappable bindings as required for the first Gameplay Input slice; treating Move.y as world +Z

**Play entry scene**:
The scene asset the Player loads when Play Mode starts — the editor's active scene as already saved on disk (path/GUID), not a live memory clone of the editable SceneInstance.
_Avoid_: Play from unsaved buffer without an explicit save step; Play always using a project default scene unrelated to the active document

**Play dirty prompt**:
When Play is requested and the active scene document is dirty, the editor asks how to proceed: save then Play, Play from the last saved asset, or cancel. Play Mode does not silently discard or silently auto-write authorship edits.
_Avoid_: Always auto-save on Play with no prompt; blocking Play with save-only and no choice; playing the on-disk asset with no indication when the editor view is dirty

**Play Scripts build**:
Before starting the Player, the editor builds the Project `Scripts/` output when those sources (or their build inputs) are newer than the last successful scripts output — otherwise it reuses `.blunder/scripts_bin`. A failed build keeps the session in Edit Mode and does not start Play Mode.
_Avoid_: Building Scripts on every Play with no dirtiness check; requiring the Player process to run `dotnet build`; starting Play against a stale missing assembly with no build attempt when Scripts are dirty

**Play live sync (deferred)**:
Pushing authorship or Scripts changes into a running Play Process without ending the session is out of the first Play Mode UI slice. Authors Stop and Play again to pick up saved scene and rebuilt Scripts.
_Avoid_: Treating ALC or asset hot-reload into a live Player as required for the first Play Mode UI; implying Edit Mode edits appear in Play Mode automatically in this slice

**Play session**:
At most one Play Process for the editor at a time. Stop ends that process (graceful close first, then force if needed). Starting Play while already in Play Mode stops the existing session first.
_Avoid_: Multiple concurrent Players as the v1 default; Stop that only clears UI state while leaving a live Player running

**Edit Mode scripting**:
Normal scene authorship does not start a .NET script host in the editor process. Gameplay Behaviour peers and Tick run in the Player during Play Mode. An env-gated editor host remains a debug/test escape hatch, not the product Play path.
_Avoid_: Requiring `BLUNDER_DOTNET_SCRIPTS` for Play Mode; dual Tick in editor and Player as the default

**Play controls**:
The editor exposes Play, Pause, and Stop for the Play session. Play starts (or resumes) the single Play Process; Pause freezes gameplay simulation while the Player stays alive; Stop ends the Play Process and returns the author to Edit Mode.
_Avoid_: Play-only toggle with no Pause; Pause that exits Play Mode; Stop that leaves the Player process running

**Play Pause**:
While paused, the Player skips gameplay Behaviour Tick (and other gameplay simulation time) but keeps the process and window alive so the author can still view the frozen world through the resolved scene Camera. Resume continues Tick from the paused world state. Pause does not enable Editor Camera orbit or other authorship input in the Player.
_Avoid_: Pause that tears down the Player; Pause that freezes rendering as the only definition; Pause as a synonym for Stop; Pause-time Editor Camera orbit in the Player window

**Play camera preflight**:
Before spawning the Player, the editor verifies the Play entry scene (as it will be loaded — after dirty-prompt save rules) contains at least one valid Camera Component. Failure keeps the session in Edit Mode and surfaces an error (same class of gate as a failed Scripts build).
_Avoid_: Starting Player with no Camera and relying on a black screen; auto-injecting a Camera at Play time

**Play control channel**:
A local IPC link between the editor and the single Play Process used to send session commands (at least pause, resume, and stop). Process exit is also treated as leaving Play Mode. It is not a networked multiplayer protocol.
_Avoid_: Editor Pause with no way to reach the Player; Stop that only kills without a graceful command path; designing the first channel as internet-facing RPC

**Player window close**:
Closing the Player's OS window ends the Play Process and therefore ends Play Mode (same outcome as Stop).
_Avoid_: Closing the window while leaving a headless Player running; tray-minimize as the v1 close behavior

**Edit during Play**:
While a Play session is running, the author may keep editing the Project in the editor. Those edits do not appear in the live Player until a later Play (after save/build rules). Play Mode does not lock the authorship document.
_Avoid_: Freezing the editor for the whole Play session as the v1 rule; implying unsaved editor edits stream into the running Player

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

On-disk layout and cook mechanics: [CONTENT_LAYOUT.md](CONTENT_LAYOUT.md). Decision records: [docs/adr/0012-pull-asset-pipeline.md](docs/adr/0012-pull-asset-pipeline.md) (Pull / three-tier), [docs/adr/0019-gltf-intermediate.md](docs/adr/0019-gltf-intermediate.md) (glTF mesh Intermediate; supersedes [0013](docs/adr/0013-collada-intermediate.md)).

**Source Asset**:
A DCC-native or exchange file archived so Intermediate can be rebuilt (e.g. `.blend`, `.psd`, `.fbx`, `.obj`, `.gltf`, `.glb`). Source Assets alone must be enough to rebuild Intermediate and Final Assets. They are not loaded by the runtime; Cook and Fast Path never read them directly.
_Avoid_: Calling cooked binaries "source", storing only Source under `Assets/`, treating Intermediate glTF as Source archive, collapsing Source and Intermediate into one folder

**Intermediate Asset**:
A lossless (or near-lossless), readable exchange/process form between Source and Final — **mesh** Intermediate bodies are **glTF/GLB** (including skinned meshes); **texture** Intermediate bodies remain ordinary image files (PNG/JPEG/…); **skeletal AnimationClip** Intermediate bodies are readable YAML sidecars extracted at Import (bone TRS keys + interpolation), not relying on COLLADA. YAML Asset Descriptors carry GUID, import settings, and references. Fast Path and Cook consume Intermediate Assets. **COLLADA (`.dae`) is not used** as mesh Intermediate. Final remains platform cooked binaries, not glTF-as-shipped-runtime.
_Avoid_: COLLADA as mesh Intermediate, opaque binary as the only intermediate, pruning/platform-optimizing at this stage, calling the Asset Descriptor YAML “the Intermediate format”, shipping glTF as Final

**Final Asset**:
A platform-optimized, pruned runtime form produced by the Build Process (today: `.blunder/cooked/{guid}.*bin`). The runtime prefers Final Assets when present and fresh. glTF is Intermediate (and often the authored exchange), not Final in v1.
_Avoid_: Hand-editing Final Assets as the source of truth, shipping DCC-native files as Final, treating glTF as the engine Final cooked form

**Assets root**:
The project folder `Assets/` that holds Intermediate **descriptors** (YAML: GUID, import settings, references). It is the Content Browser’s primary tree and the engine’s identity entry for content — not the home for glTF/image Intermediate bodies or Source Assets.
_Avoid_: Storing Intermediate data files or Source Assets under `Assets/`, treating `Assets/` as Unity-style “everything content”

**Resources root**:
The project folder `Resources/` whose non-Source subtree holds Intermediate **data bodies** (glTF/GLB, images, audio, AnimationClip YAML, …) referenced by descriptors. Not scanned as the Content Browser’s primary tree.
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
The Intermediate YAML under the Assets root that persists an Asset’s GUID, type, import settings, and references to Intermediate data (and optional Source). The durable on-disk face of an Asset. Mesh descriptors commonly use a typed suffix such as `.mesh.yaml`; they are not the mesh Intermediate body and are not Unity-style sidecars beside glTF.
_Avoid_: Embedding identity only in the registry with no descriptor file, putting descriptors under Resources, treating the descriptor YAML as the glTF/image body, treating mesh Intermediate as the GUID/meta carrier, requiring Unity-style `.meta` sidecars beside Intermediate files as the product identity model

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
A directed graph of Asset→Asset references used to invalidate Finals and to know what to Cook. v1 minimal edges: Scene→Mesh Asset; Mesh Asset→Texture Asset via the mesh descriptor’s authoritative `texture_guids` (not via paths inside mesh Intermediate alone); textures referenced only by embedded image URIs and not registered as Assets are out of graph; each Asset→its Intermediate inputs (descriptor + `source` file) as leaves for freshness. No Material Asset nodes, audio/shader edges, or Source-file parsing in v1. **Animation Phase 1** adds edges for skinned Mesh↔Skeleton bind data and consumers→AnimationClip Assets (exact edge set recorded when that change lands).
_Avoid_: Hand-maintained file lists, “only this Asset’s own mtime matters” with no cross-Asset invalidation, treating a packaging Manifest as required before the graph exists, inventing Material graph nodes before material descriptors exist, treating embedded image URIs as the canonical Mesh→Texture Asset Reference

**Manifest**:
The set of Assets required by a top-level product unit (e.g. a scene/level), derived by walking the Asset Dependency Graph from that root. Used later for packaging and “what is still needed”; not a separately hand-edited list.
_Avoid_: Manually curated include lists as the source of truth, shipping every file under Resources because Manifest is unknown

**Asset Reference**:
A durable cross-Asset link stored as the target Asset’s GUID (not a filesystem path). Scenes and Mesh→Texture links use Asset References. For meshes, Mesh→Texture Asset References live in the Asset Descriptor (\	exture_guids\); mesh Intermediate may carry ordinary image URIs for interchange/preview but those URIs are not the durable Asset Reference. Paths remain for display and for resolving a GUID via the registry; legacy path-only scene fields migrate to GUID on save.
_Avoid_: Path-as-canonical reference, renaming descriptors without a GUID identity, treating virtual path as the stable public identity of an Asset, embedding Blunder GUIDs inside mesh Intermediate as a second source of truth

**Intermediate Upgrade (v1)**:
The former glTF→COLLADA Intermediate Upgrade is **retired** with COLLADA removal. Any remaining `.dae` Intermediate is migrated back to glTF (GUID-preserving) or reimported from archived Source — procedure lands in the superseding ADR.
_Avoid_: Requiring COLLADA as an upgrade destination, leaving `.dae` as a permanent mesh Intermediate body

**Import**:
The act of registering external content as project Assets: allocate GUID, write an Asset Descriptor, place Intermediate data under the Resources root, and register the Asset. **glTF/GLB** and images are the primary mesh/texture Intermediate path; **COLLADA is removed**. FBX/OBJ may Source-Export into glTF Intermediate when supported. When mesh Import has animations enabled, the engine may attach **Companion Animation glTFs**: near-disk auto-discovery is a convenience; **multi-select Import** (mesh + companion glTFs) is the supported path for split layouts such as DogWalk Chocomel (`assets/char/…` vs `animations/world/…`). Result remains one Mesh + N AnimationClip Assets — no AnimationLibrary. Import is not Cook and is not opening a DCC for manual editing.
_Avoid_: Equating Import with Cook, registering COLLADA as mesh Intermediate, leaving Source files as the only registered form with no Intermediate, calling archived Source the Intermediate body, requiring Godot AnimationLibrary to attach companion clips, hard-coding a DogWalk `animations/world` tree walk as the only discovery mechanism

**Source Export (v1)**:
The Import sub-step that converts a non-Intermediate exchange file into Intermediate data. Mesh Intermediate target is **glTF/GLB** (not COLLADA). On Source Export, the original may be archived under the Source root; Cook and Fast Path consume Intermediate glTF (and images / AnimationClip YAML). **Animation Phase 1** extracts readable AnimationClip YAML sidecars from skinned imports while preserving Constant/Stepped.
_Avoid_: Claiming silent `.blend` export in v1, COLLADA as Source Export target, skipping Intermediate and cooking only archived Source bytes, treating glTF as Final

### Animation (Phase 1+)

**AnimationClip**:
A first-class Asset whose Intermediate body is readable YAML of skeletal TRS keyframes (bone names, times, values, interpolation). Cook produces a Final runtime clip. Distinct from Mesh Asset; not a Behaviour and not the AnimationPlayer. Phase 1 sampler supports **Constant** and **Linear** interpolation (glTF STEP/LINEAR map in); Cubic/Bezier later. Importing a multi-animation glTF — or a mesh glTF plus **Companion Animation glTFs** — yields **one Mesh Asset + N AnimationClip Assets**.
_Avoid_: Embedding durable clip authorship only inside the mesh Intermediate with no Clip Asset, treating clip bytes as Mesh payload, path-only clip identity without GUID, Phase 1 Cubic/Bezier as required, bundling all clips only inside the Mesh Asset with no Clip GUIDs, requiring a Godot-style AnimationLibrary Asset as the container for imported clips

**Companion Animation glTF**:
An authored glTF/GLB that carries skeletal animations for a character but is not the Mesh Intermediate body (Godot-style split exports). **Acceptance:** has animations and **no meshes** (meshes empty / count zero); skins may be present (Chocomel LOOP files ship skins=1, meshes=0). **Discovery:** multi-select Import of mesh + companions is the primary supported path when files live in disconnected trees (e.g. `assets/char/chocomel` vs `animations/world/LOOP-*`); automatic near-disk scan (mesh directory + immediate child dirs of its parent) remains a secondary convenience for co-located packs. In a multi-select batch, exactly one skinned mesh candidate is the host; other glTFs that pass acceptance attach as its companions; multiple skinned meshes each Import on their own and orphan companions are skipped with a warning. Import copies accepted companions under Resources as Intermediate bodies for Reimport re-extract, without registering them as Mesh Assets. Logical clip names prefer the companion file stem. Bone-name mismatch: warn and still register the clip.
_Avoid_: Treating companion files as separate Mesh Assets by default, AnimationLibrary as the companion container, path-only durable clip identity, recursive whole-tree or DogWalk-hardcoded `animations/**` scans as the product default, requiring zero skins to accept a companion, requiring Godot AnimationLibrary paths for companion clip names, failing the whole mesh Import solely because companion bones do not match, extract-only companions with no project Intermediate body when Reimport must refresh clips, skipping companion discovery solely because the mesh glTF already has animations, treating “has animations” alone as enough to attach a neighboring skinned character mesh as a companion, fuzzy filename matching as the required host↔companion pairing rule

**AnimationPlayer**:
The engine-owned playback surface (ClassDB) that samples AnimationClips onto a Skeleton and reports playback time / pose-applied signals for scripts. Phase 1 supports Play/Stop/Loop and hard cuts between clips. **Phase 2** adds a **unified two-slot** model (explicit slot APIs + blend weight; `Play(name, fade)` as sugar), still without an AnimationTree / state-machine graph. Phase 1+: co-located on the same Object as the Skeleton it drives. Clips are addressed by logical string name via a serialized name→AnimationClip Asset Reference map on the player (`Play("LOOP-chocomel-walk")`); GUID is the durable reference inside the map, not the primary script Play key. The map is **auto-filled** from Import (clip names) and remains **editable** in the Inspector afterward. Phase 2 scenes may also persist **defaults**: TimeScale and optional default two-slot clip names + initial blend weight (not a full live playback snapshot). Edit Mode may expose Phase 2 scrub controls (TimeScale / fade / two-slot weights) without running Behaviour Tick.
_Avoid_: Sampling clips only in C# as the product path, requiring an AnimationTree for Phase 1 playback, Phase 1 remote skeleton driving, GUID-only Play as the primary authoring API, read-only auto-filled maps with no Inspector edit, Godot AnimationLibrary (library/clip paths) as the required Play address model, AnimationTree parameter paths as the Phase 2 primary Play API, serializing full playback-head / in-flight Crossfade snapshots as required scene data in Phase 2

**Crossfade**:
A timed transition expressed on the same **two-slot** playback model: one slot is replaced by a newly requested clip while blend weight moves toward that slot over a duration (soft cut). Not a separate graph node or third sample path in Phase 2.
_Avoid_: Calling every weight change a Crossfade, requiring an AnimationTree OneShot for Phase 2 soft cuts, treating Crossfade as a third simultaneous sample track in Phase 2

**Weighted dual-track blend**:
Sampling up to two AnimationClips at once (two slots) and combining their bone poses by a script-driven **single blendWeight ∈ [0,1]** (0 = all slot0, 1 = all slot1), without a BlendSpace or state-machine graph. Weight can change every Tick; Crossfade is the same two-slot machinery with a time-driven weight ramp. Pose combine in Phase 2 is **local TRS blend**: per-bone translation/scale lerp and rotation slerp — not additive/Add2 layers. Scripts drive slots explicitly (`SetSlot` / blend weight); `Play(name, fade)` remains convenience sugar over the same model.
_Avoid_: Equating dual-track blend with BlendSpace1D/2D authorship, requiring N-way blend graphs in Phase 2, maintaining a separate Crossfade pipeline beside the two slots, requiring additive blend layers in Phase 2, Godot AnimationTree parameter-path APIs as the Phase 2 primary surface, dual independent weights as the Phase 2 primary blend API

**TimeScale**:
A single playback-rate multiplier on the AnimationPlayer that scales advance of all active tracks together (dual-track blend and Crossfade progress). Phase 2 does not require per-track TimeScale. Phase 4: when **AnimationTree** is active, the same AnimationPlayer **TimeScale** remains the **global** rate for tree advance (base, OneShot, and Add2 together). Phase 4 does not require per-node TimeScale or a separate Tree-only rate.
_Avoid_: Per-clip TimeScale as a Phase 2 requirement, treating TimeScale as a Behaviour-only float with no engine advance effect, requiring per-node TimeScale as Phase 4 Done, inventing a separate Tree-only rate that ignores AnimationPlayer.TimeScale as the Phase 4 default

**Skeleton**:
The runtime bone hierarchy (rest/bind poses and current pose) that skinned meshes deform against. Pose is advanced by AnimationPlayer and may be overridden after apply by later procedural layers (post–Phase 1). In DogWalk animation Phase 1, Skeleton and AnimationPlayer live on the same Object; cross-Object skeleton references are out of scope.
_Avoid_: Per-vertex animation with no bone hierarchy as the DogWalk character path, treating Object TRS alone as skeletal pose, Phase 1 cross-Object AnimationPlayer→Skeleton links

**Animation step**:
The discrete pose-change boundary in Stepped (on-2s/on-3s) playback that visual systems sync to (e.g. quantized facing). Gameplay motion and input stay real-time; step-synced visuals subscribe to pose-applied timing rather than raw frame rate. Phase 1: engine emits PoseApplied after sampling; C# detects steps (e.g. frametime modulo) — the engine does not define on-2s as a builtin event. Phase 2: under two-slot blend, step detection uses the **dominant slot** playback position (higher weight, or Crossfade target while fading), still advanced under the global TimeScale — not a blended time clock and not a separate engine AnimationStep event. Phase 4: with **AnimationTree** active, PoseApplied still fires after tree sample; step detection uses the **base dominant clip** clock (BlendSpace1D: highest-weight neighbor; StateMachine: current state's clip/dominant point; while OneShot plays: the OneShot clip) — **Add2 does not** supply the step clock; still under global TimeScale.
_Avoid_: Updating facing/blend visuals every render frame as the DogWalk look, conflating physics tick with animation step, requiring engine-builtin AnimationStep events for Phase 1, blending two clip times into one step clock in Phase 2, blending BlendSpace neighbor times into one step clock in Phase 4, using Add2 as the Stepped clock source, dropping PoseApplied while AnimationTree is active

**ValueSlicer**:
A C# gameplay utility that quantizes a continuous value into slices with hysteresis so Stepped visuals do not flicker at boundaries. Not an engine ClassDB type in Phase 1; scripts own DogWalk-style stepping policy.
_Avoid_: Baking ValueSlicer into the engine as the only facing model, overwriting gameplay floats with sliced visuals in place

**DogWalk animation Phase 1**:
The first engine+content milestone after the Move-only DogWalk character slice: P0 skeletal skinning + AnimationClip playback, plus P1 Stepped feel via C# ValueSlicer and Animation step sync — not full AnimationTree, SYNC/CINE, or procedural bone modifiers. Per-frame order: Behaviour Tick (gameplay Play/move) → AnimationPlayer sample → PoseApplied (step-synced visuals). Engineering gate: minimal skinned test rig + idle/walk. **Done criteria** also require Chocomel (or an agreed subset) Play acceptance: idle↔walk hard cut, real-time move, stepped facing. **Edit Mode** may preview AnimationPlayer clips on the Skeleton in the authorship viewport without starting the .NET host or Behaviour Tick; Stepped feel (ValueSlicer) is Play-validated. Phase 2 engine work **may proceed in parallel** with unfinished Phase 1 Chocomel hard-cut acceptance; Phase 1 Done criteria themselves are not rewritten away.
_Avoid_: Shipping full Godot AnimationTree parity as Phase 1, treating Phase 1 as content-only with no engine animation runtime, reading final skeleton pose inside Tick as the supported Phase 1 pattern, declaring Phase 1 done on test-rig-only without Chocomel feel acceptance, Play-Mode-only clip visibility as the Phase 1 editor rule, in-editor Behaviour Tick as required for Edit clip preview, silently dropping Phase 1 Chocomel hard-cut Done when starting Phase 2

**DogWalk animation Phase 2**:
The milestone after Phase 1 that adds a **unified two-slot** playback model (**weighted dual-track blend** via single blendWeight ∈ [0,1] + **Crossfade** as weight ramps; local TRS lerp / rotation slerp) and a **global TimeScale** on the AnimationPlayer — **SYNC/CINE** land in **Phase 3**; still without AnimationTree / state machine / BlendSpace graph, additive layers, or procedural bone modifiers. Phase 1 **hard cut** remains: `Play` with fade duration 0 (or equivalent default) snaps slots/weights immediately; fade > 0 Crossfades on the same model. Animation step sync under blend uses the **dominant slot** clock. **Edit Mode** authorship controls may scrub TimeScale, fade, and two-slot weights without DotNetHost / Behaviour Tick; Stepped facing remains Play-validated. Scenes persist clip name→GUID map plus **defaults** (TimeScale, optional default slots + initial weight). **Done criteria**: engineering gate on test-rig (or equivalent) for two-slot blend, Crossfade, TimeScale, and dominant-slot step sync, **plus** Chocomel (or agreed subset) Play acceptance with weighted idle↔walk (not hard-cut-only), perceptible TimeScale, and stepped facing still on the dominant slot. Engine implementation **may start in parallel** with unfinished Phase 1 Chocomel hard-cut work. Decision record: [ADR 0020](docs/adr/0020-animation-player-two-slot-blend.md).
_Avoid_: Equating Phase 2 with full Godot AnimationTree parity, folding state machines into Phase 2 by default, requiring per-track TimeScale in Phase 2, treating Crossfade as a third sample path beside the two slots, removing hard-cut Play as the Phase 2 default path, requiring additive blend in Phase 2, requiring Edit Mode to match Play Stepped feel as Phase 2 Done, declaring Phase 2 done on test-rig-only without Chocomel blend feel acceptance, folding SYNC/CINE into Phase 2

**Sync Group**:
An engine-owned **runtime** set of **AnimationPlayer** members that can be triggered so members **start (or seek) at the same logical moment**. Created, joined, fired, and released by scripts/API for the session (or until explicitly destroyed) — **not** a required serialized scene graph of members. A **Fire** carries **per-member instructions** `(AnimationPlayer, clipName[, seek])` (heterogeneous clip names are first-class). Same logical name across all members MAY be offered as convenience sugar, not the only path. **Default Fire is a hard cut** (`Play` with fade 0 / equivalent snap) when the member has **no active AnimationTree**; soft Crossfade is not the Sync Group default. **Phase 4:** if that Object's AnimationTree is **active**, Fire applies as a tree **OneShot** for that clip (not by deactivating the tree). Decision record: [ADR 0023](docs/adr/0023-animation-sync-group.md). Used for DogWalk-style multi-Object coordination (character + prop + partner). Not a shared continuous playback head across members, not a cutscene timeline, and not an AnimationTree.
_Avoid_: Relying only on ad-hoc same-Tick `Play` calls as the supported SYNC product path, a single shared sample clock across all members as the Phase 3 default, treating Sync Group as a full sequencer/director, requiring a baked scene-component member list as the only way to form a group, making one AnimationPlayer a permanent master with followers as the Phase 3 model, requiring every synced Object to share one identical clip logical name, making a shared Crossfade fade the default Sync Group Fire path, requiring Fire to deactivate AnimationTree as the default Phase 4 path

**SYNC**:
The Phase 3 authorship concern for **coordinated multi-Object AnimationPlayer starts** via a **Sync Group** (clip names may use a `SYNC-` prefix by convention). Distinct from **CINE** (over-scene / handoff clips) and from Animation step / Stepped facing sync.
_Avoid_: Equating SYNC with Stepped pose sync, requiring AnimationTree for multi-Object starts, treating `SYNC-` filename prefix alone as an engine feature

**CINE**:
A Phase 3 **cinematic segment** contract: a short authored takeover (often prop/partner clips named `CINE-*`) that **enters** with pose/control handoff and **exits** returning control to gameplay. Engine provides thin start/end hooks plus optional **in-CINE** marking / gameplay-input suppression; **pose alignment and gameplay state transitions stay in C# Behaviours**. **Segment end is authoritative via an explicit End API** (scripts call end); member `finished` signals may assist but do not alone define exit. Playback still uses AnimationPlayer (+ **Sync Group** when multiple Objects must start together). Not a multi-track cutscene director.
_Avoid_: Equating CINE with a full Cutscene Director / timeline editor, treating `CINE-` prefix alone as an engine type, requiring AnimationTree OneShot for every CINE segment, baking DogWalk state machines or automatic TRS snap/restore into the engine as the Phase 3 path, requiring “all members finished” or a fixed wall-clock duration as the only way to end a CINE segment

**DogWalk animation Phase 3**:
The milestone after Phase 2 that adds **SYNC** (**Sync Group** runtime coordination of multi-Object AnimationPlayer starts) and **CINE** (short cinematic segment contract with thin enter/exit hooks and optional gameplay-input suppression). Still without AnimationTree / state machine / BlendSpace graph, additive layers, procedural bone modifiers, or a full cutscene director. Phase 1 co-located Skeleton↔AnimationPlayer and Phase 2 two-slot blend / TimeScale remain. **Edit Mode** supports Sync Group Fire and CINE Enter/End preview: authors can see **in-CINE** / input-suppression marking and multi-Skeleton playback **without** DotNetHost / Behaviour Tick, and **without** automatic Object TRS or gameplay state-machine handoff (those remain Play / C#). **Done criteria**: engineering gate for Sync Group Fire (per-member clip instructions) + CINE in/out hooks, **plus** a DogWalk-style mini Play acceptance (character + prop/partner synchronized start and CINE handoff returning control — simplified props OK). Engine work **may proceed in parallel** with unfinished Phase 2 Chocomel weighted acceptance; Phase 2 Done criteria are not cancelled. Decision record: [ADR 0023](docs/adr/0023-animation-sync-group.md).
_Avoid_: Shipping AnimationTree / BlendSpace as Phase 3, treating SYNC as Stepped facing sync, requiring full Chocomel/Pinda ending sequences as the only acceptance bar, declaring Phase 3 done on Sync Group unit tests alone without the mini multi-Object Play bar, silently dropping Phase 2 weighted Chocomel Done when starting Phase 3, Play-only SYNC/CINE with no Edit multi-Object preview path, requiring Edit Mode to auto-snap Object TRS or run gameplay state machines for CINE preview

**AnimationTree**:
An engine-owned ClassDB **playback graph** (Phase 4) co-located on the same Object as **AnimationPlayer** and **Skeleton**. It resolves clips via the player's logical name→GUID map and samples a lean node set (**StateMachine**, **BlendSpace1D**, **OneShot**, **Add2**) onto the Skeleton. **Sample stack:** a **base** pose from the StateMachine (state may be a BlendSpace1D or single clip; OneShot may temporarily replace/insert over that base) then optional **Add2** layers on top. Additive deltas are relative to **bind/rest**, not Phase 2 dual-track lerp/slerp. Scripts drive the tree through a **narrow named API** (`Travel` / `Start`, per-node BlendSpace1D scalar by **node logical name**, `RequestOneShot`, Add2 weight/clip setters) — not Godot-style `parameters/…` path strings as the Phase 4 primary surface. **Graph topology is scene-embedded** on the AnimationTree (states, BlendSpace points by clip logical name + scalar, OneShot/Add2 slots); Phase 4 does **not** require a visual node graph editor or a separate AnimationTree Asset. **Edit Mode** may activate the tree and scrub those named drives (plus TimeScale) without DotNetHost / Behaviour Tick. When the tree is **active**, it **exclusively** owns Skeleton sampling for that Object; AnimationPlayer Play / two-slot / Crossfade do not write bones until the tree is inactive (Phase 1–3 path resumes). A Sync Group **Fire** on an active-tree member is honored **through the tree** as a **OneShot** insert; members without an active tree keep Phase 3 hard-cut `Play`. Fire→arbitrary StateMachine `travel` is optional sugar, not the Phase 4 default Fire path. Distinct from AnimationPlayer (clip bank + two-slot Play/Crossfade/TimeScale). Not full Godot AnimationTree parity (no required BlendSpace2D, nested everything, or AnimationLibrary). Decision record: [ADR 0025](docs/adr/0025-animation-tree-phase-4.md).
_Avoid_: Hosting the graph only inside AnimationPlayer, C#-only graph topology as the product path, AnimationLibrary as the graph host, requiring an AnimationTree on every animated Object, using AnimationGraph as a second product name for the same type, letting active Tree and Player two-slot both write the Skeleton in one frame, feeding Tree output into Player slots as the Phase 4 product path, deactivating the tree as the default Sync Fire path, failing Fire solely because the member's tree is active, treating Add2 as another lerp dual-track slot, requiring arbitrary-deep node nesting as Phase 4 Done, Godot AnimationTree parameter-path APIs as the Phase 4 primary script surface, a single anonymous global blend float with no node name when multiple BlendSpace1D nodes exist, requiring a visual AnimationTree editor or a standalone AnimationTree Asset as Phase 4 Done

**BlendSpace1D**:
A Phase 4 **AnimationTree** node that blends among discrete authored clip points along **one** scalar parameter (DogWalk: speed-like walk/trot/run). Neighbor blends use the same local TRS lerp / rotation slerp family as Phase 2 — not additive. Points reference clips by **logical name** on the co-located AnimationPlayer map. Not BlendSpace2D.
_Avoid_: Equating Phase 2 two-slot weight with BlendSpace1D authorship, requiring BlendSpace2D in Phase 4, using Add2 as the speed blend mechanism, requiring a standalone BlendSpace Asset as Phase 4 Done

**StateMachine** (animation):
A Phase 4 **AnimationTree** node that selects among **named animation states** (each state's playback is a BlendSpace1D or a single clip) via script **travel** / **start** (queue optional later). Distinct from the gameplay / character state machine in C# Behaviours.
_Avoid_: Merging gameplay state machine into the AnimationTree as one type, requiring Godot-complete transition graphs and nested state machines as Phase 4 Done

**OneShot**:
A Phase 4 **AnimationTree** node that **inserts** a clip (or short sub-pose) over the base graph, then **returns** to the underlying StateMachine/BlendSpace base. Used for trip-like interrupts and as the default Sync Group **Fire** path into an **active** AnimationTree.
_Avoid_: Equating every Crossfade with OneShot, requiring AnimationPlayer fade as the only interrupt path, treating CINE Enter as an engine OneShot

**Add2**:
A Phase 4 **AnimationTree** additive layer that combines an additive clip/pose onto the **base** pose. Additive deltas are relative to **bind/rest**. Used for DogWalk-style turn / bark overlays on locomotion. Not a third Phase 2 sample slot and not BlendSpace.
_Avoid_: Calling Phase 2 dual-track blend Add2, requiring N independent additive buses as Phase 4 Done, using world-space additive as the Phase 4 default reference

**DogWalk animation Phase 4**:
The milestone after Phase 3 that adds **AnimationTree** with a lean locomotion graph: **BlendSpace1D**, **StateMachine** (with **OneShot**), and **Add2** — sample order **base then additive (bind/rest)**; scripts use the **narrow named API** (per-node BlendSpace scalars); **topology is scene-embedded** (no required visual graph editor / Tree Asset). Animation step under an active tree uses the **base dominant clip** clock (PoseApplied retained; Add2 is not the step source). Active tree advance uses AnimationPlayer **TimeScale** globally. **Edit Mode** may activate the tree and scrub named drives (BlendSpace scalars, Travel/Start, OneShot, Add2 weights, TimeScale) without DotNetHost / Behaviour Tick; Stepped feel remains Play-validated. **Done criteria:** engineering gate (travel, BlendSpace1D, OneShot including Sync Fire→active tree, Add2 overlay, exclusive sampling, PoseApplied/dominant clock, TimeScale, Edit scrub) **plus** Chocomel (or agreed subset) Play acceptance — perceptible speed-like BlendSpace motion, **visible additive turn** (exact Godot turn clip names not required), OneShot return-to-base, stepped facing still on the base dominant clock. Still without requiring BlendSpace2D, procedural bone modifiers, method/audio tracks, Cubic/Bezier, cross-Object Skeleton drive, AnimationLibrary, or a full cutscene director. When Tree is active it exclusively samples the Skeleton; inactive falls back to AnimationPlayer; Sync Fire on active-tree members uses **OneShot**. Phase 1–3 foundations remain available; engine work **may proceed in parallel** with unfinished Phase 1–3 content gates (those Done criteria are not cancelled). Decision record: [ADR 0025](docs/adr/0025-animation-tree-phase-4.md).
_Avoid_: Shipping full Godot AnimationTree parity as Phase 4, treating Phase 4 as BlendSpace2D/Pinda-complete, folding procedural SkeletonModifier or method/audio tracks into Phase 4 by default, silently dropping Phase 1–3 Chocomel/SYNC content gates when starting Phase 4, growing Phase 1–3 Play APIs into the graph host instead of AnimationTree, dual-writing Skeleton from active Tree and Player two-slot, requiring Fire→travel as the only Sync path into a tree, arbitrary-deep graph nesting as Phase 4 Done, parameter-path strings as the Phase 4 primary drive API, requiring a visual AnimationTree editor or standalone Tree Asset as Phase 4 Done, dropping PoseApplied or Stepped sync while AnimationTree is active, requiring per-node TimeScale as Phase 4 Done, Play-only AnimationTree with no Edit scrub path, requiring Edit Mode Behaviour Tick to drive the tree, declaring Phase 4 done on engineering gate alone without Chocomel-subset Play feel, requiring byte-identical Godot turn clip naming for Add2 acceptance

**Skinning path (Phase 1)**:
How bind pose + weights turn Skeleton pose into deformed vertices. **Fast Path** (Intermediate) uses **CPU skinning**; **Final** (Cooked) uses **GPU skinning**. Editor after Cook and Player share the Final/GPU path; uncooked preview stays CPU.
_Avoid_: Editor-always-CPU as the product rule, requiring GPU skin before first Intermediate preview, maintaining a third unrelated skinning backend

**Scene Asset**:
A first-class Asset for a level/scene document. Persisted as `.scene.asset` JSON with a GUID field; registered like other Assets. Entity mesh links are Asset References (GUID). It is a root for walking the Asset Dependency Graph and (later) Manifests.
_Avoid_: Path-only scene identity, treating scenes as non-Assets outside the registry, omitting GUID from scene documents

**Reimport**:
Re-running the Import/Source Export path for an existing Asset to refresh Intermediate from archived Source (when present) or to re-apply import settings to existing Intermediate, then invalidating Finals and dependents. Distinct from Cook.
_Avoid_: Equating Reimport with Cook, requiring delete-and-re-import to pick up Source changes

**Asset Watch**:
Editor file watching that drives Pull freshness: the Assets root and Intermediate data under the Resources root. Changes invalidate Finals (and dependents via the Asset Dependency Graph). Changes under the Source root trigger automatic Reimport for Assets that archive that Source.
_Avoid_: Watching only Assets for browser refresh with no Cook invalidation, ignoring Intermediate/Source mutations on disk
