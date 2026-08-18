# Blunder Engine

Editor and runtime for a Z-up, glTF-aligned 3D engine with Blender-inspired viewport gizmos.

## Language

### Engine composition

**Privileged core**:
The non-replaceable, non-unloadable substrate of a running engine process: ClassDB, the C-ABI bridge, the RHI (offscreen Vulkan presented through Slint), and Object–Entity binding. Extensions compose beside it; they do not replace it or hot-unload it. Decision record: [ADR 0035](docs/adr/0035-first-party-seams-beside-privileged-core.md).
_Avoid_: Core (the README conceptual layer: log, events, math, memory, LayerStack), Physics Kernel, Reflection kernel, treating every `RuntimeGlobalContext` field as privileged, Cordis / DeepSeek Harness “no privileged core”, everything-is-plugin including ClassDB or RHI

**First-party composition**:
Only the engine repository ships code that mounts beside the Privileged core. That code is statically linked into the engine (typically `engine_runtime`). Project Behaviours remain gameplay; they are not engine modules. There is no third-party native plugin ABI and no C# editor-script composition path.
_Avoid_: Plugin DLL, GDExtension, Unity native plugin, marketplace modules, treating Behaviour as a System, C# editor scripts as the engine composition mechanism, Plugin as the name of a System or Seam registration

**System**:
A process-lifetime first-party object that owns a capability (for example RenderSystem, AssetImportService, DocumentHistory). Started at process boot according to Host composition; torn down only at process shutdown. Not a Layer, not a Behaviour, and not unloadable mid-session.
_Avoid_: Plugin, treating Layer as a System, treating Behaviour as a System, mid-session unload of a System, Cordis component

**Host composition**:
Which Systems start in a given process. Today that is Editor Session versus Player (`EngineHostMode`). Changing composition means starting a new process, not remounting Systems in a live session.
_Avoid_: Cordis profile / bundle / patch YAML as the first composition format, hot-swapping Host composition inside a live process, treating Play Mode as a Host composition (Play Mode is a session; Player is the host)

**Seam**:
A declared extension point beside the Privileged core with three roles: a definition (the interface), providers (implementations), and at least one consumer (the System that uses them). One role alone is not a Seam. Adding a first-party capability means designing all three. The first Seam to cut is the **SkeletonModifier type catalog**; Import codec is the second. Editor Command and Add… Unique attachments are not Seams.
_Avoid_: Ad-hoc switch statements as the lasting extension path, calling a single helper a Seam, treating ClassDB itself as a Seam (ClassDB is Privileged core), treating LayerStack as a Seam, an Editor Command type registry, listing Unique attachments as Seam registrations

**Seam registration**:
One provider or intercepting listener installed on a Seam, with a disposer that fully reverts that install. Tests and feature flags unregister this way. Unregistering does not tear down the owning System.
_Avoid_: Unloading a System to remove one importer, reversible teardown of the Privileged core, using Plugin or Cordis effect as the product term

**Context System**:
A System looked up from the process-wide runtime context (`RuntimeGlobalContext`). It hosts the Privileged core, or every shipped Host composition starts it (Editor Session and Player). AssetCompiler is a Context System because both hosts cook-if-stale. Torn down only at process shutdown.
_Avoid_: Putting Editor-only authorship Systems here as the lasting path, treating every current GlobalContext field as a Context System, moving AssetCompiler to the registry while Player still cooks at boot

**Registered System**:
A System that at least one shipped Host composition omits. Editor-only authorship belongs here — Content Browser, Selection, Hierarchy, Scene Edit, Document History, Viewport Pick, Placement Preview, Animation Preview, Play Session, thumbnail/preview render services, Asset Import, UiHost, Slint, viewport sink/bridge. Player must not create them. Mounted at process boot via a registry; callers tolerate absence. Still process-lifetime — not a Plugin and not a Seam registration.
_Avoid_: Plugin, unloading a Registered System while the process runs, conflating Registered System with Seam registration, creating Content Browser or Import inside the Player, Cordis ctx keys for the Privileged core

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

**Add…**:
The Inspector picker that lists authorable attachments for the current selection — ECS Components, ClassDB members on the Object, and Behaviours. It is an editor authorship gesture, not a ClassDB type, not an ECS Component, and not Unity Add Component. First slice: Unique attachments (Camera, Skeleton, AnimationPlayer, AnimationTree); Behaviour types from the Behaviour type catalog; SkeletonModifier types. Mesh stays Content Browser spawn. First slice requires exactly one selected entity. The popup is a grouped flat list (Unique attachments, Behaviours, Skeleton Modifiers) with no search; an empty Behaviour catalog still shows that group with the build-Scripts hint.
_Avoid_: Add Component, Add Node, treating the menu itself as a runtime type; keeping parallel Add Camera / Add Behaviour / Add Skeleton Modifier buttons as the product path; multi-select Add… as the first slice; nested submenus or type-ahead search as first-slice scope

**Unique attachment**:
An Add… item that may exist at most once on the selected Object or entity: Camera, Skeleton, AnimationPlayer, AnimationTree. When already present, the row stays visible and disabled.
_Avoid_: Hiding unique items from Add…; treating Behaviours or SkeletonModifiers as unique; allowing a second Camera / Player / Tree / Skeleton on the same selection

**Add… host cascade**:
Add… creates missing co-located animation hosts on the same Object: AnimationTree implies AnimationPlayer implies Skeleton. Adding Skeleton does not create Player or Tree. Import still does not fill the AnimationPlayer clip map. A newly added AnimationTree is empty and inactive, with no AnimationTree Asset GUID required.
_Avoid_: Cross-Object cascade from Add…; auto-filling clips when adding Player; creating Player because the author only added Skeleton; requiring a Tree Asset to Add… AnimationTree; activating a new Tree so it blocks Player sampling by default

**Add clip**:
Inspector control on the AnimationPlayer section that appends one empty name→GUID row to the player's clip map. Not an Add… item. Content Browser drop onto that row is later, not the first slice.
_Avoid_: Listing clips in Add…; requiring a drag-from-browser to create the first row; Import auto-fill as the only way to create a row

**Remove attachment**:
Inspector removal of an authored attachment. Unique attachments have a section Remove; Behaviours, SkeletonModifiers, and clip rows keep per-row Remove. Removing Tree or Player does not cascade to Skeleton. Remove Skeleton is disabled while Player, Tree, or any SkeletonModifier remains.
_Avoid_: Reverse cascade that deletes Player/Tree/Modifiers because Skeleton was removed; Unique attachments removable only via scene JSON; clip rows that can be added but not deleted

**Add… Object materialization**:
Add… creates a bound Object for ClassDB-hosted attachments (Skeleton, AnimationPlayer, AnimationTree, Behaviour, SkeletonModifier) when the selected entity has none. Camera does not create an Object.
_Avoid_: Creating an Object because Camera was added; requiring a pre-existing Object before Add…; using Add… to force 1:1 Object↔Entity

**Skeleton hydration**:
Filling a Skeleton's rest/bind from the selected entity's skinned mesh Intermediate glTF onto the same Object when Add… creates that Skeleton. A static mesh or failed read still yields an empty Skeleton (warn, do not fail the Add). Add… does not expand a glTF child hierarchy.
_Avoid_: A separate Skeleton Asset as a prerequisite for Add…; empty-only Skeleton as the skinned Add Player path; using Add Skeleton to spawn importUnderEntity children

**Add… command**:
One Editor Command per Add… click, Remove attachment, Add clip, or clip-row Remove, on Document History. Host cascade and Skeleton hydration belong to that same Command, not separate undo steps.
_Avoid_: Silent non-undoable Add…; one undo step per cascaded host; stuffing Add… into Global History

**Inspector Behaviour UX**:
Edit Mode authoring of an entity's Behaviour list and property bag in the Inspector. The authoritative Edit Mode surface is the scene **Behaviour declaration** (ordered type + BehaviourId + bag), not a live Script Peer. Mounting peers remains a Play/Player (or env-gated debug host) step, not a prerequisite for Inspector edits. Available types for Add come from a **Behaviour type catalog** produced by scanning the Project Scripts assembly after `dotnet build` (metadata only; no DotNetHost required). The first UX slice edits only **bool / number / string** public instance fields or properties — the same narrow bag already persisted and applied on mount. Add, Remove, reorder (drag), and property commits (Enter / focus loss) are Document History Commands addressed by `EntityId`, same interaction-boundary sealing as Transform. A declaration whose type is missing from the catalog stays in the list as a visible missing/broken entry (remove allowed; not auto-deleted).
_Avoid_: Requiring editor DotNetHost for normal Behaviour authoring; treating live peers as the Edit Mode source of truth; conflating Add/Remove Behaviour UI with AttachBehaviour mount; hand-typed CLR names as the primary Add path; starting CoreCLR solely to populate the Add list; shipping Vec3/enum/nested/asset-reference Behaviour editors in the first Inspector Behaviour UX slice; silent non-undoable Behaviour edits in that slice; silently dropping missing-type declarations; blocking Save/Play solely because a Behaviour type is missing from the catalog; treating list order as non-authoritative relative to Ready/Tick order

**Behaviour type catalog**:
An editor-facing list of concrete non-abstract Behaviour CLR types discovered from the Project's built Scripts assembly metadata after `dotnet build`. Each entry includes the type's public bool/number/string instance members so Inspector can draw a full property form without a live peer. The catalog refreshes after a successful Scripts build (same path as the Play Scripts gate); opening Add with a missing catalog prompts or triggers that build. Used by Inspector Add Behaviour and property rows; not the live Script Peer table and not ClassDB.
_Avoid_: Runtime PeerTable as the type picker source; ClassDB registration of game Behaviours; requiring AttachBehaviour to discover types; showing only keys already present in the scene property bag as the primary form; free-form bag keys unbound from CLR members as the primary authoring path; rescanning the assembly on every Add-menu open as the primary refresh policy; manual-only catalog refresh with no build hook

**Scene Tree**:
The authored parent/child hierarchy of Objects. Outline, reparenting, and naming walk this tree. When an Object has an ECS Entity, transform parenting is projected from the Scene Tree into the ECS World — the tree is not stored twice.
_Avoid_: Dual-written parents on Object and ECS, ECS Parent as the editor-facing tree of record

**Hierarchy Panel**:
The editor chrome that lists the visible Scene Tree for selection, expand/collapse, and naming. The scene display name is panel chrome above the tree, not a tree parent. A pointer down on a visible row (including the Hierarchy Line gutter) selects that entity; a pointer down on the expand chevron of a row with children toggles expand.
_Avoid_: Outliner as the product name, calling the panel the Scene Tree, treating the scene title as a Scene Tree root, treating the Hierarchy Line as a separate control or reparent handle

**Hierarchy Line**:
The parent/child guide in the Hierarchy Panel gutter: a vertical stem under an expanded parent and a horizontal tick to each visible child's expand-chevron column (the slot stays empty when the row has no children). Nested expanded rows draw their own stem. A parent's stem ends at its last visible child; it does not run through grandchildren. Same-depth names share one left edge. Root entity rows have no incoming line. The line is quieter than expand chevrons and row names, and keeps that color on a selected row. Visual chrome, not Scene Tree storage.
_Avoid_: Outliner connector, Scene Tree line, indent-only as this decoration, treating the guide as authored scene data, requiring the same guides in the Content Browser as this term, shifting a leaf label left because it has no chevron, drawing a stem from the scene title to root entities, matching selection blue or glowing on select

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

### Physics

**Physics Kernel**:
The engine-owned rigid-body simulation core (integration, contact generation, stacking, sleep). Authored and stepped in native code; verified by the Physics golden suite and module unit tests. Built for **lockstep-grade determinism** (cross-platform bit/checksum identity as a product contract). Distinct from later gameplay-facing character / controller APIs and from C# Message as a physics deliverable. Decision record: [ADR 0028](docs/adr/0028-physics-kernel-fixedpoint-lockstep.md).
_Avoid_: CharacterController as the kernel, shipping game Messages or Behaviour APIs as the first physics milestone, treating editor eyeballing as the sole correctness gate, adopting a third-party solver as the long-term source of truth for Blunder physics, treating same-machine-only repeatability as enough for networked physics

**Physics Kernel v0**:
The first closed Physics Kernel slice: 3D rigid bodies (Dynamic / Static / Kinematic), gravity/forces, convex colliders (box / sphere / capsule), per-Collider materials (friction + restitution 0 default), resting contact and stacking, per-body sleep. Explicitly out of v0: CCD, joints/constraints beyond contact, island sleep, triangle meshes / heightfields, soft bodies, cloth, vehicles, 2D, character-controller APIs, and SceneInstance/ECS/Object bridges.
_Avoid_: Bundling CCD/joints/meshes/character/island-sleep/scene-bridge into the first kernel closed loop, declaring v0 done on visual sandbox alone

**Physics World**:
The authoritative native container for Physics Kernel state (bodies, colliders, velocities, contacts). The golden suite drives and asserts against this World directly. Projection into `SceneInstance` / a future ECS World / Object APIs is a later bridge milestone — not part of Kernel v0 closure.
_Avoid_: Making SceneInstance Entity TRS or Object nodes the v0 source of truth for body state, requiring ECS World migration before the first golden suite can run, Godot-style scene-tree nodes as the kernel authority

**Physics step**:
One fixed-timestep advance of a Physics World (`dt = 1/60` by default). The golden suite calls `step` a fixed number of times; variable frame-rate accumulators belong to a later host/bridge, not Kernel v0.
_Avoid_: Variable `delta_time` as the v0 integration input, unbounded substeps that chase frame time inside the World, baking editor/player frame pacing into the kernel API

**Physics units**:
Physics World quantities use SI: metres, seconds, kilograms. Default gravity is `(0, 0, -9.81)` in Z-up world space; a World may override gravity. Matches engine right-handed Z-up space. Stored and stepped as **Physics fixed scalar** (Q32.32), not float.
_Avoid_: Centimetres as the default length unit, unspecified units with ad-hoc golden tolerances, Y-up gravity inside the Physics World, float storage as the kernel source of truth under lockstep

**RigidBody**:
A Physics World body with pose, velocity, mass properties, and a motion type. Owns zero or more Colliders. Not an Object, not a SceneInstance Entity, and not a gameplay CharacterController.
_Avoid_: Equating RigidBody with a scene node or Object, one-shape-only forever as the data model, treating Behaviour as the body authority

**Collider**:
A collision shape (v0: box / sphere / capsule) attached to a RigidBody, with material parameters used in contact. Multiple Colliders may attach to one RigidBody.
_Avoid_: Collider as a standalone simulated body, requiring a scene-tree CollisionShape node as kernel authority

**Physics material**:
Per-Collider contact parameters for Kernel v0: friction (at least one usable μ model) and restitution. Contact between two Colliders combines materials with a simple rule (exact combine function is an implementation detail). Defaults favor stacking: moderate friction, restitution 0. Bouncy defaults and bounce goldens are out of v0 closure.
_Avoid_: World-only global friction as the only model, frictionless v0 that fakes stacking via position correction alone, default restitution > 0 as the v0 stacking path

**Body motion type**:
How a RigidBody participates in the simulation: **Dynamic** (forces/impulses, integrated), **Static** (immovable, infinite mass), **Kinematic** (external `target_pose`; each Physics step derives linear/angular velocity from the pose delta, uses that velocity in contact so moving Kinematics can push Dynamics, then ends the step at `target_pose`). All three are in Kernel v0; a moving-platform-pushes-box case belongs in the Physics golden suite.
_Avoid_: Fake Static via huge-mass Dynamics, teleport-only Kinematic with zero velocity as the v0 product rule, velocity-only Kinematic that cannot accept a target pose, shipping a Kinematic type without a push golden, conflating Kinematic with CharacterController API

**Body sleep**:
A Dynamic RigidBody may enter sleep when linear and angular speed stay below thresholds for N consecutive Physics steps; contacts or applied forces wake it. Static and Kinematic bodies do not sleep. Kernel v0 uses per-body sleep (S1); island-wide sleep/wake is a follow-on kernel milestone, not required to close v0.
_Avoid_: No-sleep v0 that only hopes velocities stay small, timer-forced freeze as sleep, requiring island sleep before v0 golden closure

**Physics determinism (v0)**:
Kernel v0 is **lockstep-grade deterministic**: same World inputs and same Physics step sequence produce bit-identical simulation state across the supported platform/compiler matrix. Stepping is single-threaded with a frozen, order-stable contact/solve pipeline. Multithreading is allowed later only if it preserves that contract. The simulation scalar is **Q32.32 fixed-point** (`int64` integer/fractional split), not IEEE float, on the physics hot path — transcendental/sqrt helpers used by physics are engine-owned fixed-point routines (no platform `libm` on that path). **v0 Done requires the Physics golden suite to pass with bit-identical World state on Windows MSVC x64 and Linux Clang or GCC x64 (P1).**
_Avoid_: Treating same-build flaky-tolerant floats as the product bar, multithreaded v0 that relaxes order for speed, deferring cross-platform determinism until after netcode lands, declaring lockstep physics ready on float or single-platform-only goldens, Q22.10 as the primary physics scalar, mixing unchecked float into Kernel step

**Physics fixed scalar**:
The Physics Kernel's numeric type: binary fixed-point **Q32.32** stored in a signed 64-bit integer (32 integer bits, 32 fractional bits). World quantities remain SI conceptually (metres, seconds, kilograms) but are represented and computed in this scalar. Golden assertions compare in the fixed domain (or values derived from it without nondeterministic float).
_Avoid_: Float as the kernel scalar under a lockstep contract, per-module ad-hoc Q formats without a single physics scalar, lookup-table math that must be regenerated on every format tweak as the primary approach

**Fixed math library**:
An engine-owned Q32.32 math module (scalars, vectors, quaternions, and the transcendentals/sqrt physics needs) used by the Physics Kernel. Independent of float `glm` / platform `libm` on the simulation path. Kernel v0 consumes it for physics only (**Scope1**); the module is packaged for later lockstep gameplay reuse (**Scope2** reserve). Scene bridge converts fixed poses to float for `SceneInstance` / rendering.
_Avoid_: Physics-private one-off fixed helpers that cannot be reused by future lockstep gameplay, float glm as a dependency of the simulation math path, converting to float mid-step for "just this solve"

**Physics scene bridge**:
The first post–Kernel-v0 milestone: project Physics World body poses into `SceneInstance` and/or a future ECS Transform path so simulation is visible/usable in the engine host. Still not CharacterController or full gameplay physics API. Follows v0 golden closure; precedes treating multithreading / CCD / joints / meshes as the next top priority by default.
_Avoid_: Bundling the bridge into Kernel v0 closure, equating the bridge with Character API, requiring multithreaded solve before any scene projection

**Physics golden suite**:
A fixed set of scenarios with numeric assertions that gate Physics Kernel changes. Primary acceptance for the kernel; module unit tests cover isolated algorithms (e.g. GJK, contact solve step). Optional early calibration against another engine is allowed; that engine is not the lasting truth source. **Kernel v0 required scenarios:** (1) free fall under gravity; (2) Dynamic resting on Static; (3) stack of ≥3 boxes; (4) sphere–box and capsule–box resting contacts; (5) frictional incline; (6) Kinematic platform lifts/pushes a Dynamic box; (7) impact against Static with bounded kinetic energy at restitution 0; (8) sleeping Dynamic wakes on hit. Bounce, joints, CCD, triangle meshes, and compound-shape stress are out of the v0 required set.
_Avoid_: Editor visual inspection as the only gate, locking correctness forever to Jolt/Bullet/PhysX reference dumps, declaring v0 closed without the eight required scenarios

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
The cached still image shown for a Content Browser entry in an icon **Browser View Layout**. For Mesh Assets the product image is a **Mesh Preview Render** frame written into the project thumbnail cache; for **Scene Assets** it is a **Scene Thumbnail Render** frame; Texture Assets keep image thumbnails; remaining types use placeholders (including a Scene placeholder when scene still generation fails). Not the leading graphic in Details.
_Avoid_: Treating Mesh thumbnails as texture atlases by default; synchronous GPU generation that blocks the whole Browser refresh as the product path (generation is asynchronous with visible-item priority); using a live dirty editor SceneInstance as the Scene thumbnail source; requiring nested Child Scene content inside Scene thumbs; using a Thumbnail as the Details row icon

**Browser View Layout**:
The session-wide presentation of Content Browser entries for the current folder. Icon layouts (Extra large, Large, Medium, Small) show **Content Browser Thumbnails** at a matching size; **Details** is a columnar table (Name, Type, Size of the Assets-root file, Date modified). The Details Name column uses a type-keyed **Editor Icon** (Folder, Mesh, Scene, Texture, AnimationClip, File), not a Thumbnail. Details column headers sort that column (second click reverses); every sort keeps folders before files. The status-bar slider and the View Layout menu write the same state: slider at minimum is Details; dragging right re-enters Small icons. Not persisted and not remembered per folder.
_Avoid_: Independent slider vs menu modes; Explorer List, Tiles, or Content in this slice; Unity namelist as Details; Explorer Content two-line rows as Details; per-folder remembered views; editor-preference persistence in this slice; a single generic file glyph for every non-folder Details row; mixing folders and files in one Date/Size/Type ordering

**Scene Thumbnail Render**:
The authorship-only still path that, for an on-disk **Scene Asset**, temporarily instantiates that scene, resolves a camera with the Play rule (Main Camera, else first stable Camera), draws one frame at square aspect into a **dedicated** offscreen target (not the main viewport, not **Camera Preview**, not **Mesh Preview Render**), and CPU-readback writes the **Content Browser Thumbnail** cache. No Editor Overlays. Skinned content stays bind/rest pose (no AnimationPlayer sampling in this slice). Prefer scene lights when present; otherwise fall back to fixed studio lighting. Cache invalidation uses the scene file mtime plus a fingerprint of direct Mesh Asset References on that scene. On failure (no camera, render error, etc.) show a Scene placeholder. Does not recurse nested scenes — **Child Scene** composition is out of product.
_Avoid_: Borrowing the Camera Preview or main viewport RT for Browser scene thumbs; first-mesh-only proxy as the product Scene thumbnail; sampling AnimationPlayer clips for scene thumbs in this slice; square thumbs that use the live editor viewport aspect; requiring authored cover images for v1; treating recursive nested-scene instantiation as required for thumbs

**Mesh Preview**:
An authorship-only interactive view of a **Mesh Asset** embedded in the Inspector when that Asset is selected in the Content Browser (**Asset Inspector** mode): orbit, zoom, and reset to default framing. Orbit orientation is session-ephemeral (not persisted). It consumes **Mesh Preview Render** and never appears in the Player.
_Avoid_: Floating Camera Preview panel for Mesh Assets; requiring a scene Entity selection to preview a Mesh Asset; persisting orbit angles as Asset or scene data in the first slice; middle-mouse pan as a first-slice requirement; using **Placement Preview** as the Inspector Mesh view

**Ground placement**:
The spawn pose of a Mesh Asset in the editor viewport: the Editor Camera ray through the pointer intersects the world Z=0 ground plane. A miss uses the world origin. Shared by drop-to-spawn and **Placement Preview**.
_Avoid_: Surface snapping, view-plane billboard placement, dropping at the camera position

**Placement Preview**:
A transient, non-document visualization of a Mesh Asset at **Ground placement** in the editor viewport while dragging that Asset from the Content Browser. Visible only while the pointer is over the editor viewport and the drag source is a Mesh Asset. Hidden over the Content Browser (including folder reparent) and for non-mesh drag sources. Shading matches the spawned MeshRenderer: opaque, Mesh Asset materials, scene lighting — not Mesh Preview studio lighting. It is not a scene Entity, not Mesh Preview, and not an Editor Overlay tool. Drop still seals one Spawn Entity Command.
_Avoid_: Mesh Preview, Drag ghost, Camera Preview, spawning a live Entity during drag, listing the preview in the Outliner, showing the preview while the pointer is still over the Browser, ghost/wireframe as the product look, studio-lit Mesh Preview Render frames as the viewport follow-mesh

**Content Browser drag**:
A pointer-driven drag of a Content Browser entry. Drop on the editor viewport may spawn a Mesh Asset or open a Scene Asset; drop on a Browser folder reparents the entry. Escape aborts the drag with no spawn, no scene open, and no reparent. Distinct from OS file drop onto the Browser and from docking drag. This slice does not spawn or open from an OS drop onto the viewport.
_Avoid_: Slint DragArea as the product mechanism, treating OS import drop as this drag, docking panel drag, requiring mouse release on a random panel to stop a drag, OS file drop onto the viewport as a spawn path

**Content Browser drag cursor**:
During Content Browser drag, the system cursor is one of three states: pointer when the pointer is over the editor viewport and the source is a Mesh Asset or a Scene Asset (place or open); move when over a Browser folder (reparent); not-allowed otherwise.
_Avoid_: A single cursor for the whole drag, a custom copy/plus glyph as the v1 product cursor, leaving the default arrow during drag, showing not-allowed over the viewport for a Scene Asset that will open on drop

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
A single reversible unit on Editor History (Document or Global). It exposes undo and redo. Continuous interactions (e.g. a Translate Modal Session) become one Command at confirm — not one Command per pointer move. New Commands are new types pushed onto History — not a Seam type catalog.
_Avoid_: Per-frame history entries, full-scene snapshot as the default history unit, an Editor Command type registry / palette as the first composition Seam

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

On-disk layout and cook mechanics: [CONTENT_LAYOUT.md](CONTENT_LAYOUT.md). Decision records: [docs/adr/0012-pull-asset-pipeline.md](docs/adr/0012-pull-asset-pipeline.md) (Pull / three-tier), [docs/adr/0019-gltf-intermediate.md](docs/adr/0019-gltf-intermediate.md) (glTF mesh Intermediate; supersedes [0013](docs/adr/0013-collada-intermediate.md)), [docs/adr/0029-detection-action-and-editor-mesh-hot-reload.md](docs/adr/0029-detection-action-and-editor-mesh-hot-reload.md) (Detection Action + editor Mesh hot reload).

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
A directed graph of Asset→Asset references used to invalidate Finals and to know what to Cook. v1 minimal edges: Scene→Mesh Asset; Mesh Asset→Texture Asset via the mesh descriptor’s authoritative `texture_guids` (not via paths inside mesh Intermediate alone); textures referenced only by embedded image URIs and not registered as Assets are out of graph; each Asset→its Intermediate inputs (descriptor + `source` file) as leaves for freshness. No Material Asset nodes, audio/shader edges, or Source-file parsing in v1. Consumers (e.g. Scene / AnimationPlayer maps)→AnimationClip use Asset References; there is **no** Mesh→AnimationClip graph edge — Clip independence is product law ([ADR 0028](docs/adr/0031-animation-clip-independent-of-mesh.md)).
_Avoid_: Hand-maintained file lists, “only this Asset’s own mtime matters” with no cross-Asset invalidation, treating a packaging Manifest as required before the graph exists, inventing Material graph nodes before material descriptors exist, treating embedded image URIs as the canonical Mesh→Texture Asset Reference, Mesh→AnimationClip packaging edges or cascade-delete-as-graph-edge

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
The act of registering external content as project Assets: allocate GUID, write an Asset Descriptor, place Intermediate data under the Resources root, and register the Asset. **glTF/GLB** and images are the primary mesh/texture Intermediate path; **COLLADA is removed**. FBX/OBJ may Source-Export into glTF Intermediate when supported. **Companion Animation glTFs** Import as independent **AnimationClip** Assets (same batch or near-disk discovery may Import Mesh + clips together as a gesture only). Result is Mesh and/or N Clip Assets — no AnimationLibrary and no durable Mesh↔Clip packaging link. Import is not Cook and is not opening a DCC for manual editing.
_Avoid_: Equating Import with Cook, registering COLLADA as mesh Intermediate, leaving Source files as the only registered form with no Intermediate, calling archived Source the Intermediate body, requiring Godot AnimationLibrary to attach companion clips, hard-coding a DogWalk `animations/world` tree walk as the only discovery mechanism, treating multi-select/near-disk as a Mesh-owned clip container, writing `companion_animation_sources` on Mesh, forcing Intermediate under `Models/{mesh}/companions/`

**Source Export (v1)**:
The Import sub-step that converts a non-Intermediate exchange file into Intermediate data. Mesh Intermediate target is **glTF/GLB** (not COLLADA). On Source Export, the original may be archived under the Source root; Cook and Fast Path consume Intermediate glTF (and images / AnimationClip YAML). **Animation Phase 1** extracts readable AnimationClip YAML sidecars from skinned imports while preserving Constant/Stepped.
_Avoid_: Claiming silent `.blend` export in v1, COLLADA as Source Export target, skipping Intermediate and cooking only archived Source bytes, treating glTF as Final

### Animation (Phase 1+)

**AnimationClip**:
A first-class Asset whose Intermediate body is readable YAML of skeletal TRS keyframes (bone names, times, values, interpolation). Cook produces a Final runtime clip. Distinct from Mesh Asset; not a Behaviour and not the AnimationPlayer. The Clip descriptor owns its Intermediate `source`; deleting a Mesh does not delete Clips. Phase 1 sampler supports **Constant** and **Linear** interpolation (glTF STEP/LINEAR map in); Cubic/Bezier later. Importing a multi-animation glTF or **Companion Animation glTFs** yields **N AnimationClip Assets** (and a Mesh Asset only when a mesh host was also Imported).
_Avoid_: Embedding durable clip authorship only inside the mesh Intermediate with no Clip Asset, treating clip bytes as Mesh payload, path-only clip identity without GUID, Phase 1 Cubic/Bezier as required, bundling all clips only inside the Mesh Asset with no Clip GUIDs, requiring a Godot-style AnimationLibrary Asset as the container for imported clips, Mesh→Clip product dependency or cascade delete from Mesh

**Companion Animation glTF**:
An authored glTF/GLB that carries skeletal animations for a character but is not the Mesh Intermediate body (Godot-style split exports). **Acceptance:** has animations and **no meshes** (meshes empty / count zero); skins may be present (Chocomel LOOP files ship skins=1, meshes=0). Import registers independent **AnimationClip** Assets (stem names); Intermediate exchange bodies live under `Resources/Animations/<stem>/` — folders are organization only, not Mesh ownership. **Discovery gestures:** multi-select (disconnected trees) and near-disk scan (mesh dir + parent’s immediate child dirs) may Import clips alongside a skinned mesh without persisting Mesh↔Clip packaging. Same-batch bone-name mismatch against a skinned host: warn and still register. No Mesh descriptor field lists companion sources.
_Avoid_: Treating companion files as Mesh Assets, AnimationLibrary as the companion container, path-only durable clip identity, recursive whole-tree or DogWalk-hardcoded `animations/**` scans as the product default, requiring zero skins to accept a companion, requiring Godot AnimationLibrary paths for companion clip names, failing Import solely because companion bones do not match, extract-only companions with no Intermediate `source` on the Clip, forcing `Models/{mesh}/companions/` or `_standalone_companions` as the product layout, writing `companion_animation_sources` on Mesh, auto-filling AnimationPlayer maps as part of companion Import packaging

**AnimationPlayer**:
The engine-owned playback surface (ClassDB) that samples AnimationClips onto a Skeleton and reports playback time / pose-applied signals for scripts. Phase 1 supports Play/Stop/Loop and hard cuts between clips. **Phase 2** adds a **unified two-slot** model (explicit slot APIs + blend weight; `Play(name, fade)` as sugar), still without an AnimationTree / state-machine graph. Phase 1+: co-located on the same Object as the Skeleton it drives. Clips are addressed by logical string name via a serialized name→AnimationClip Asset Reference map on the player (`Play("LOOP-chocomel-walk")`); GUID is the durable reference inside the map, not the primary script Play key. The map is **editable** in the Inspector (scene assembly); companion/mesh Import must not require writing this map. Phase 2 scenes may also persist **defaults**: TimeScale and optional default two-slot clip names + initial blend weight (not a full live playback snapshot). Edit Mode may expose Phase 2 scrub controls (TimeScale / fade / two-slot weights) without running Behaviour Tick.
_Avoid_: Sampling clips only in C# as the product path, requiring an AnimationTree for Phase 1 playback, Phase 1 remote skeleton driving, GUID-only Play as the primary authoring API, read-only maps with no Inspector edit, requiring Import to auto-fill the name→GUID map as packaging, Godot AnimationLibrary (library/clip paths) as the required Play address model, AnimationTree parameter paths as the Phase 2 primary Play API, serializing full playback-head / in-flight Crossfade snapshots as required scene data in Phase 2

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
The runtime bone hierarchy (rest/bind poses and current pose) that skinned meshes deform against. Pose is advanced by AnimationPlayer and may be overridden after apply by later procedural layers (post–Phase 1). In DogWalk animation Phase 1, Skeleton and AnimationPlayer live on the same Object; cross-Object skeleton references are out of scope. Holds the authoritative **Local Pose**; **Global Pose** and **Matrix Palette** are Animation Pipeline products derived from it (not duplicate authored pose stores).
_Avoid_: Per-vertex animation with no bone hierarchy as the DogWalk character path, treating Object TRS alone as skeletal pose, Phase 1 cross-Object AnimationPlayer→Skeleton links

**Animation Pipeline**:
The per-animated-Object evaluation contract that turns clip clocks plus a blend specification into a Local Pose, then Global Pose and Matrix Palette for consumers. Stages: clip pose extract → pose blend → global pose → optional post-process (SkeletonModifier) → global recompute when needed → matrix palette. AnimationPlayer and AnimationTree supply clocks and blend specification; they are not themselves the pipeline. **Invocation (near-term):** the co-located AnimationPlayer advance / active AnimationTree sample path runs `Pipeline.evaluate` after resolving clocks and blend specification — Player remains the scheduler, not the pose math. The public shape aims toward Player/Tree as pure producers of those inputs. **First reconstruction slice:** extract evaluate so Player/Tree specs write Local Pose, always run the modifier chain then Global + Matrix Palette buffers, fire PoseApplied after stage 6, and make Skinning paths consume the Pipeline palette — without unifying BlendSpec algebra, lazy palette, or world-batch scheduling in that slice. Decision record: [ADR 0032](docs/adr/0032-animation-pipeline-evaluator.md).
_Avoid_: Treating AnimationPlayer or AnimationTree as the whole evaluation pipeline, ad-hoc sample-then-render with no named stages, a second unrelated pose path beside the co-located Skeleton, a world-batch Animation System as the first host for this reconstruction, leaving Tree-active and Edit scrub on a second pose path that skips the Pipeline, shipping unified BlendSpec / lazy-only palette / PreGlobal-PostGlobal split as the first reconstruction Done bar

**Blend Specification**:
The per-frame input that tells Animation Pipeline stages 1–2 which clip poses to extract and how to combine them into one Local Pose. Near-term there are two shapes produced by the co-located playback surfaces—**Player blend specification** (two-slot / Crossfade local TRS blend) and **Tree blend specification** (AnimationTree base then Add2, etc.)—evaluated by one Pipeline for stages 3–6. Long-term aim is one unified blend-specification algebra; that merge is not required to introduce the Pipeline. Exactly one specification feeds the Pipeline per Object per evaluate (active Tree excludes Player two-slot writes).
_Avoid_: Treating AnimationPlayer or AnimationTree themselves as the blend specification, dual-writing Local Pose from Player slots and an active Tree in one evaluate, requiring a single unified BlendSpec type before Pipeline stages 3–6 can exist, inventing a third ad-hoc sample path beside Player and Tree specs

**Local Pose**:
The per-bone local TRS pose stored on the Skeleton after extract/blend (and after post-process that writes locals). The single blended full-skeleton local result of pipeline stages 1–2 (and 4 when modifiers rewrite locals).
_Avoid_: Calling Global Pose or Matrix Palette the Local Pose, treating Object TRS as skeletal Local Pose

**Global Pose**:
The per-bone pose in **Skeleton / model space** produced by walking the Skeleton hierarchy over the Local Pose (no Object world TRS baked in). An explicit Animation Pipeline product with invalidation when Local Pose changes (including after post-process), not merely an unnamed on-demand read. World-space bone positions for gameplay or aim are derived by consumers (or at Modifier entry) using the host Object Transform; they are not a second Pipeline Global buffer in this reconstruction.
_Avoid_: Treating raw Local Pose as gameplay/render world bone pose, requiring callers to re-walk the hierarchy ad hoc as the product contract, baking Object world TRS into Global Pose or Matrix Palette as the default, maintaining dual model+world Global buffers as the first Pipeline requirement

**Matrix Palette**:
The per-joint skinning matrices produced as Global Pose × inverse bind for each bone. Animation Pipeline **stage 6** output: built and cached on each evaluate after final Global Pose (stage 5 when needed). CPU and GPU Skinning paths **consume** this buffer; they do not authoritatively re-derive the palette from Local Pose as the product contract. Default: compute every evaluate. Skip/lazy materialization for non-skinned or non-visible subjects is an allowed optimization that must still invalidate with Local/Global changes.
_Avoid_: Equating Matrix Palette with Global Pose, baking skinning matrices only inside the renderer with no pipeline stage, treating inverse bind alone as the palette, uploading a stale palette after Local-writing post-process

**PoseApplied** (pipeline timing):
The signal raised after a Pipeline evaluate has finished stages through Matrix Palette (stage 6) for that Object — i.e. after blend, SkeletonModifier chain, global recompute when applicable, and palette generation. Retains existing Phase 1–4 roles for Animation step / script sync; it does not fire halfway through an evaluate.
_Avoid_: Firing PoseApplied before stage 5/6 when Global Pose or Matrix Palette are advertised as final, requiring a second “palette ready” event for the default skinned path

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
A Phase 4 **AnimationTree** node that selects among **named animation states** (each state's playback is a BlendSpace1D, BlendSpace2D, or a single clip). Phase 4/5 primary drive is script **Travel** / **Start**. With **AnimationTree Canvas** v1, the StateMachine MAY also auto-Travel via authored **StateMachine transitions** (parameter conditions). Distinct from the gameplay / character state machine in C# Behaviours.
_Avoid_: Merging gameplay state machine into the AnimationTree as one type, requiring Godot-complete transition graphs and nested state machines as Phase 4 Done, removing Travel/Start when transitions exist without an explicit product decision

**OneShot**:
A Phase 4 **AnimationTree** node that **inserts** a clip (or short sub-pose) over the base graph, then **returns** to the underlying StateMachine/BlendSpace base. Used for trip-like interrupts and as the default Sync Group **Fire** path into an **active** AnimationTree.
_Avoid_: Equating every Crossfade with OneShot, requiring AnimationPlayer fade as the only interrupt path, treating CINE Enter as an engine OneShot

**Add2**:
A Phase 4 **AnimationTree** additive layer that combines an additive clip/pose onto the **base** pose. Additive deltas are relative to **bind/rest**. Used for DogWalk-style turn / bark overlays on locomotion. Not a third Phase 2 sample slot and not BlendSpace.
_Avoid_: Calling Phase 2 dual-track blend Add2, requiring N independent additive buses as Phase 4 Done, using world-space additive as the Phase 4 default reference

**DogWalk animation Phase 4**:
The milestone after Phase 3 that adds **AnimationTree** with a lean locomotion graph: **BlendSpace1D**, **StateMachine** (with **OneShot**), and **Add2** — sample order **base then additive (bind/rest)**; scripts use the **narrow named API** (per-node BlendSpace scalars); **topology is scene-embedded** (no required visual graph editor / Tree Asset). Animation step under an active tree uses the **base dominant clip** clock (PoseApplied retained; Add2 is not the step source). Active tree advance uses AnimationPlayer **TimeScale** globally. **Edit Mode** may activate the tree and scrub named drives (BlendSpace scalars, Travel/Start, OneShot, Add2 weights, TimeScale) without DotNetHost / Behaviour Tick; Stepped feel remains Play-validated. **Done criteria:** engineering gate (travel, BlendSpace1D, OneShot including Sync Fire→active tree, Add2 overlay, exclusive sampling, PoseApplied/dominant clock, TimeScale, Edit scrub) **plus** Chocomel (or agreed subset) Play acceptance — perceptible speed-like BlendSpace motion, **visible additive turn** (exact Godot turn clip names not required), OneShot return-to-base, stepped facing still on the base dominant clock. Still without requiring BlendSpace2D, procedural bone modifiers, method/audio tracks, Cubic/Bezier, cross-Object Skeleton drive, AnimationLibrary, or a full cutscene director. When Tree is active it exclusively samples the Skeleton; inactive falls back to AnimationPlayer; Sync Fire on active-tree members uses **OneShot**. Phase 1–3 foundations remain available; engine work **may proceed in parallel** with unfinished Phase 1–3 content gates (those Done criteria are not cancelled). Decision record: [ADR 0025](docs/adr/0025-animation-tree-phase-4.md).
_Avoid_: Shipping full Godot AnimationTree parity as Phase 4, treating Phase 4 as BlendSpace2D/Pinda-complete, folding procedural SkeletonModifier or method/audio tracks into Phase 4 by default, silently dropping Phase 1–3 Chocomel/SYNC content gates when starting Phase 4, growing Phase 1–3 Play APIs into the graph host instead of AnimationTree, dual-writing Skeleton from active Tree and Player two-slot, requiring Fire→travel as the only Sync path into a tree, arbitrary-deep graph nesting as Phase 4 Done, parameter-path strings as the Phase 4 primary drive API, requiring a visual AnimationTree editor or standalone Tree Asset as Phase 4 Done, dropping PoseApplied or Stepped sync while AnimationTree is active, requiring per-node TimeScale as Phase 4 Done, Play-only AnimationTree with no Edit scrub path, requiring Edit Mode Behaviour Tick to drive the tree, declaring Phase 4 done on engineering gate alone without Chocomel-subset Play feel, requiring byte-identical Godot turn clip naming for Add2 acceptance

**DogWalk animation Phase 5**:
The milestone after Phase 4 that adds three capability buckets under **one** milestone with **three independent engineering gates** (all required for Phase 5 Done): **(A)** **SkeletonModifier** chain (post–mixer / post–AnimationTree, before PoseApplied) + **method tracks** (YAML; key-crossing dispatch on dominant-clip clock) — includes extension point, test double, and one minimal LookAt/aim sample; **(C)** **BlendSpace2D** (triangulation + barycentric); **(D)** **AnimationTree Asset** (GUID) + Inspector authorship as D-core (visual canvas optional, not blocking); Asset base + small scene overrides when referenced. Suggested implement order A → C → D. **Done criteria:** all three engineering gates **plus** lean Play bars per gate (A: visible post-pose modifier effect + observable method dispatch; C: perceptible 2D blend — Pinda subset or test field OK, full Pinda not required; D: Asset reference + Inspector edit + instance override without Behaviour Tick). **Edit Mode** may preview SkeletonModifier enable/order + post-pose result, BlendSpace2D (x,y) scrub, and Tree Asset / reference / overrides without DotNetHost / Behaviour Tick; Stepped feel and real method Behaviour handling remain Play-validated. Still without requiring **audio tracks**, Cubic/Bezier, cross-Object Skeleton drive, AnimationLibrary, full cutscene director, or visual canvas as Done. Phase 1–4 foundations remain; open earlier content Done gates are not cancelled. Decision record: [ADR 0026](docs/adr/0026-animation-phase-5.md).
_Avoid_: Treating Phase 5 as full Godot AnimationTree parity, requiring Cutscene Director, silently dropping Phase 1–4 content gates when starting Phase 5, folding audio tracks into Phase 5 by default before they are locked, a single blocking Done bar that withholds A/C until the editor ships, splitting into separate Phase 5a/5b/5c milestones as the default plan, requiring a visual AnimationTree canvas as Phase 5 D Done, PoseApplied-only C# bone hacks as the supported procedural product path, baking procedural overrides only inside AnimationTree nodes, engine PtrCall of arbitrary named methods as the method-track product path, script-only PlaybackPosition scanning as the supported method-track path, full in-scene graph duplication as the primary Asset reuse path, declaring Phase 5 done on engineering gates alone without the lean per-gate Play bars, requiring full Pinda/leash/paper-mouth content parity as the only C/A Play bar, Play-only A/C with no Edit scrub path, requiring Edit Behaviour Tick to receive method tracks

**DogWalk animation Phase 6**:
The milestone after Phase 5 that **productizes** the SkeletonModifier chain with three ClassDB types—**PaperMouth**, **SkeletonAttachModifier**, and configurable **LookAt**—plus scene serialization, Inspector authorship, a lean C-ABI / Blunder.Api bump for key drives, and Edit/Play feel bars for those modifiers only. Hybrid scope: thin engine seam + this-phase content Done; Phase 1–5 open content gates stay tracked and are not closed by Phase 6 alone. Still without leash, audio tracks, Cubic/Bezier, AnimationLibrary, Cutscene Director, Tree canvas-as-Done, generic C# SkeletonModifier subclass hot-bridge, or cross-Object Skeleton animation drive. Decision record: [ADR 0027](docs/adr/0027-animation-phase-6.md).
_Avoid_: Dumping all Phase 5 deferrals into Phase 6, treating leash as Phase 6 SkeletonModifier Done, requiring Tree canvas or Cutscene Director, silently closing Phase 1–5 content Done via Phase 6 alone, equating Attach with remote Skeleton drive, requiring full Pinda parity as the only Play bar, requiring a generic C# modifier subclass bridge as Phase 6 Done

**DogWalk animation Phase 7**:
The milestone after Phase 6 that ships **AnimationTree Canvas** v1 (Asset-truth, dual-track with Inspector, layout-in-Asset, topology-only preview, Asset + Object open paths) covering Phase 5 node types (**StateMachine**, **BlendSpace1D/2D**, **OneShot**, **Add2**), plus **StateMachine transitions** with hybrid condition inputs, author priority, hard-cut switches, and coexisting **Travel**/**Start**. **Done criteria:** engineering + Edit authorship gate (canvas Asset round-trip including transition edges; auto Travel from conditions; both open paths; Inspector dual-track preserved) **plus** a lean Play bar — perceptible automatic state change driven by transition conditions (test rig or agreed subset OK; not full Chocomel/Pinda content parity). Does not cancel open Phase 1–6 content Done gates. Bound canvas preview character, clip-end-only edges, and per-edge fade are follow-ups — not Phase 7 Done by default. Decision record: [ADR 0030](docs/adr/0030-animation-tree-canvas-phase-7.md).
_Avoid_: Treating Phase 7 as full Godot AnimationTree parity, silently closing Phase 1–6 content gates via Canvas alone, requiring bound live preview or per-edge Crossfade as Phase 7 Done, editing scene-embed as the Canvas truth source when an Asset is in play, removing Inspector topology authorship, declaring Phase 7 done on editor UI alone without the lean Play transition bar, requiring full Chocomel/Pinda content parity as the only Phase 7 Play bar

**LookAt** (SkeletonModifier):
A SkeletonModifier that aims a named bone toward a target point. Reads Global Pose (model space), writes Local Pose, thus requiring stage 5 before Matrix Palette. **Target** is specified in **world space** at the product surface; apply converts into Skeleton/model space using the host Object Transform before aiming. Configurable product as of Phase 6.
_Avoid_: Treating LookAt target as model-space by default without conversion, baking Object TRS into Pipeline Global Pose so LookAt can skip conversion, equating LookAt with Add2 or Behaviour Tick bone hacks

**SkeletonModifier type catalog**:
The first Seam: construct a SkeletonModifier from a ClassDB type name. A factory table beside ClassDB — not a second property database. Each Seam registration is a factory plus Add… visibility (`show_in_add_menu`). ClassDB keeps properties, methods, and Inspector fields; the catalog does not store field schemas. Scene deserialize (Editor and Player) and the Add… Skeleton Modifiers group consume it. Product types remain Exported ClassDB classes; test doubles may register a factory without an Inspector export and with Add… hidden. Keys must match ClassDB class names when the class exists. Decision record: [ADR 0035](docs/adr/0035-first-party-seams-beside-privileged-core.md).
_Avoid_: A second property/reflection database, listing Behaviours or Unique attachments here, an Editor-only catalog that Player deserialize cannot see, growing ClassDB instantiate/enumerate as this Seam, putting Inspector schemas or per-type serialize hooks in the catalog, treating the Animation Pipeline `apply` chain as this Seam, moving Registered Systems or Host composition in the same slice as this Seam

**Missing SkeletonModifier**:
A chain slot whose type name is not in the SkeletonModifier type catalog. It keeps the authored type string and an opaque bag of unknown fields that Save writes back unchanged. It does not apply, shows as broken in Inspector, and may be removed. It does not block Save or Play. Not an Add… item, not a product ClassDB type, and not a second property schema.
_Avoid_: Coercing unknown types to base SkeletonModifier (today's `else` drops the name), dropping the slot, failing scene load, listing missing types in Add…, rewriting unknown fields into ClassDB properties, dropping leftover keys on Save

**SkeletonModifier**:
An engine-owned ClassDB post-pose step (Phase 5+) that runs as Animation Pipeline **stage 4** — after Local Pose blend (Player or Tree blend specification) and **before** PoseApplied — mutating the Skeleton or writing related transforms. Multiple modifiers on an Object form an ordered chain. **Semantics:** a modifier may read Global Pose and/or write Local Pose (or non-skeleton outputs such as a child Object Transform). Writes to Local Pose invalidate Global Pose and require pipeline **stage 5** (global recompute) before Matrix Palette and pose consumers. **Near-term implementation:** after the chain completes, always run stage 5 (conservative); per-modifier skip optimization may follow without changing the semantic model. Phase 5 shipped the chain, extension point, test double, and a LookAt sample. Phase 6 product types include PaperMouth, SkeletonAttachModifier, and configurable LookAt. Distinct from Add2 (clip additive in the tree) and from gameplay Behaviours that run in Tick before sampling. Decision records: [ADR 0026](docs/adr/0026-animation-phase-5.md), [ADR 0027](docs/adr/0027-animation-phase-6.md).
_Avoid_: Equating SkeletonModifier with Add2, requiring AnimationTree to host all procedural overrides, reading final pose inside Behaviour Tick as the supported procedural path, treating leash as a required Phase 6 modifier type, splitting the product chain into PreGlobal/PostGlobal hooks as the first Pipeline reconstruction requirement, skipping stage 5 after Local-writing modifiers while advertising cached Global Pose as authoritative

**PaperMouth**:
A Phase 6 SkeletonModifier that drives a named jaw bone from an `openAmount` scalar (typically ∈ [0,1]). Scripts or Inspector set the scalar; an optional mode may fill the same scalar when Attach occupancy / prop-in-mouth logic applies. Not Godot-parity multi-bone lip sync and not a mesh blendshape system.
_Avoid_: Equating PaperMouth with full facial rig parity, requiring blendshapes as the Phase 6 mouth path, requiring leash for mouth Done

**SkeletonAttachModifier**:
A Phase 6 SkeletonModifier that, after pose sample, copies a host Skeleton attachment bone's world transform onto a **child Object**'s Transform so props follow the bone. This is **not** cross-Object Skeleton drive (no remote Skeleton is animated by another player's sampler). Decision record: [ADR 0027](docs/adr/0027-animation-phase-6.md).
_Avoid_: Calling Attach cross-Object Skeleton drive, requiring the prop to be the same Mesh Asset as the host, requiring leash/rope simulation as Attach

**Method track**:
A non-TRS channel on an **AnimationClip** (Phase 5) storing timed logical events (name + optional args) in Intermediate YAML. During playback the engine dispatches on **key-crossing** using the same **base dominant-clip clock** as Animation step (OneShot clock while OneShot is active). Delivery targets co-located Behaviours (and MAY use Message); the engine does not PtrCall arbitrary C# methods by string as the product path. Distinct from audio tracks (out of Phase 5 by default).
_Avoid_: Script-only PlaybackPosition table scans as the supported path, engine-direct PtrCall of named methods as the product path, requiring audio tracks alongside method tracks in Phase 5, blending multiple clip method streams into one clock

**BlendSpace2D**:
A Phase 5 **AnimationTree** node that blends among authored clip points on a **2D** parameter plane (DogWalk/Pinda: e.g. speed × fatigue). Runtime finds a triangle and blends up to three neighboring clips with **barycentric** weights using local TRS lerp / rotation slerp — not additive, not axis-aligned grid-only. Scripts set the 2D parameter by **node logical name** (e.g. `SetBlendSpace2D(node, x, y)`).
_Avoid_: Equating BlendSpace1D with BlendSpace2D, requiring BlendSpace2D for Chocomel Phase 4 Done, using Add2 as the 2D speed/fatigue mechanism, requiring a regular grid as the only authorship layout, nearest-three distance weights without triangulation as the Phase 5 product path

**AnimationTree Asset**:
A first-class Asset (Phase 5 D-core) whose body holds reusable AnimationTree topology (states, BlendSpace1D/2D points, OneShot/Add2 slots, etc.). An Object's AnimationTree MAY reference it by GUID. **Runtime topology:** Asset is the base when referenced; the scene MAY store the reference plus a **small instance-override** set (exact fields at apply-time) — not a full duplicate graph as the product path. With **no** Asset reference, Phase 4 **scene-embedded** topology remains valid. Phase 5 D Done authors via Inspector (visual canvas not required). When an **AnimationTree Canvas** exists, it authors this Asset — not scene-embedded topology as the primary canvas target.
_Avoid_: AnimationLibrary as the tree container, requiring a visual graph editor to create a Tree Asset for Phase 5 Done, path-only tree identity without GUID, ignoring embedded topology when no Asset is set, treating full in-scene graph copy as the primary reuse path when an Asset reference exists, using the canvas to edit scene-embed as the product truth source when an Asset is in play

**AnimationTree Canvas**:
A Phase 7 visual node-graph editor whose **product truth source** is the **AnimationTree Asset** (create/edit topology on the Asset body). Scene instances keep Asset reference + small overrides; Phase 4 embed-only remains when there is no Asset. **v1 Done node set** matches Phase 5 graph capabilities: **StateMachine**, **BlendSpace1D**, **BlendSpace2D**, **OneShot**, **Add2** — not a Godot-complete tree. **Authorship:** Canvas and Inspector are **dual-track** — both may edit the same Asset topology; neither is read-only after Canvas ships. **Editor layout** (node positions / view) persists **in the Asset body** with topology — not only as per-machine editor prefs. **v1 preview:** topology-only — live Skeleton preview while editing the canvas is **not** required for Done (use a scene Object referencing the Asset + existing Edit scrub); **bound preview character** on the canvas is an explicit follow-up. **Open paths (v1):** Content Browser on an AnimationTree Asset **and** Inspector on a scene Object that references that Asset — both edit the same Asset. **StateMachine on canvas:** v1 includes a **real transition graph** (edges drive runtime), not decoration-only links. Distinct from a Godot-complete AnimationTree editor. Decision record: [ADR 0030](docs/adr/0030-animation-tree-canvas-phase-7.md).
_Avoid_: Calling scene-embed JSON hand-edit the Canvas, requiring Canvas for Phase 4/5/6 Done, treating Canvas and full in-scene graph duplicate as the same authorship path, shipping Godot-parity transition graphs / extra node types beyond the locked v1 surface as Canvas Done, leaving BlendSpace2D Inspector-only while 1D is canvas-only as the product split, removing Inspector topology authorship as soon as Canvas lands, storing canvas layout only in ephemeral local prefs as the product path, requiring an in-canvas mini viewport or bound preview Object as Canvas v1 Done, supporting only Asset-browser or only Object-Inspector open as the sole v1 entry, treating transition edges as visual-only with no runtime effect

**StateMachine transition (Canvas v1)**:
A directed edge on the AnimationTree StateMachine that can **automatically Travel** when its **condition** holds. **v1 condition shape:** a **single predicate** per edge — `param op value` with ops `==` `!=` `<` `<=` `>` `>=`, or a bool param truth check — not AND/OR bundles or a free expression language on one edge (compose via multiple edges + priority). Conditions evaluate on the animation advance path — not decoration-only, and not clip-end as the only v1 trigger (clip-end auto edges remain a possible follow-up). **Condition inputs (v1):** a **hybrid** — may read existing named tree drives (BlendSpace1D scalar, BlendSpace2D x/y, Add2 weight, etc.) **and** a small set of independent bool/float **tree parameters** set by script/Inspector. **Conflict rule:** when multiple outgoing edges from the current state are true in one evaluation, pick the highest author **priority**; ties break by a stable order (e.g. declaration order). **Switch pose (v1):** auto transition applies a **hard cut** into the target state (same family as today's Travel) — no per-edge fade required for Done (edge fade is a follow-up). **Script `Travel` / `Start` remain first-class** and MAY force a state change even when no transition edge applies. Distinct from Sync Group Fire / OneShot.
_Avoid_: Decoration-only edges as the product transition model, requiring full Godot expression/transition stacks as v1, treating Sync Group Fire or OneShot as a StateMachine transition edge, removing Travel/Start as the only switch path once transitions ship, requiring every transition condition to invent a parallel param when a BlendSpace drive already exists, forbidding independent bool flags for non-blend gameplay signals, leaving multi-true edges undefined or random, requiring per-edge Crossfade/mixer as Canvas v1 Done, routing tree state switches through AnimationPlayer two-slot Crossfade while the tree is active, requiring AND/OR or scripted expressions on a single edge as Phase 7 Done

**Skinning path (Phase 1)**:

How bind pose + weights turn Skeleton pose into deformed vertices. Consumes the Animation Pipeline **Matrix Palette** (Global × inverse bind). **Fast Path** (Intermediate) uses **CPU skinning**; **Final** (Cooked) uses **GPU skinning**. Editor after Cook and Player share the Final/GPU path; uncooked preview stays CPU.
_Avoid_: Editor-always-CPU as the product rule, requiring GPU skin before first Intermediate preview, maintaining a third unrelated skinning backend, re-deriving an authoritative palette only inside the draw bridge while ignoring Pipeline stage 6

**Scene Asset**:
A first-class Asset for a level/scene document. Persisted as `.scene.asset` JSON with a GUID field; registered like other Assets. Entity mesh links are Asset References (GUID). It is a root for walking the Asset Dependency Graph and (later) Manifests. A Scene Asset is a single flat document of entities — not a nestable prefab via child scene references.
_Avoid_: Path-only scene identity, treating scenes as non-Assets outside the registry, omitting GUID from scene documents, nested scene composition via childScenes

**Child Scene** (removed):
Former nested Scene Asset composition (`childScenes` / `SceneChildReference`) that recursively instantiated other Scene Assets under a parent. **Out of product** — removed in an independent change before New / Duplicate / Save As ([ADR 0030](docs/adr/0030-remove-child-scenes.md)). Scene load, Save, Thumbnail, and fingerprint operate on one Scene Asset only. Legacy `.scene.asset` files that still contain a `childScenes` field load by **ignoring** that field with a warning; the next Save omits it. Prefer placing entities in the scene, or later a deliberate prefab/instance model if nesting returns.
_Avoid_: Keeping childScenes as latent file format; Save merging orphan childScenes; recursive Thumbnail/load of nested scenes; calling mesh/entity parenting a Child Scene; failing open because a legacy childScenes array is present

**New Scene Asset**:
Creating a new starter Scene Asset on disk (new GUID) under the Content Browser folder context, then opening it as the active scene (with the existing dirty-open prompt if the current document is dirty). The starter document is not entity-empty: it includes one default **Main Camera** entity (`isMain: true`, engine CameraComponent defaults) so Play / Scene Thumbnail resolve a camera out of the box. Distinct from Duplicate Scene Asset and from Save As. Primary entry: Content Browser Create/New Scene in the current folder, including a folder/empty-area right-click menu (**New Scene**). Name: auto `NewScene.scene.asset` in the current Browser folder, with `_1`, `_2`, … on collision — no naming dialog in this slice.
_Avoid_: Truly entity-empty New as the product default; omitting Main Camera; cloning the current document; treating New as Save As; creating without opening; putting New only on the editor top bar; requiring a name dialog for v1; shipping lights/meshes as part of the New starter

**Duplicate Scene Asset**:
Copying an existing on-disk Scene Asset to a new path with a new GUID, preserving the source file's authored content (not the live dirty SceneInstance unless that content was already saved). Selects the new entry in the Content Browser and does not open it or change the active scene. Distinct from Save As. Primary entry: Content Browser action on a selected `.scene.asset`, including that asset's right-click menu (**Open**, **Duplicate**, **Delete**). Non-scene Assets' right-click exposes **Delete** only in this slice. Name: same folder as the source, `{stem}_Copy.scene.asset`, with `_Copy_1`, … on collision — no naming dialog in this slice.
_Avoid_: Reusing the source GUID; writing the unsaved active document into the copy; calling Duplicate "Save As"; auto-opening the duplicate; putting Duplicate on the editor top bar; requiring a name dialog for v1; putting Save As on the Content Browser context menu

**Save As (Scene)**:
Writing the current editable scene document (live SceneInstance export, including unsaved edits) to a new Scene Asset path with a new GUID, then continuing authorship against that new Scene Asset as the active document. Document History is kept; dirty clears and the save baseline refreshes as with a normal Save (unlike Open, which clears Document History). Primary entry: editor top bar **Save As…** beside Save — not a Content Browser action. Name: same folder as the active scene, `{stem}_Copy.scene.asset` (fallback stem `NewScene`), with collision suffixes — no naming dialog in this slice.
_Avoid_: Leaving the editor pointed at the old path after Save As; reusing the old GUID on the new file; equating Save As with Duplicate Scene Asset; clearing Document History on Save As; burying Save As only inside the Content Browser; requiring a name dialog for v1

**Reimport**:
Re-running the Import/Source Export path for an existing Asset to refresh Intermediate from archived Source (when present) or to re-apply import settings to existing Intermediate, then invalidating Finals and dependents. Distinct from Cook. May be invoked manually or by Asset Watch via Detection Action. Mesh Reimport refreshes only that Mesh; AnimationClip Reimport refreshes only that Clip from its own descriptor `source` — Mesh Reimport does not drive Clip refresh via packaging lists.
_Avoid_: Equating Reimport with Cook, replacing manual Reimport with watch-only workflows, requiring delete-and-re-import to pick up Source changes, Mesh Reimport cascading into AnimationClips through `companion_animation_sources` or `companions/` folders

**Asset Watch**:
Editor file watching on the Assets root and Resources root (Source archive and Intermediate bodies) that detects on-disk changes for Pull freshness. Watched paths map to Asset GUID(s) via descriptor `source` / `archived_source` (and related glTF sidecars). Detection Action decides whether a matched change prompts or auto-runs Reimport; descriptor/settings churn may still invalidate Finals without a full Reimport.
_Avoid_: Watching only Assets for browser refresh with no Cook invalidation, treating Asset Watch as a replacement for manual Reimport, ignoring Intermediate mutations that should Reimport Intermediate-direct Assets, assuming Project-external DCC folders are watched in v1

**Detection Action**:
An editor-user preference controlling what Asset Watch does when a watched Source or Intermediate (including glTF sidecars) change is attributed to one or more Assets: **Prompt** (ask before Reimport; product default) or **Auto** (Reimport after debounce without a prompt). Applies uniformly to Source archive and Intermediate-direct paths.
_Avoid_: Separate silent-always policy only for Source, per-Project Detection Action as the v1 home for this preference, equating Detection Action with Cook

**Editor Asset Hot Reload**:
After a successful Reimport (manual or Detection-driven), refreshing already-loaded Mesh presentation in the current editor session (AssetManager / viewport / Mesh Preview / placed scene meshes) without restarting the editor. Distinct from Cook and from AnimationClip playback hot-swap.
_Avoid_: Calling Final invalidation alone “hot reload”, requiring immediate Cook to call the session updated, treating AnimationClip/AnimationPlayer live track swap as part of the first hot-reload slice
