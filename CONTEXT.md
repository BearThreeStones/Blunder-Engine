# Blunder Engine

Editor and runtime for a Z-up, glTF-aligned 3D engine with Blender-inspired viewport gizmos.

## Language

### Reflection & scripting

**ClassDB**:
The single runtime type database for engine-exposed classes, properties, and methods. It is the source of truth for both editor tooling (Inspector, serialization) and script bindings.
_Avoid_: Separate editor reflection vs script reflection, dual metadata stores, ad-hoc per-UI property maps

**C-ABI bridge**:
The stable, symbol-free C calling surface that exports ClassDB operations to other languages. Script runtimes talk only through this bridge, never via C++ class layouts or member pointers.
_Avoid_: Direct P/Invoke of C++ methods, exposing C++ member pointers to C#

**Object**:
The identity-layer citizen of ClassDB: naming, hierarchy, lifecycle hooks, serialization entry, and the attachment point for a Script Peer. Objects are not the bulk storage for hot simulation data.
_Avoid_: Making every ECS component an Object, Godot-style everything-is-Node as the only model

**ECS World**:
The data-oriented store of entities and components, plus the systems that query and update them (transform, rendering prep, physics, etc.).
_Avoid_: Script virtual dispatch inside ECS systems, storing GCHandles on components

**Entity (ECS)**:
An ID in an ECS World that owns a set of components. Distinct from today's `Entity` C++ class (TRS + name POD), which will be split/renamed as the dual track lands.
_Avoid_: Using "Entity" to mean a scriptable Object, conflating with Object

**Component (ECS)**:
A data blob (and optional native behavior) stored in the ECS World. Components may be described in ClassDB as data schemas for editor/serialization; they are not Script Peer hosts.
_Avoid_: Component-as-Object, per-component C# GCHandle

**Script Peer**:
The language-side twin of an Object (e.g. a C# instance + GCHandle). Bound weakly through the Object; torn down and rebound on script hot reload while the Object and its ECS entity remain. An Object has at most one Script Peer.
_Avoid_: Strong C++ references to managed types, storing managed delegates as the sole identity of an Object, multiple Script Peers per Object, GCHandles on ECS components

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
Engine-owned callbacks such as Ready/Tick that may be implemented by a Script Peer. Hooks are registered per exported type (replaceable dispatch table) and invoked via PtrCall on Objects that have a Script Peer — not per-instance native function pointers.
_Avoid_: Per-Object TickDelegate fields, unmanaged C# doing its own full world walk as the only tick path

**Reflection kernel**:
The first deliverable of the reflection system: ClassDB, Clang-driven export for a narrow set of types, PtrCall plus a small Variant path, API Blueprint, Object/`ObjectId`, projected property accessors, and a C-ABI skeleton — without shipping a .NET host, ALC hot reload, or a full ECS rewrite of `SceneInstance`.
_Avoid_: Treating C# hot reload or full ECS migration as part of the first reflection milestone

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
The undo/redo stack for editor authorship of open documents (scene edits, and later other document types). It does not record runtime gameplay or Play-mode simulation steps.
_Avoid_: Engine-wide action history shared with gameplay, treating script Tick as undoable

**Editor Command**:
A single reversible unit on the Editor History. It exposes undo and redo. Continuous interactions (e.g. a Translate Modal Session) become one Command at confirm — not one Command per pointer move.
_Avoid_: Per-frame history entries, full-scene snapshot as the default history unit

**Document History**:
The Editor History owned by one open editable document — for v1, the active scene (`activeScenePath` / active `SceneInstance`). Opening another scene replaces or clears that history; it is not a global stack across unrelated documents.
_Avoid_: Cross-scene undo continuum, surviving history after the document instance is unloaded

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
Undo/Redo are reachable via editor shortcuts (Ctrl+Z; Redo accepts Ctrl+Y and Ctrl+Shift+Z) and Edit menu items, enabled from Document History canUndo/canRedo. The same `EditorHistory` API backs both paths.
_Avoid_: API-only milestone with no user-facing undo; menu-only without shortcuts

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
