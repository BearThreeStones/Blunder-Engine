# Blunder Engine

Editor and runtime for a Z-up, glTF-aligned 3D engine with Blender-inspired viewport gizmos.

## Language

### Agent environment (repository)

**Agent environment**:
The repository-owned control plane around coding agents that edit this repo. Phase 1 is path protection, the Completion gate, and Default validation. Phase 2 is Failure promotion as a mechanism. Phase 3 is Merge CI. It is not a Host composition, not Authorship, and not part of a running engine process.
_Avoid_: Harness (bare), project harness as the product name, DeepSeek Harness, Cordis, Agent Guard as the name of the whole environment, treating Cursor skills or slash commands as the environment itself, calling this an engineering harness, shipping clang-tidy or a failure ledger as Phase 3, treating Failure promotion as a reminder

**Engineering harness**:
A first-party automated test fixture or acceptance executable that exercises an engine slice without claiming product feel.
_Avoid_: Calling Cursor hooks, AGENTS.md, or the Agent environment an engineering harness

**Test run**:
An observed Shell command that executes a first-party test (ctest, or invoking the test executable). Success or failure is recorded separately. Compiling a `*_test` CMake target is a build, not a Test run.
_Avoid_: `cmake --build … --target *_test` as a Test run, claiming a build log as execution, Diagnose or Capture as a Test run, treating exit 0 as part of this term, treating a failing execution as "not a Test run"

**Completion evidence**:
Session-scoped proof taken only from successful Shell commands the Agent environment observed in this session (exit code 0), never from chat. Relevance is distinctive stems from the edited file basename (split on `_` / `-`, dropping generic segments such as `system` or `test`): at least one remaining stem must appear in that observed command string. There is no maintained file-to-test map. A successful command only covers edits that happened before that success. Later C++ edits need new evidence. The Agent environment does not run the tests itself. Whether a stem is covered by an existing first-party test is decided by matching that stem against names under `engine/src/tests/` (file names and executable names), not by an agent statement. If a stem matches an existing test name, Completion evidence for that edit is an observed Test run whose command contains the stem. If no existing test name contains the stem, Completion evidence for that edit is an observed first-party `engine_editor` (or equivalent) **build**. Promotion arming does not replace this proof.
_Avoid_: Authorship Diagnose as this proof, Validate as an Authorship Op, LLM Judge scores, claiming “已验证” or “没有测试” in chat as the proof, treating Capture or a windowed editor smoke as the Phase 1 proof, counting an unrelated `*_test` as evidence, treating an earlier Test run as covering later edits, treating a test-target compile as evidence, a hand-maintained path→target table, a verbal no-test exemption, treating Promotion evidence as a substitute for this proof

**Path protection**:
The Agent environment rule that denies writes to vendor, build, and cache trees, with the Slint fork as the allowed exception. A deny result fails closed. If the hook process is canceled by the runner before it returns JSON, the write is not blocked.
_Avoid_: Editing `engine/3rdparty/` except `engine/3rdparty/slint/`, treating Path protection as the Completion gate, treating a canceled hook process as a successful deny

**Completion gate**:
The Agent environment policy that will not treat a session as finished when this session edited first-party C++ under `engine/src/` (`.cpp` / `.h` / `.hpp`, including tests) and Completion evidence is missing. It does not arm for Slint, CMake, Markdown, content, or `engine/3rdparty/`. It is a stop follow-up, not Host composition and not Diagnose. Follow-ups are capped at two, or four when this session has Promotion arming. After those follow-ups the session may end; that end is not Completion evidence or Promotion evidence. Evidence recording and this gate fail open if their scripts crash. When this session has Promotion arming, the gate also requires Promotion evidence; arming does not waive Completion evidence.
_Avoid_: `/validate` as the gate itself (that command is a map entry), remind-only postToolUse as the gate, the Agent environment executing CTest inside the stop hook, arming the gate on `.slint` or docs edits, unbounded stop follow-up loops, failing closed on evidence recording or the stop hook, treating Promotion arming as a waiver of Completion evidence

**Default validation**:
The documented procedure for producing Completion evidence: build with the CMake presets, run relevant first-party tests, re-cook when import or mesh/texture pipeline changed. It is a map entry (`/validate`, `docs/agents`), not the Completion gate and not an Authorship Op.
_Avoid_: Making validate an Authorship Op, “no CTest suite yet” as current truth, treating a windowed editor smoke as a substitute for tests when a relevant executable exists, requiring Capture as Phase 1 Default validation

**Failure promotion**:
Turning a confirmed escaped defect into a first-party test. Phase 2 of the Agent environment; a mechanism, not a reminder. Confirmation is Promotion arming; done is Promotion evidence. An Agent environment hook may be added alongside; it does not close promotion.
_Avoid_: A prompt paragraph as promotion, a failure-ledger product, adding a Golden Principle line instead of a test, treating Failure promotion as Phase 1, treating CI or clang-tidy as this promotion, inferring confirmation from chat, treating a hook edit as Promotion evidence

**Promotion arming**:
An explicit user command (`/promote` or equivalent) that the Agent environment observes by recording this session as armed. Chat does not arm; the agent cannot declare arming. The record is session state, not a failure ledger.
_Avoid_: Inferring a bug report from conversation, Diagnose or Capture as the arming act, a standing failure ledger, treating Phase 1 Completion evidence as arming, treating an agent statement as arming, treating `/promote` as the Completion gate

**Promotion evidence**:
Session-scoped proof that Failure promotion happened: after Promotion arming, the source of that first-party test under `engine/src/tests/` was edited, then an observed failing Test run, then a later observed successful Test run of the same first-party test name (CTest name or test executable), not merely the same basename stem. One such pair satisfies Promotion evidence for that arming. Re-running an already-green test is not this proof. It is stricter than Completion evidence and applies only when this session is armed.
_Avoid_: A green Test run with no preceding observed failure, compiling a `*_test` target, chat claiming TDD, treating Completion evidence as Promotion evidence, pairing an unrelated failure with a different test's success, treating two `hierarchy_*` tests as the same test because they share a stem, a RED-GREEN pair on a test whose source was not edited after arming, requiring a RED-GREEN pair for every test file touched while armed

**Merge CI**:
The Agent environment gate that runs at merge time (pull request or default branch), outside the coding session. Phase 3. The merge host is GitHub Actions. The job is the documented Linux configure and build, then Test runs of `classdb_test`, `variant_test`, `object_db_test`, and `ptrcall_lifecycle_test` (exact names). Cursor Cloud is a Linux development VM for agents, not this gate. It is not Completion evidence, not Failure promotion, not clang-tidy, and not a failure ledger.
_Avoid_: Treating Merge CI as a substitute for the Completion gate, treating a clang-tidy job as Phase 3, a standing failure ledger, requiring GPU or windowed editor smoke in this phase, treating a Cursor Cloud agent VM as Merge CI, a Windows VS 2026 job as this phase, treating a Merge CI Test run as session Completion evidence

**Default change path**:
The spine for multi-file or product-facing work: Grill, OpenSpec change, apply, automated QC, human acceptance, adversarial review, archive. OpenSpec remains the contract; Superpowers and gstack are tools on this path. Decision record: [ADR 0050](docs/adr/0050-default-change-path.md).
_Avoid_: Three-layer stack as the spine, Superpowers or gstack as a parallel planning system, a second PRD beside OpenSpec, replacing OpenSpec with document-driven pillars

**Grill**:
The human-approved wish step that must finish before an OpenSpec change is created. A small bugfix with no OpenSpec change is not grilled.
_Avoid_: Skipping Grill to keep propose momentum, gstack office-hours as this step, grilling a bugfix that has no OpenSpec change, treating Grill as the OpenSpec contract, 许愿 as the repo term

**User story**:
A scene-shaped wish in an OpenSpec proposal (3–7 per change) that the human confirms before propose finishes. It is the human-readable sign-off, not a spec, not CONTEXT, and not an ADR.
_Avoid_: WHEN/THEN as this sign-off, a separate PRD, treating CONTEXT as the wish list, treating Grill chat as the lasting artifact

**Human acceptance**:
The human walking each User story in the windowed editor, or Headless when that story is a no-window path. The agent may draft the matching checklist; only the human can confirm.
_Avoid_: Completion evidence as this, Merge CI as this, gstack /review as this, Engineering harness as this, agent self-acceptance, a Test run as walking a User story

**Working memory**:
The durable record of one grilled change: that OpenSpec change (and its archive entry). Chat transcripts are not this. The archive listing is the index.
_Avoid_: Checking chat into the repo, docs/exec-plans as this, docs/superpowers/plans as this, AGENTS.md as a session index, a standing PLAN.md dump, treating Cursor transcripts as a deliverable

**Adversarial review**:
gstack `/review` of the change diff, after Human acceptance and before archive. Other gstack planning commands are not this.
_Avoid_: office-hours, plan-ceo-review, autoplan as this, a product/architect/QA role split, treating this as Human acceptance

**Complexity penalty**:
A veto inside Adversarial review: parallel systems, speculative helpers, a second planning track, or a new framework when the existing path suffices. It is not a phase, not a CI metric, and not a new Golden Principle line.
_Avoid_: Cyclomatic budgets, file-count gates, a dedicated complexity stage, adding a Golden Principle instead of using review

**Doc deliverable**:
Versioned Markdown in this repo (`docs/`, `CONTEXT.md`, ADRs, OpenSpec, `AGENTS.md`) plus GitHub Pages that publishes that same source. It is not a second repo, not OINK/Hugo, and not `llms.txt`.
_Avoid_: A second docs repo, OINK, requiring `llms.txt`, requiring a tutorial per OpenSpec change, treating unpublished Markdown as the full deliverable

**Docs site**:
GitHub Pages over this repo's Markdown, built with GitHub's Jekyll (`jekyll-theme-primer`) from a publish tree that keeps repo-relative paths. The tree is README, AGENTS, CONTEXT, CONTENT_LAYOUT, and first-party `docs/`. The home page is `docs/index.md` copied to the site root (a short map, not a product pitch). Jekyll config lives in `.github/pages/` and is copied at build time. It deploys from `main` when those paths change, plus `workflow_dispatch`; pull requests do not publish a preview. Live URL: https://bearthreestones.github.io/Blunder-Engine/. Decision records: [ADR 0050](docs/adr/0050-default-change-path.md), [ADR 0051](docs/adr/0051-docs-github-pages.md).
_Avoid_: OINK, Hugo, MkDocs Material, a second docs repo, publishing `openspec/`, flattening `docs/` to the site root, Merge CI publishing Pages, PR preview sites, using README or AGENTS.md as `/`, putting `_config.yml` inside `docs/` as a document

**Change-path stop**:
The agent halt when Grill or Human acceptance is missing: it asks the human instead of inventing completion. It lives in skills, `AGENTS.md`, and [docs/agents/workflow.md](docs/agents/workflow.md), not in hooks.
_Avoid_: A new Agent environment hook, a Human-confirmed checkbox in the proposal, the agent declaring Grill or Human acceptance done

**Agent-doc maintenance**:
A change to the agent map, skills, or glossary that does not change engine behavior. It is not an OpenSpec change and does not take User stories.
_Avoid_: An OpenSpec change for `AGENTS.md` routing, Human acceptance for skill text, treating this as the Default change path

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
Which Systems start in a given process. Today that is Editor Session versus Player (`EngineHostMode`). Changing composition means starting a new process, not remounting Systems in a live session. No OS window (**Headless**) is not a third composition.
_Avoid_: Cordis profile / bundle / patch YAML as the first composition format, hot-swapping Host composition inside a live process, treating Play Mode as a Host composition (Play Mode is a session; Player is the host); `EngineHostMode::Headless` as a third host

**Headless**:
An Editor Session or Player running with no OS window. Still Editor or Player `EngineHostMode`, not a third mode. Headless Editor mounts Authorship System and Play Session; it does not mount Slint, UiHost, or the viewport sink/bridge. Headless Player still does not mount Authorship System. Windowed Player close is **Player window close**; Headless Player ends via Stop or process exit. A CLI or MCP Editor Session is Headless; the windowed editor does not host those adapters.
_Avoid_: A Headless host mode beside Editor and Player; `engine_agent` as a third process kind; treating no-window as a different Authorship contract; calling an orphan-after-close Player Headless; requiring Slint for Capture; a Headless-only observation API; mounting CLI or MCP on the windowed Editor Session

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
A System that at least one shipped Host composition omits. Editor-only authorship belongs here — Content Browser, Selection, Hierarchy, Scene Edit, Document History, Viewport Pick, Placement Preview, Animation Preview, Play Session, thumbnail/preview render services, Asset Import, UiHost, Slint, viewport sink/bridge, Authorship System. Player must not create them. Mounted at process boot via a registry; callers tolerate absence. Still process-lifetime — not a Plugin and not a Seam registration.
_Avoid_: Plugin, unloading a Registered System while the process runs, conflating Registered System with Seam registration, creating Content Browser or Import inside the Player, Cordis ctx keys for the Privileged core; requiring Slint, UiHost, or the viewport sink in a Headless Editor

### Reflection & scripting

**Gameplay scripting language**:
C# is the sole first-class language for Project gameplay logic. The engine does not ship or productize other script languages (Lua, GDScript, etc.); the C-ABI bridge may still be used by non-product hosts.
_Avoid_: Multi-language official scripting, Lua/GDScript as co-equal product tracks, treating C# as DogWalk-only scaffolding

**.NET script host**:
The in-process CoreCLR host (`nethost` / hostfxr) that loads Project C# assemblies and owns Script Peers. It talks to the engine only through the C-ABI bridge. Assembly-Load-Context hot reload is a later phase on the same host, not part of the first host milestone. For Play Mode, the host runs inside the Player process; Edit Mode does not start it for normal authorship.
_Avoid_: Mono as the product host, out-of-process `dotnet` IPC as the shipping model, bundling ALC hot reload into the first host slice, direct P/Invoke of C++ member layouts; treating an editor-process host as required for Play Mode

**Engine API assembly**:
The generated managed library (working name `Blunder.Api`) produced from the API Blueprint. Project game assemblies reference it; for the .NET host MVP it ships beside the editor/runtime and is referenced by path from the Create `.csproj` template. Default target framework is `net10.0` (current .NET LTS). A NuGet package may be added later for distribution without changing the Blueprint → generator source of truth. Gameplay diagnostics go through the **Debug API**, not `System.Console`.
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
Engine-owned callbacks such as Ready/Tick implemented by Behaviours. For each Object, the engine invokes each Behaviour's Script Peer in that Object's Behaviour list order (one call per Behaviour instance via PtrCall) — not a single call per Object and not per-instance native function pointers on the Object. A **Lifecycle exception** does not skip the rest of that Object's Behaviour list.
_Avoid_: Per-Object TickDelegate fields, one lifecycle call per Object with manual C# fan-out as the only path, global per-type batching that ignores per-Object list order, unmanaged C# doing its own full world walk as the only tick path; letting OnMessage or PoseApplied bypass the Lifecycle exception rule

**Message**:
A directed gameplay communication addressed to an Object by ObjectId. Identified by a MessageId and carrying a small Variant argument bag of at most four arguments. Authors send via a global Message facade (`Message.Send` / `Message.Register`), not via Object instance methods as the primary API. Delivery is synchronous by default and fans out to every Behaviour on that Object in list order; each Behaviour may handle or ignore it (no consume/stop-propagation). During Send, the engine snapshots that Object's Behaviour list and skips destroyed/tombstoned peers; deeper reentrancy safety is by convention, not a full deferred-mutation transaction. The engine receive path for Behaviours is a single entry (`OnMessage`); per-name handler methods are optional authoring sugar later, not the MVP bridge contract. The MVP Send surface is C# (Behaviour-to-Object); native/C++ Send uses the same delivery path but ships later. Used only for cross-Object notifications and commands — not for queries/reads (those use the Object property surface or sibling Behaviour access), not for Lifecycle dispatch, and not as a substitute for ordinary property get/set. A **Lifecycle exception** in one receiver aborts that `OnMessage` only; later Behaviours in the snapshot still receive the Message.
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
The Inspector picker that lists authorable attachments for the current selection — ECS Components, ClassDB members on the Object, and Behaviours. It is an editor authorship gesture, not a ClassDB type, not an ECS Component, and not Unity Add Component. First slice: Unique attachments (Camera, Skeleton, AnimationTree); Behaviour types from the Behaviour type catalog; SkeletonModifier types. Mesh stays Content Browser spawn. First slice requires exactly one selected entity. The popup is a grouped flat list (Unique attachments, Behaviours, Skeleton Modifiers) with no search; an empty Behaviour catalog still shows that group with the build-Scripts hint.
_Avoid_: Add Component, Add Node, treating the menu itself as a runtime type; keeping parallel Add Camera / Add Behaviour / Add Skeleton Modifier buttons as the product path; multi-select Add… as the first slice; nested submenus or type-ahead search as first-slice scope; using Hierarchy **Create…** as if it attached to the clicked row without spawning

**Unique attachment**:
An Add… item that may exist at most once on the selected Object or entity: Camera, Light, Skeleton, AnimationTree. When already present, the row stays visible and disabled.
_Avoid_: Hiding unique items from Add…; treating Behaviours or SkeletonModifiers as unique; allowing a second Camera / Light / Tree / Skeleton on the same selection; AnimationPlayer as a Unique attachment

**Add… kind icon**:
The Inspector and Add… picker mark each Add… kind with one editor icon: Camera, Light, Skeleton, AnimationTree, Behaviour, SkeletonModifier. The icon sits on Unique section headers and on each Behaviour / SkeletonModifier row, and on the matching Add… picker rows. Color follows the label (including grey Unique-already-present and missing Behaviour). Clip rows are not a kind.
_Avoid_: Per-CLR Behaviour icons; per-subclass Modifier icons; treating clip rows as an Add… kind; a second icon on the Behaviours / Skeleton Modifiers section titles; hiding the icon when Unique is already present; AnimationPlayer as an Add… kind

**Hierarchy row icons**:
Icons at the right of a Hierarchy entity row for what is on that entity: Local Transform; MeshRenderer when present; each present Unique attachment (Camera, Light, Skeleton, AnimationTree); each Behaviour; each SkeletonModifier. Unique / Behaviour / SkeletonModifier reuse **Add… kind icon**. Local Transform and MeshRenderer are shown on this row even though they are not Add… kinds. **Clip Binding** is not a Hierarchy icon — it lives only inside AnimationTree (Inspector clip list).
**Attachment property preview** opens from these icons (see that term).
_Avoid_: Unity Component strip as the product name; ECS Component icons; treating Hierarchy row icons as Add… kinds; hiding Transform because every spatial entity has Local Transform; an AnimationPlayer Unique icon; Clip Binding as a scene-mounted attachment or Hierarchy row icon; LMB on a row icon opening **Attachment property preview**

**Inspector present-only sections**:
Add…-authored Inspector property sections (Unique attachments, Behaviours, Skeleton Modifiers) appear only when the selection has that attachment or at least one list row. Absence means the section is not drawn. Add… catalog Unique rows stay listed and disabled when present. Local Transform is not gated by this rule.
_Avoid_: Empty Unique foldouts with placeholder copy such as `No Skeleton on entity`; hiding Unique rows from Add… because they are already attached

**Attachment property preview**:
A floating Inspector-like card for one Hierarchy row icon's attachment (Local Transform, MeshRenderer, Unique, Behaviour, or SkeletonModifier). Opened by Alt+LMB on that icon, including the Camera Unique icon. Plain LMB on the icon selects that row's entity and does not open a preview. Same fields as that Inspector section. Not **Camera Preview**, not **Edit animation preview**, not clip playback, not merely scrolling the Inspector. **Pin** locks that card to that entity+attachment: Hierarchy selection may change and the card stays. Unpin or close dismisses it. Unpinned: a new selection, or Alt+LMB on another icon, closes it. Alt+LMB the same icon while its unpinned card is already open **closes** that card. If that card is pinned, Alt+LMB the same icon raises it — no close, no unpin, no second card for the same attachment. If the entity is deleted or that attachment is Removed, the card **closes** (pin does not keep a missing attachment). Undo that restores the entity or attachment does **not** reopen the card. Closing or switching away from that scene document **closes every preview card** for it; reopening the scene does not restore pins. Entering **Play Mode** does not close preview cards (editor chrome, not Player UI). Several pinned cards may be open at once. This pin is not the dock auto-hide pin. Field edits on the card are **Document History** Commands — the same scene stack as the main Inspector for that attachment, including a pinned card whose entity is not the current selection. Not Global History.
_Avoid_: Camera Preview as this card; treating Alt+LMB as Camera Preview (including on the Camera icon); using this card to scrub clips; a second full Inspector dock; dock auto-hide pin as this lock; one global Inspector lock instead of per-card pin; read-only preview cards; stuffing these edits into Global History; LMB on a Hierarchy row icon opening this preview; Alt+LMB the same icon always spawning another card for that attachment; a pinned card surviving after its entity or attachment is gone; Undo auto-reopening a closed preview card; keeping preview cards after the scene document closes or is replaced; closing all preview cards just because Play Mode started

**Add… host cascade**:
Add… creates missing co-located animation hosts on the same Object: AnimationTree implies Skeleton. Adding Skeleton does not create Tree. Import still does not fill the AnimationTree clip map ([ADR 0031](docs/adr/0031-animation-clip-independent-of-mesh.md), [ADR 0036](docs/adr/0036-clip-binding-authorship.md)). A newly added AnimationTree is empty and **active**, with no AnimationTree Asset GUID required. Empty active Tree does not sample clips (rest). There is no AnimationPlayer Unique and no AnimationPlayer.Play fallback.
_Avoid_: Cross-Object cascade from Add…; auto-filling clips when adding Tree; creating Tree because the author only added Skeleton; requiring a Tree Asset to Add… AnimationTree; a new Tree defaulting to inactive so pose never samples; putting Clip Bindings on AnimationPlayer; Add… AnimationPlayer

**Add clip**:
Inspector control on the AnimationTree section that opens an AnimationClip picker and, on confirm, appends one complete **Clip Binding** (logical name defaults to the clip stem; a taken name is rejected). Cancel adds no row. Not an Add… item. Empty name/GUID draft rows are not a product path.
_Avoid_: Listing clips in Add…; empty name→GUID drafts as the Add clip result; requiring a drag-from-browser as the only way to create a row; Import auto-fill as the only way to create a row; pasting GUID as the primary fill

**Remove attachment**:
Inspector removal of an authored attachment. Unique attachments have a section Remove; Behaviours, SkeletonModifiers, and clip rows keep per-row Remove. Removing Tree does not cascade to Skeleton. Remove Skeleton is disabled while Tree or any SkeletonModifier remains.
_Avoid_: Reverse cascade that deletes Tree/Modifiers because Skeleton was removed; Unique attachments removable only via scene JSON; clip rows that can be added but not deleted

**Add… Object materialization**:
Add… creates a bound Object for ClassDB-hosted attachments (Skeleton, AnimationTree, Behaviour, SkeletonModifier) when the selected entity has none. Camera and Light do not create an Object.
_Avoid_: Creating an Object because Camera or Light was added; requiring a pre-existing Object before Add…; using Add… to force 1:1 Object↔Entity

**Skeleton hydration**:
Filling a Skeleton's rest/bind from the selected entity's skinned mesh Intermediate glTF onto the same Object when Add… creates that Skeleton. A static mesh or failed read still yields an empty Skeleton (warn, do not fail the Add). Add… does not expand a glTF child hierarchy.
_Avoid_: A separate Skeleton Asset as a prerequisite for Add…; empty-only Skeleton as the skinned Add Tree path; using Add Skeleton to spawn importUnderEntity children

**Add… command**:
One Editor Command per Add… click, Remove attachment, Add clip, or clip-row Remove, on Document History. Host cascade and Skeleton hydration belong to that same Command, not separate undo steps.
_Avoid_: Silent non-undoable Add…; one undo step per cascaded host; stuffing Add… into Global History

**Create…**:
The Hierarchy authorship gesture that spawns a **new entity** as a **child of the right-clicked row** (or a Scene Tree root when invoked on empty Hierarchy area, the scene title chrome, or an empty scene). Right-click anywhere on a row (name, Hierarchy Line gutter, expand chevron) selects that entity (single) then opens the menu; canceling the menu leaves that selection. Left-click on the chevron still toggles expand. Empty area and scene title do not change selection until a Create… item runs. First slice Create items are a flat list of **Empty** (no Unique), **Camera**, and **Light** — no Create submenu. On an entity row the same menu continues with a separator then **Delete** (see **Hierarchy Delete**); empty-area and scene-title menus keep only the three Create items. Default entity names are `Empty`, `Camera`, and `Light`; collision in the scene adds `_1`, `_2`, … — no naming dialog. Local TRS is identity (parent origin, unit rotation, scale 1). Create… Camera does not mark **Main Camera** (`isMain` stays false). Light is one menu row; the new Light Component is **Directional Light** and type stays a field. After Create…, the new entity is selected; a collapsed parent expands so the row is visible. First slice host is the **Hierarchy Panel** only (docked and floating). Skeleton and AnimationTree stay Inspector **Add…**, not this menu. Distinct from Inspector **Add…**, which attaches to an existing selected entity and does not spawn. Mesh still comes from Content Browser spawn, not this menu.
_Avoid_: Add Component; Create Node; calling this Add…; attaching Light/Camera to the clicked row without a new entity; putting Mesh spawn on this menu; creating a sibling of the clicked row; parenting at scene root when a row was the menu target; treating the scene title as a Scene Tree parent; refusing Create… when the scene has no entities; requiring a row target for the menu; four Light kinds as Create… rows; first-slice Create… of Skeleton / AnimationTree; naming Create… Camera `Main Camera`; naming Create… Light `Directional Light`; a naming dialog on Create…; Create… Camera flipping **Main Camera** on or stealing it from an existing Main; copying the New Scene Directional world pose onto Create… Light; a non-identity default local TRS; leaving selection on the parent after Create…; keeping multi-select when right-clicking one selected row; creating one child under each selected entity; a nested Create submenu; Duplicate / Rename on this first-slice menu; putting **Delete** among the Create items with no separator; showing **Delete** or a trailing separator on empty-area or scene-title menus; treating a right-click on the expand chevron as expand/collapse; name-only as the menu hit target; viewport or editor top-bar Create… in this slice

**Create… command**:
One Editor Command per Create… item on Document History: spawn the entity, optional Unique (Camera or Light), parent, identity TRS, name, and the post-Create selection. Undo removes that entity and restores the previous selection. Not a Spawn Entity Command followed by an Add… command.
_Avoid_: Silent non-undoable Create…; two undo steps (spawn then Unique); stuffing Create… into Global History

**Hierarchy Delete**:
The Hierarchy authorship gesture that deletes the right-clicked entity row. It is the same scene-entity operation as the Delete key: one Document History Command, soft-delete of that entity only (stable EntityId / tombstone). Descendants stay parented; they leave the editable document while an ancestor is tombstoned and return when that entity is undone. No confirm dialog. Create… Camera / Main Camera is not protected. After delete, selection matches the Delete key (cleared). Empty Hierarchy area and the scene title chrome do not offer Delete or a trailing separator. On an entity row the menu is Empty / Camera / Light, a separator, then **Delete**. The Command label is `Delete {name}` or `Delete Entity` when the name is unknown — same string for the menu path and the Delete key. Distinct from Content Browser **Delete** / **Delete Folder** (Global Command) and from Inspector **Remove** (Unique/list row).
_Avoid_: Asset Delete; Delete Folder; Inspector Remove; treating Hierarchy Delete as a Global Command; a second Command per descendant; a confirm dialog; refusing to delete Main Camera; showing Delete on empty area or scene title; a separate viewport or top-bar Delete in this slice; disabling the Delete key because the menu exists; a different delete semantic for the menu vs the Delete key; leaving the History row as `Edit` when the entity name is known

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
The editor chrome that lists the visible Scene Tree for selection, expand/collapse, and naming. The scene display name is panel chrome above the tree, not a tree parent. A left pointer down on a visible row (including the Hierarchy Line gutter) selects that entity; a left pointer down on the expand chevron of a row with children toggles expand. Right-click **Create…** and **Hierarchy Delete** live only here (docked and floating). Under **Interior freeze (Hierarchy)**, row geometry and Hierarchy Line stay as authored while selection color follows the **Editor accent**; the panel surface fill and default window text follow Editor Theme Window.
_Avoid_: Outliner as the product name, calling the panel the Scene Tree, treating the scene title as a Scene Tree root, treating the Hierarchy Line as a separate control or reparent handle; viewport or top-bar as the Create… or Hierarchy Delete host

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

**Project display name**:
The Project File `name`. It is the identity of the open Project in a windowed Editor Session: Application Bar wordmark and OS window title. It is not the Scene name and not the disk path.
_Avoid_: Folder basename when a Project File name exists; treating the Hierarchy/scene title segment as the Project; putting the full root path in the wordmark or title as identity

**Scripts root**:
The Project folder `Scripts/` that holds C# gameplay sources and the game `.csproj`. It is not part of the `Assets/` / `Resources/` content pipeline; built assemblies are loaded by the .NET script host, not as Content Browser Assets. Project Create (from .NET host MVP onward) scaffolds this folder with a minimal `.csproj` template.
_Avoid_: Storing gameplay `.cs` under `Assets/`, treating scripts as Intermediate Assets, putting the game project inside the engine repo as the product default, naming this root `Source/` (reserved for DCC Source Assets under `Resources/Source/`), leaving new Projects without a Scripts scaffold once the host ships

**Project Manager**:
The standalone `project_manager` executable that lists, creates, imports, and opens Projects before a full Editor Session. Ships beside `engine_editor` in the same output directory (not a multi-version Hub product).
_Avoid_: Manager as a mode of `engine_editor`, Unity Hub multi-editor-version management, an in-editor dock that assumes a Project is already loaded

**Editor Session**:
A full editor run bound to exactly one open Project (its project root). Started by launching `engine_editor` with that Project's path (typically `--project-root`); not the Project Manager app. It may be **Headless** (no OS window) and is still Editor. A CLI or MCP invocation is its own Headless Editor Session; it does not attach to a windowed editor already running on the same root.
_Avoid_: Multi-project tabs in one process for v1, hot-swapping project root inside a live session (deferred); requiring a visible window for Capture or Diagnose; attaching CLI/MCP to another Editor Session; a windowed CLI/MCP Editor Session

**Editor Session restore**:
The windowed Editor Session's remembered **dock layout** (splits, tab order, active tabs, floating panels, auto-hide) and last Live **Scene Asset** (its Asset Reference / GUID) for that Project. It lives in that Project's `.blunder/` cache, not the Project File and not the Project List. That GUID is remembered when that scene opens (and again when the windowed session ends). Dock layout is remembered when a dock change settles and when the windowed session ends — not during an in-progress splitter/float drag. Restored the next time a windowed Editor Session opens the same Project. Distinct from Save. Headless, CLI, and MCP do not use it. OS window geometry, Viewport camera, Content Browser folder, and selection/expand are not this restore.
Windowed Live document open: `--scene` if given (virtual path); else resolve the remembered GUID to the Scene Asset's current virtual path when it still exists in the Project; else `BLUNDER_STARTUP_SCENE` if set; else the compiled default startup scene. `--scene` does not apply the remembered GUID; dock layout still restores. A remembered dock layout that omits a panel kind the editor now ships **injects** that kind at its default home; the rest of the remembered layout stays. A restore record that cannot be read falls back to the default dock layout. Two windowed Editor Sessions on the same Project last-write the restore record; there is no merge and no lock. Decision: [ADR 0046](docs/adr/0046-editor-session-restore.md).
_Avoid_: Application Bar Save; last-opened GUI scene as adapter Live document; reopen-last-Project as the editor's no-arg default; Browser View Layout as this layout; stuffing this into `project.blunder`; treating Viewport camera or OS window placement as dock layout; writing dock layout on every drag sample; letting `BLUNDER_STARTUP_SCENE` beat a remembered Live Scene Asset; skipping dock restore because `--scene` was set; wiping a remembered layout because the engine added a panel; shipping a Reset layout menu in this slice; treating the remembered virtual path as identity after Rename / reparent; merging or locking two sessions' restore records

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
The Project Manager window layout mirrors Godot's Project Manager spatial rhythm (Projects header, create/import strip, project list, open/remove side actions, status) while exposing only Project Manager MVP actions. Non-MVP Godot affordances are omitted, not shown disabled. List rows show name, path, missing state, and last-opened time. UI language is English; the primary Project Open control is labeled Open. Visual colors follow the **Editor Theme**; buttons and Create/Import dialogs use **Editor controls** / **Editor modal**; project rows read as hairline cards with **Editor accent** selection, and New Project / Open are accent primaries. Structure and spacing follow Godot.
_Avoid_: Greyed-out Scan/Favorites/Asset Library/Settings chrome; labeling Open as Edit; a Manager UI language that diverges from the rest of the editor; rebuilding Manager as Unity Hub

**Editor entry (v1)**:
Run `project_manager` to choose/create/import Projects. Run `engine_editor` with `--project-root` for an Editor Session. Debug builds that define `BLUNDER_PROJECT_ROOT` may open that root when no `--project-root` is given. CLI and MCP launches require `--project-root` even in Debug. Release / packaged builds do not use compile-time root as a silent default.
_Avoid_: Embedding Manager UI inside `engine_editor`; Release silently opening the engine checkout; requiring every Debug contributor to pass CLI with no escape hatch; adapter launches omitting `--project-root` and falling back to `BLUNDER_PROJECT_ROOT`

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

### Editor chrome

**Editor Theme**:
The modern dark visual language for Blunder editor chrome: three **Base layers** in deep cool neutrals, **Editor depth** (hairlines plus soft shadows), one **Editor accent**, **Editor corner radius**, named color variables with interaction states (hover, focus, pressed, selected, disabled), and Inter 12px as the default UI type size. Light theme is not part of this decision. Decision: [ADR 0039](docs/adr/0039-modern-dark-editor-theme.md).
_Avoid_: Hardcoded hex as the source of truth; a second ad-hoc dark palette beside these variables; copying Unity Editor Dark's grays or bevels as the product look; treating Light theme as in-scope until decided

**Editor depth**:
Layering reads from stacked **Base layers** plus 1px hairline borders (`#3B3F45`) and soft shadows — a small shadow on docked window frames, a larger one on floating viewport strips and **Editor modals**, whose dim layer is blurred. Bevel (inset/outset light-and-dark edge) borders are not used.
_Avoid_: Unity-style inset/outset bevels; heavy drop shadows on docked panels; borderless panels that lose their edge on the Application Bar

**Editor accent**:
One accent color token — `#4C8DFF` — drives every accented state: primary buttons, checked ToolbarButtons, focus rings, selected rows (including the Hierarchy selected row and List/Tree View selection), Content Browser thumbnail selection, Project Manager selected project, and the Play button. Soft fill (~17% over the surface) and line (~45%) tints derive from that single token, as does the lighter hover fill.
_Avoid_: Per-panel or per-tool accent colors; a second hardcoded blue beside the token; Unity-highlighting Inspector property rows; restyling Hierarchy Line or square selection geometry as an accent treatment

**Editor corner radius**:
6px on controls (buttons, fields, Application Bar buttons, dock tabs, Foldouts, Add… / Remove, ToolbarButtons); 10px on docked window frames, Viewport tool strips, and Content Browser thumbnails; 14px on **Editor modals**. Hierarchy selection rows stay square (**Interior freeze**).
_Avoid_: Square (0px) corners on controls, App Bar, tool strips, or docked windows; rounding Hierarchy selection rectangles

**Editor Shell**:
The host chrome around panel interiors: **Application Bar**, dock tab well, window/panel frames, splitters, dialogs, and the same chrome on Project Manager. It follows the Editor Theme, with **Editor corner radius** on frames and controls and **Editor depth** for layering. Distinct from **panel interiors**.
_Avoid_: Restyling Hierarchy interiors as part of the Shell; calling the whole editor window the Shell; square bevelled dock frames as the product look

**Startup cover**:
The branded surface a windowed Editor Session shows from the first moment that session can show an OS window until the Editor Shell is on screen. Cook and other boot work run behind it. The field is brand plus a short stage name; it is not a percentage. It is not the Viewport, not a Player splash, and not Project Manager chrome. An empty Viewport after the Shell is up is not this cover.
_Avoid_: Splash as the product name (reads as a game/Player splash); treating Headless / CLI / MCP as having a cover; keeping the cover up until the Live scene or first 3D frame; waiting to show any window until after cook; covering only the empty HWND after the window already existed; calling the black HWND the intended look; a fake percent bar as this surface

**Base layers**:
Three stacked backgrounds, darkest first: Application Bar (Base 1, `#191B1F`), Window (Base 2, `#2A2D31`), Toolbar / raised surfaces (Base 3, `#34383D`); recessed fields and **Inspector property field** cells sit below Base 2 (`#1E2125`). Brightness is anchored to Godot's editor (panel `#292929`, field `#1C1C1C`) with a slight cool tint — dark, not near-black. Layering reads through this stacking plus **Editor depth**.
_Avoid_: A single flat editor background; near-black panel surfaces (`#1B1E22` and below) that make the editor read as a void; Unity's lighter 2022 grays (`#383838` window family); bevels as the depth cue

**Application Bar**:
The Editor Shell strip at the top of the Editor Session window (~48px). It is Base 1, darker than Window. Layout: left **wordmark** (`Blunder Editor — {Project display name}`, no Scene, no dirty `*`); **Save / Undo / Redo** on the left as ghost **Editor Icon** buttons (fill on hover only); Play / Pause / Stop / Reload as a centered segmented **Editor Icon** cluster whose Play uses the **Editor accent**; **View** and **Save As…** stay word buttons on the right / left as authored. Not a native File/Edit menu rewrite. Icon-versus-text: [ADR 0042](docs/adr/0042-icon-first-chrome-labels.md).
_Avoid_: Top toolbar; treating a window Toolbar as the Application Bar; moving Save/Undo into menus as part of this Shell pass; leaving Play in the left-aligned button row; giving every App Bar button a filled background; moving Move/Rotate/Scale onto this bar as part of this Shell pass; word faces on Save/Undo/Redo or the Play cluster once those Godot glyphs are wired; using the wordmark for Scene identity or Project switch/open-folder actions

**Viewport tool strip**:
Slint overlay bars on the editor viewport (transform tools, projection toggle). They stay overlays in this pass. They read as floating: translucent Base 3 fill, hairline, 10px radius, soft shadow, and **Editor accent** on the checked tool. Transform **Move / Rotate / Scale** and the global/local space toggle are **Editor Icon**-only (**Icon-first chrome**). Distinct from **Editor Overlay** (3D draw) and from Application Bar / a Scene Window Toolbar. The overlay **animation preview** toolbar is replaced by the **Animation Window**.
_Avoid_: Relocating these strips under the Scene tab as a Window Toolbar in this pass; treating them as Editor Overlays; opaque bevelled bars flush against the viewport; word faces on Move/Rotate/Scale

**Editor modal**:
Authored Slint dialogs (Import Mesh, dirty Play/Open, Detection reimport, Browser Delete, Project Manager Create/Import) use Editor Theme modal chrome — 14px radius, no titlebar divider, blurred dim layer, actions bottom-right with the confirming action as the **Editor accent** primary — plus Editor controls. Copy and button sets stay as authored. Not native OS dialogs for these flows.
_Avoid_: Mixing OS DisplayDialog chrome with Editor Theme for the same flows; more than one accent primary per dialog; redesigning import or dirty-scene workflows as part of this Shell pass

**Panel interior**:
The content inside a docked or floating window frame (Hierarchy rows, Inspector sections, Content Browser grid, History list, Console list). The Editor Shell is the frame around it, not this content.
_Avoid_: Treating the dock tab well as interior; calling the whole panel including tabs the interior

**Interior freeze (Hierarchy)**:
Hierarchy Panel keeps its authored interior *structure*: 22px row geometry, gutter, chevrons, Hierarchy Line (`#737373`), and square selection rectangles. Color is not frozen: the selected row derives from the **Editor accent** (soft fill, accent-tinted text) instead of the authored `#3a3a4a` / `#bde7ff`, and the panel surface fill and default window text follow Editor Theme Window so it occupies Base 2.
_Avoid_: Restyling Hierarchy Line or row geometry; rounding the selection rectangle; a second hardcoded selection color beside the accent token; leaving a hardcoded non-Window island behind the dock frame

**Editor controls**:
The Slint control set whose *inventory* follows Unity Foundations (Button, ToolbarButton, Tab, Foldout, Text/Numeric/Search Field, Toggle, Slider, Color Field, Object Field, List View, Tree View, and the same states); its look is the Editor Theme, not Unity's. Metrics: Inter 12px default, 22px single-line height, 26px large single-line, **Editor corner radius**, hairline borders, ghost-then-fill hover, **Editor accent** for checked/primary/focus. Dock tabs are pills — the active tab is filled, idle tabs have no fill. This is the product chrome for the Editor Shell, Inspector Foldout/Add…/Remove, and unfrozen panels. Not used for **Inspector property fields**.
_Avoid_: std-widgets as the product look; Unity's palette, bevels, or 18px line height; a second Godot color palette; Godot 11px as the default type size outside Inspector property fields; treating this as a UI Toolkit runtime; Numeric Field for Camera FOV or other Inspector property fields; square control corners

**Inspector control skin**:
Inspector chrome that uses Editor controls is limited to section Foldouts, **Add…**, and Remove. **Inspector property fields** and **Transform section chrome** stay Godot-style. **Add…** information architecture stays as authored. Inspector selection colors stay as authored. Panel surface fill and default window text follow Editor Theme Window.
_Avoid_: Replacing Inspector property fields with Unity Numeric Field / Toggle / Color Field; restyling Add… grouping, uniqueness, or cascade; Unity highlight on Inspector rows

**Inspector property field**:
A Godot-style labeled value row in the Inspector: Camera FOV / Near / Far, Light scalars and toggles, **Inspector color cells**, Behaviour bag bool/number/string, SkeletonModifier fields, **Material Inspector** scalars/colors/toggles, **Inspector object cells** for Texture Asset slots, and the same compact 22px cell rhythm as AxisNumberField (not an Editor controls Numeric Field). Cell fill is the theme's recessed field surface, i.e. darker than the Inspector panel, rather than a fixed Godot gray. Inspector does not host **Editor shading overrides**.
_Avoid_: Editor controls Numeric Field, Slider, Color Field, or Object Field for these rows; treating only Transform as Godot while Camera/Light use Editor controls; a cell fill lighter than the panel surface; Blinn-Phong / SSAO global knobs in Inspector

**Inspector object cell**:
An Inspector property field that holds an Asset reference: label, recessed 22px cell showing the Texture Asset display name or None, pick/clear, and Content Browser drag of a Texture Asset. Same Godot row metrics as FOV / Light. Used for **Material Inspector** texture slots (Base Color, Metallic-Roughness, Normal, Occlusion). Behavior may match Editor Object Field (pick, clear, drop); the skin is not Editor Object Field.
_Avoid_: Dropping Editor Object Field chrome into Inspector; GUID/path string as the only slot UI; treating a cleared slot as Texture Asset Delete

**Inspector color cell**:
An Inspector property field whose value is a color: label, recessed 22px swatch, and **Inspector field reset** only when that row uses it. Clicking the swatch opens the **Color picker**; RGB / HSV / Hex live in that picker, not as per-channel Axis number fields on the row. Same Godot row metrics as FOV / Light. **Light color** (Inspector and **Attachment property preview**) has field reset. **Material Inspector** color rows (Base Color, Ambient, Diffuse, Specular) do not — those stay on **Reset overrides**. A Base Color swatch uses a checkerboard when A is below 1. Not the Base Color **Inspector object cell** (texture slot). Not **Editor Color Field**.
_Avoid_: Editor Color Field; three Axis number fields as the Inspector color row; Unity Color Field on this row; calling this row 调色盘; replacing Material texture slots with this cell; Inspector field reset on Material Inspector color rows

**Color picker**:
The popup opened by clicking an **Inspector color cell**. It prefers the left of the swatch, flips below or right if clipped, and closes on click outside — not a dock, not **Attachment property preview**, not **Editor Color Field**. It authors stored linear RGB through RGB / HSV / Linear (opens in RGB; RGB shows 0–255, Linear 0–1); A only when the field stores alpha (Material Base Color); no I, no eyedropper, no Swatches / Recent Colors, and no Hex copy / `#` menu in this slice. Chrome is an HSV wheel, a value bar, a current-color preview (not old/new split), mode sliders, and Hex. Channel numbers are **Axis number fields** (no axis letter) with the same Shift-fine scrub; those scrubs apply live and do not seal until the picker closes or Hex Enter. Wheel and sliders apply live; click-outside, Esc, or clicking the same swatch again closes and seals one Command; Hex Enter seals without closing (the next drag is a new live session). Document Command for **Light color**, Global Command for **Material Inspector** colors. At most one Color picker: clicking another **Inspector color cell** seals and closes the current popup, then opens the new field's picker.
_Avoid_: A left-side color dock; a persistent floating color window; Editor Color Field; 调色盘 as the product name; hiding Light intensity inside an HDR color; an I channel; an eyedropper in this slice; Swatches or Recent Colors in this slice; Hex copy or a `#` menu in this slice; old/new split comparison in this slice; alpha on Light color or Ambient / Diffuse / Specular; storing HSV as a second document color; one History Command per picker pixel; Esc or re-click as cancel-restore; two Color pickers at once; Hex Enter as closing the picker; picker-internal scrub-ended as a History Command

### Editor icons

**Editor Icon**:
A themed vector glyph used for editor chrome and panel affordances (dock close/pin, Application Bar, browser search/refresh/folder, tree/breadcrumb arrows, Inspector scale-link, viewport transform tools, Animation Window transport). Sourced from Godot's `editor/icons` SVG set and shown through dedicated Slint icon components. Fill follows Editor Theme; the default chrome icon color is theme icon gray (`#B3BBC4`), and a checked tool tints its icon with the **Editor accent**. This Shell pass does not switch the source set to Unity's Editor Icon Library.
_Avoid_: Emoji or Unicode text as the source for those affordances; one-off hand-drawn geometry once a Godot glyph is adopted for the same control; replacing the Godot SVG set with Unity PNG icons as part of this Shell pass

**Icon-first chrome**:
Action controls that have a dedicated Godot editor glyph are **Editor Icon**-only; the English name is the tooltip and accessible name, not a visible label on the button. Text stays for panel identity (dock tab titles — a leading icon does not replace the title), entity and Asset display names, Inspector property labels and Foldout titles, dialog/menu copy (**View**, **Save As…**, **Add…**), status word badges (**CINE**, **Inp**), and Clip Binding logical names. The Animation Window TimeScale slider may use the Time glyph as chrome; the Inspector TimeScale row keeps the word. Decision: [ADR 0042](docs/adr/0042-icon-first-chrome-labels.md).
_Avoid_: Replacing property labels or object names with glyphs; inventing non-Godot geometry once a Godot SVG exists; making dock tab titles icon-only; word faces on Save/Undo/Redo, Play/Pause/Stop, Move/Rotate/Scale, Loop, Fire, or Enter/End CINE once those glyphs are wired

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
A session in which the open Project runs as a game for the author to try, separate from scene authorship editing. Gameplay simulation does not share the editor's authorship SceneInstance as its live world. The product Play path is one **Player** process (**Standalone**), not in-process PIE and not Unity-style in-place Tick.
_Avoid_: In-editor Play that mutates the editable document in place as the product model; a second in-process PIE Play beside the Player; treating ScriptHost-on as synonymous with Play Mode; closing **Attachment property preview** cards just because Play Mode started

**Edit Mode**:
The normal editor session for authoring scenes, assets, and project settings — not currently running a Play Mode session.
_Avoid_: Calling idle editor "stopped Play" without a Play Mode concept

**Play Process**:
The separate OS process that runs Play Mode for the open Project. It owns the live gameplay world and script host for that session; the editor process remains in Edit Mode and does not host that gameplay ObjectDB.
_Avoid_: Second editor window in the same process as the product Play boundary; in-process PIE as a second product Play; treating env-gated in-editor DotNetHost as Play Mode

**Player**:
The dedicated Play Process executable (`engine_player`) — a thin entrypoint over the shared engine runtime, not the editor shell. It runs Play Mode only; it is not an authorship UI. It may be **Headless** (no OS window) and is still Player. In a windowed Player, only Gameplay Input (plus system window chrome such as close) is accepted; authorship input (Editor Camera, Editor Overlays / gizmos, viewport pick, Editor Commands) is off. Headless Player has no window chrome; session control is the Play control channel.
_Avoid_: Reusing `engine_editor` with a play flag as the long-term Player; a fully forked second engine tree for Play; treating Player as a second editor viewport; mounting Authorship System in the Player because it is Headless

**Editor Overlay**:
Authorship-only viewport chrome that is drawn and hit-tested in the editor viewport — ground grid, Transform gizmo, Navigate gizmo, selection outline, world axes, origins, wireframe, Camera Gizmo (scene Camera Component visualization), Light Gizmo (scene Light Component visualization), and similar tools. The Player never shows or interacts with Editor Overlays in Play Mode (including while Play Pause is active). The editor viewport keeps Editor Overlays while a Play Session runs.
_Avoid_: Game HUD; debug draw as product Overlay; “game mode chrome”; hiding editor-viewport overlays merely because a Play Session is open; a Player debug toggle to force Editor Overlays on in the first slice; treating Editor Camera orbit as Player gameplay

**Camera Gizmo**:
The Editor Overlay that visualizes and interacts with a scene **Camera Component** in the editor viewport. Visual language matches Blender’s camera wire: origin point, four frustum edges, a **view frame** rectangle, and an **up triangle** on the frame’s top edge. Unselected cameras draw the same shape in a muted color; a **single** selected camera uses the selection color and exposes FOV / clip interaction handles (multi-select draws bodies/frames but not those handles). Frame aspect follows the current editor viewport; frame depth is a fixed local display distance (not a stored sensor aspect). Hit-testing the Camera Gizmo takes priority over mesh viewport pick. It is not the **Editor Camera** and is never shown or driven in the Player.
_Avoid_: Editor Camera widget; Play view HUD camera; treating Navigate gizmo as the Camera Gizmo; inventing a separate non-Blender camera icon language for the first slice; FOV/clip handles on a multi-camera selection

**Light Gizmo**:
The Editor Overlay that visualizes a scene **Light Component** in the editor viewport. Shape follows type: a Directional arrow along the **Light emit axis**; a Point sphere whose radius is **Light range**; a Spot outer-cone along the emit axis; an Area rectangle in local XY. Unselected lights draw muted; a **single** selected light uses the selection color. Clicking the gizmo selects that light entity. This slice has no drag handles for cone angles or Area size (those are Inspector). Hit-testing takes priority over mesh viewport pick; when a Camera Gizmo and Light Gizmo both hit, the closer wins. It is never shown or driven in the Player.
_Avoid_: Billboard light icons as the product gizmo; Player light widgets; FOV-style drag handles on lights in this slice; hiding Light Gizmos merely because a Play Session is open

**Camera Preview**:
An authorship-only floating panel over the editor viewport that shows a live view through a selected scene **Camera Component** (pose + FOV + near/far). It is Slint chrome plus a dedicated preview image, not an OverlaySystem draw into the main viewport offscreen, and never appears in the Player.
_Avoid_: Game View dock; Play window; Editor Camera widget; baking the PiP into `viewport-image`; using Camera Preview to preview a Mesh Asset (that is **Mesh Preview**); using Camera Preview as **Attachment property preview**; opening Camera Preview via Alt+LMB on the Hierarchy Camera icon; using Camera Preview as v1 Capture

**Mesh Preview Render**:
The shared authorship render path that draws a **Mesh Asset** (Final preferred when fresh, otherwise Fast Path Intermediate) with automatic bounds framing into a dedicated offscreen target — not the main viewport offscreen and not the **Camera Preview** target. Lights are fixed **Studio lighting**. Surfaces use each primitive’s MaterialAsset, or **Mesh shading defaults**. A **Mesh material override** applies only to the Mesh Asset’s one MaterialAsset (glTF: first primitive); extra primitives stay Import-built. SSAO is off. It does not read **Editor shading overrides**. It produces still frames for **Content Browser Thumbnails** and live frames for **Mesh Preview**. Skinned meshes use bind-pose (or equivalent rest) stills in the first slice; it does not play AnimationPlayer clips for thumbnails.
_Avoid_: Using a material base-color texture alone as the Mesh thumbnail; sharing the Camera Preview or main viewport render target for Mesh Asset frames; requiring Cook before any 3D Mesh thumbnail; embedding AnimationPlayer playback into thumbnail generation as the first slice

**Content Browser Thumbnail**:
The cached still image shown for a Content Browser entry in an icon **Browser View Layout**. For Mesh Assets the product image is a **Mesh Preview Render** frame written into the project thumbnail cache; for **Scene Assets** it is a **Scene Thumbnail Render** frame; Texture Assets keep image thumbnails; remaining types use placeholders (including a Scene placeholder when scene still generation fails). Not the leading graphic in Details.
_Avoid_: Treating Mesh thumbnails as texture atlases by default; synchronous GPU generation that blocks the whole Browser refresh as the product path (generation is asynchronous with visible-item priority); using a live dirty editor SceneInstance as the Scene thumbnail source; requiring nested Child Scene content inside Scene thumbs; using a Thumbnail as the Details row icon

**Browser View Layout**:
The session-wide presentation of Content Browser entries for the current **Browser Folder**. Icon layouts (Extra large, Large, Medium, Small) show **Content Browser Thumbnails** at a matching size; **Details** is a columnar table (Name, Type, Size of the Assets-root file, Date modified). The Details Name column uses a type-keyed **Editor Icon** (Folder, Mesh, Scene, Texture, AnimationClip, File), not a Thumbnail. Details column headers sort that column (second click reverses); every sort keeps folders before files. The status-bar slider and the View Layout menu write the same state: slider at minimum is Details; dragging right re-enters Small icons. Not persisted and not remembered per folder.
_Avoid_: Independent slider vs menu modes; Explorer List, Tiles, or Content in this slice; Unity namelist as Details; Explorer Content two-line rows as Details; per-folder remembered views; editor-preference persistence in this slice; a single generic file glyph for every non-folder Details row; mixing folders and files in one Date/Size/Type ordering

**Content Browser chrome**:
Search, breadcrumbs, tool strips, tree, and grid (or Details) use **Editor controls**. Virtual paths, Pull, Browser View Layout, thumbnails, and folder ops stay as authored. Not a Unity Project window layout.
_Avoid_: Cloning the Unity Project template's spatial rhythm as the product Browser; retargeting the tree to Assets-only because Unity's Project window does

**Browser Folder**:
An on-disk directory under the Assets root that appears as a folder entry in the Content Browser. It is not an Asset and has no GUID.
_Avoid_: Create Folder (Project Manager); a GUID-keyed folder Asset; treating Resources or Source directories as the Browser primary tree; equating a Browser Folder with an Asset Descriptor

**Browser folder context**:
The Browser Folder that receives New Folder and New Scene. Toolbar and grid empty-area use the currently open folder (grid contents). A folder row’s right-click menu (grid or tree) uses that folder as the parent — create goes inside it, without navigating the grid. New Folder and New Scene share this rule.
_Avoid_: Creating a sibling when the menu was invoked on a child folder; navigating into a folder merely because New was chosen; New Folder and New Scene using different parents from the same menu

**New Folder**:
Creating a new empty Browser Folder under the **Browser folder context**. The first on-disk name is `New Folder`, with `_1`, `_2`, … on collision; **Inline Rename** then starts immediately. Creating the directory seals one Global Command; a later Inline Rename commit that changes the name seals a second. Cancelled Inline Rename leaves the auto-name and only the create command. Primary entry: toolbar **+ New Folder** beside New Scene, plus folder/empty-area right-click on the grid and the folder tree. The Assets root may receive New Folder; it cannot be renamed or deleted.
_Avoid_: Create Folder; allocating a GUID; creating under Resources or Source; a naming dialog; omitting New Folder from the same empty-area menu as New Scene; bundling create+rename into one Command; deleting the new folder when Inline Rename is cancelled

**Inline Rename**:
In-place edit of a Content Browser entry name. Shared by New Folder (right after create), Rename Folder, and Asset rename. Entry points: F2, context-menu **Rename**, and a click on the name of an already selected entry. Does not start when more than one grid entry is selected. Folders edit the directory name; Assets edit the filename stem only — the typed suffix (`.mesh.yaml`, `.scene.asset`, `.texture.yaml`, `.animation.yaml`, …) is preserved and is not in the edit buffer. Commit that collides or is an illegal **Browser entry name** is refused and stays in edit; cancel restores the previous name.
_Avoid_: A modal naming dialog; a separate rename widget per entry kind; treating New Folder as create-without-rename; letting Inline Rename change an Asset’s typed suffix; sanitizing illegal characters into a different name; renaming a multi-selection

**Browser entry name**:
The on-disk directory name of a Browser Folder, or the filename stem of an Asset descriptor. Sibling names must be unique. After trimming leading and trailing whitespace, a name is illegal when it is empty, is `.` or `..`, contains `\ / : * ? " < > |`, is a Windows reserved device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`, case-insensitive), or ends with a space or `.`.
_Avoid_: Auto-replacing illegal characters with `_`; treating collision as a rename success with a silent `_1` suffix; allowing the Assets root’s name to be edited

**Rename Folder**:
Changing the on-disk directory name of an existing Browser Folder while keeping the same parent. Distinct from Content Browser drag reparent.
_Avoid_: Create Folder; renaming the Assets root; Asset rename; treating rename as a new GUID or a new Asset; treating a cross-folder move as Rename Folder

**Asset rename**:
Changing an Asset’s descriptor filename stem under the Assets root while keeping the same parent Browser Folder, typed suffix, and GUID. Intermediate `source` and Source `archived_source` paths are not renamed. Distinct from Rename Folder and from reparent.
_Avoid_: Pairing Intermediate or Source file renames with this gesture; a display-name field separate from the filename in this slice; changing GUID; stripping or replacing the typed suffix; treating a cross-folder move as rename

**Delete Folder**:
Removing a Browser Folder and every Asset whose descriptor lives under it, as one Global Command. The command succeeds only if no Asset in that set has a non-Scene dependent outside the set; Scene references outside the set are detached as with Asset Delete. Empty Browser Folders delete directly with no extra confirm. A folder that contains at least one Asset asks for confirm (folder name + Asset count) before the command runs. Multi-select Delete unions selected Assets and Browser Folders into one set and one Global Command (all-or-nothing, one confirm with the Asset count). The Assets root cannot be deleted. Primary entry: folder context-menu **Delete** (grid and tree) and the Delete key when a Browser Folder is selected.
_Avoid_: Explorer-style directory removal that skips the registry; cascading AnimationClip Assets that live outside the set; deleting Resources or Source trees from the Browser; a partial folder delete that leaves a half-removed tree; deleting the Assets root; a confirm dialog for an empty Browser Folder; skipping folders in a multi-select Delete

**Open Scene follow**:
When Rename Folder, Browser Folder reparent, or Asset rename changes only the path of the open Scene Asset, the editor keeps that document open at the new path. GUID and Document History stay; the path change does not by itself mark the scene dirty. Deleting that Scene Asset (alone or via Delete Folder) uses the existing dirty prompt if needed, then closes the document.
_Avoid_: Treating a path change as opening a different document; clearing Document History on rename or reparent; continuing to edit after the Scene Asset file is deleted

**Delete scene detach**:
Clearing Scene Asset references to GUIDs removed by Asset Delete or Delete Folder. The detach is payload of that same Global Command — on-disk Scene Assets and the live SceneInstance when the open scene is affected. It is not a Document Command. An affected open scene becomes dirty. Undo of that Global Command restores the references. Decision: [ADR 0038](docs/adr/0038-delete-scene-detach-global-command.md).
_Avoid_: A second Document History command for the detach; leaving the live document pointing at a deleted GUID; using viewport Ctrl+Z to undo a Browser delete

**Scene still**:
One dedicated-offscreen frame of a scene through the Play-rule camera, with no Editor Overlays. Aspect is a parameter of this still: **Scene Thumbnail Render** is square; **Capture** is 16:9 with a capped longest edge. **Scene Thumbnail Render** is the On-disk cache product. **Capture** is the Host-observation product: Live document uses the live SceneInstance; On-disk instantiates.
_Avoid_: Main viewport RT; Camera Preview RT; Mesh Preview Render; two lighting or camera rules for Thumbnail vs Capture; using the live window size as Capture aspect

**Scene Thumbnail Render**:
The authorship-only still path that, for an on-disk **Scene Asset**, temporarily instantiates that scene, resolves a camera with the Play rule (Main Camera, else first stable Camera), draws one frame at square aspect into a **dedicated** offscreen target (not the main viewport, not **Camera Preview**, not **Mesh Preview Render**), and CPU-readback writes the **Content Browser Thumbnail** cache. No Editor Overlays. Skinned content stays bind/rest pose (no AnimationPlayer sampling in this slice). Prefer **Light Components** when present; otherwise fall back to **Studio lighting**. Surfaces use each mesh’s MaterialAsset, or **Mesh shading defaults**. SSAO is off. It does not read **Editor shading overrides**. Cache invalidation uses the scene file mtime plus a fingerprint of direct Mesh Asset References on that scene. On failure (no camera, render error, etc.) show a Scene placeholder. Does not recurse nested scenes — **Child Scene** composition is out of product. **Capture** shares this still path (Live document uses the live SceneInstance; On-disk still instantiates) and does not write the Thumbnail cache.
_Avoid_: Borrowing the Camera Preview or main viewport RT for Browser scene thumbs; first-mesh-only proxy as the product Scene thumbnail; sampling AnimationPlayer clips for scene thumbs in this slice; square thumbs that use the live editor viewport aspect; requiring authored cover images for v1; treating recursive nested-scene instantiation as required for thumbs; using a live dirty editor SceneInstance as the Scene thumbnail cache source; treating Capture as a cache write
_Avoid_: Borrowing the Camera Preview or main viewport RT for Browser scene thumbs; first-mesh-only proxy as the product Scene thumbnail; sampling AnimationPlayer clips for scene thumbs in this slice; square thumbs that use the live editor viewport aspect; requiring authored cover images for v1; treating recursive nested-scene instantiation as required for thumbs

**Mesh Preview**:
An authorship-only interactive view of a **Mesh Asset** embedded in the Inspector when that Asset is selected in the Content Browser (**Asset Inspector** mode): orbit, zoom, and reset to default framing. Orbit orientation is session-ephemeral (not persisted). It consumes **Mesh Preview Render** and never appears in the Player.
_Avoid_: Floating Camera Preview panel for Mesh Assets; requiring a scene Entity selection to preview a Mesh Asset; persisting orbit angles as Asset or scene data in the first slice; middle-mouse pan as a first-slice requirement; using **Placement Preview** as the Inspector Mesh view

**Ground placement**:
The spawn pose of a Mesh Asset in the editor viewport: the Editor Camera ray through the pointer intersects the world Z=0 ground plane. A miss uses the world origin. Shared by drop-to-spawn and **Placement Preview**.
_Avoid_: Surface snapping, view-plane billboard placement, dropping at the camera position

**Placement Preview**:
A transient, non-document visualization of a Mesh Asset at **Ground placement** in the editor viewport while dragging that Asset from the Content Browser. Visible only while the pointer is over the editor viewport and the drag source is a Mesh Asset. Hidden over the Content Browser (including folder reparent) and for non-mesh drag sources. Shading matches the spawned MeshRenderer: opaque, Mesh Asset materials, **Light Components** in the open scene — not **Studio lighting**. With no Light Component, the preview has no hidden directional. It is not a scene Entity, not Mesh Preview, and not an Editor Overlay tool. Drop still seals one Spawn Entity Command.
_Avoid_: Mesh Preview, Drag ghost, Camera Preview, spawning a live Entity during drag, listing the preview in the Outliner, showing the preview while the pointer is still over the Browser, ghost/wireframe as the product look, studio-lit Mesh Preview Render frames as the viewport follow-mesh

**Content Browser drag**:
A pointer-driven drag of a Content Browser entry (Asset or Browser Folder, not the Assets root). Drop on the editor viewport may spawn a Mesh Asset or open a Scene Asset; drop on a Browser Folder reparents the entry. Reparent requires a folder target, refuses drop into self or descendants, refuses sibling name collision (no silent `_1`), and treats drop onto the current parent as a no-op success. Folder drop onto the viewport does not spawn or open. Escape aborts the drag with no spawn, no scene open, and no reparent. Distinct from OS file drop onto the Browser and from docking drag. This slice does not spawn or open from an OS drop onto the viewport.
_Avoid_: Slint DragArea as the product mechanism, treating OS import drop as this drag, docking panel drag, requiring mouse release on a random panel to stop a drag, OS file drop onto the viewport as a spawn path, auto-renaming on reparent collision, reparenting the Assets root, dropping a folder into its own descendant

**Content Browser drag cursor**:
During Content Browser drag, the system cursor is one of three states: pointer when the pointer is over the editor viewport and the source is a Mesh Asset or a Scene Asset (place or open); move when over a valid Browser Folder reparent target; not-allowed otherwise (including over files, self, descendants, colliding names, and a folder source over the viewport).
_Avoid_: A single cursor for the whole drag, a custom copy/plus glyph as the v1 product cursor, leaving the default arrow during drag, showing not-allowed over the viewport for a Scene Asset that will open on drop, showing move over an illegal reparent target

**Browser reparent**:
Moving an Asset or Browser Folder to another Browser Folder via Content Browser drag. One Global Command: on-disk move under the Assets root, registry paths rewritten for every moved descriptor, GUIDs unchanged. Intermediate `source` and Source `archived_source` files are not moved. **Open Scene follow** applies when the open Scene Asset’s path changes. Does not start from a multi-selection.
_Avoid_: Cross-root moves; moving Resources or Source trees; assigning new GUIDs on move; auto `_1` on collision; reparent as a Document Command; multi-select drag-reparent in this slice

**Asset Inspector**:
Inspector presentation when the selection is a Content Browser **Asset** rather than a scene Entity. For Mesh Assets: **Mesh Preview**, read-only identity (display name, GUID, type, Intermediate `source` path), and **Material Inspector**. It is not the Import-settings editor and not a dependency-graph browser.
_Avoid_: Equating Asset Inspector with full Import/Reimport UX; requiring Asset Inspector for every Asset type in this slice; driving Mesh Preview only from MeshRenderer Entity selection; putting **Editor shading overrides** back into Entity Inspector

**Material Inspector**:
The Asset Inspector section that edits the selected Mesh Asset’s MaterialAsset surface: Unlit, Base Color, Metallic, Roughness, Ambient, Diffuse, Specular, Shininess, and texture slots (Base Color, Metallic-Roughness, Normal, Occlusion). That surface is the Mesh Asset’s one MaterialAsset — the same object `loadMesh` / `getMaterialAsset()` use (for glTF: the first primitive). Extra primitives in Mesh Preview or scene import keep their Import-built materials; this slice does not list or override them. Edits persist as a **Mesh material override** on that Mesh descriptor: only committed keys are stored, applied on load over the Import-built MaterialAsset. Each committed field (focus loss / Enter / slot pick, clear, or drop) seals one **Global Command** — not a Document Command, not one Command per pointer move while dragging a value. **Reset overrides** is Inspector chrome on this section (same class as Add… / Remove: an Editor control, not a property-field row): one Global Command that deletes the whole sparse bag so the Import MaterialAsset shows through. It does not Reimport and does not delete Texture Assets. It is not a Content Browser Material Asset, not an Entity Inspector section, and not **Editor shading overrides**. Scalar/color/toggle rows use **Inspector property fields** (color rows use **Inspector color cells**); texture slots use **Inspector object cells** — not Editor Slider/Toggle/Color Field/Object Field chrome.
_Avoid_: A standalone Material Asset type in this slice; Editor accent sliders or Editor Object Field for these rows; hosting this section on a scene Entity as a replacement for the retired global block; writing Inspector edits into glTF/Source; session-only RAM with no descriptor write; GUID/path string as the slot UI; pushing these edits onto Document History; a per-primitive material list in this slice; stamping this section’s fields onto every glTF primitive; treating Reset as Reimport; per-field Revert in this slice

**Mesh material override**:
A sparse bag of authored shading keys on a Mesh Asset descriptor (scalars, colors, unlit, texture-slot references). Only keys the author committed are stored. Load = Import-built MaterialAsset for the Mesh Asset’s one surface (`loadMesh` / `getMaterialAsset()`, glTF: first primitive), then overlay those keys. Absent key → Import value. A slot key with a Texture Asset GUID uses that Asset. A slot key that is empty suppresses the Import texture on that slot; it does not delete the Texture Asset. Extra primitives keep Import-built materials. Reimport rebuilds the Import MaterialAsset, then applies the same keys — it does not delete the bag. `texture_guids` stays the Mesh→Texture graph edge list: Import-discovered GUIDs union non-empty override-slot GUIDs. Authorship is undoable as Global Commands (**Material Inspector** commits and **Reset overrides**). **Reset overrides** deletes the bag so Import values show through; it does not Reimport and does not delete Texture Assets. They are Mesh Asset data, not a Material Asset node, not scene-entity overrides, and not **Editor shading overrides**.
_Avoid_: Snapshotting the whole MaterialAsset on first edit; Material Asset graph nodes for this slice; per-entity material instances in this slice; treating Intermediate glTF materials as the editable product document; Reimport silently dropping the override bag; treating an untouched slot as an empty override; replacing `texture_guids` wholesale with only the slots; recording these writes on Document History; applying this bag to every primitive from the same descriptor; using Reimport as Reset

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

**Light Component**:
A native scene Component (like Camera Component) on an entity: pose follows that entity’s TRS; it is the authored light in the Scene Asset. Type is a field on this Component — **Directional Light**, **Point Light**, **Spot Light**, **Area Light** — not four separate Unique kinds and not a C# Behaviour. At most one per entity (Unique attachment). **Add… Light** creates a Light Component whose type defaults to **Directional Light**; changing type in the Inspector keeps that same Unique attachment. Every Light Component has **Light color**, **Light intensity**, **Light enabled**, and **Light contribution**. Every **Light enabled** Light Component in the scene contributes to shading according to **Light linking** and the **Light evaluation cap**; disabled lights contribute nothing. The editor viewport, Player, Camera Preview, and Placement Preview shade only from Light Components in that scene. A scene with none has no hidden directional, no process-global editor light, and no ambient floor. Distinct from **Studio lighting**. There is no **Scene environment** in this slice.
_Avoid_: Light-as-Behaviour; Directional/Point/Spot/Area as four Unique attachments; treating Blinn-Phong inspector sliders as the scene document light; a hidden editor directional when the scene has no Light Component; a hidden ambient fill when Light Components are missing or disabled; only the first Directional contributing; OmniLight / SpotLight3D / AreaLight3D / DirectionalLight3D as product names

**Light enabled**:
A boolean on a Light Component (default on). When off, that light is ignored for illumination and for shadows.
_Avoid_: Deleting the Light Component as the only way to turn a light off; intensity 0 as the product off switch; a hidden editor light that replaces disabled lights

**Light contribution**:
How a Light enabled Light Component participates in shading: **Illuminate and shadows** (default), **Illuminate only**, or **Shadows only**. Shadows only adds no direct light; it only casts this light’s shadows. It is not intensity 0 and not Light enabled off.
_Avoid_: Intensity 0 as Shadows only; a second boolean pair that can mean “do nothing”; Blender ray-visibility checkboxes as the first-slice model

**Light shadows**:
Occlusion produced by a Light Component whose **Light contribution** includes shadows. This slice: only **Directional Light** actually casts shadows, and **at most one** such Directional per view — the first Light enabled Directional whose contribution includes shadows, in stable EntityId order. Other Directionals still add direct light when their contribution includes illumination. Point, Spot, and Area do not cast shadows. Shadows only on a non-Directional light has no effect until that type casts shadows.
_Avoid_: Point/Spot/Area shadow maps as this slice; treating Shadows only on a Point as a working product path this slice; a separate Shadow linking list; picking the shadow Directional by selection, Main, or highest intensity; multiple directional shadow maps in this slice

**Light linking**:
An optional inclusive receiver list on a Light Component: entity ids that have a MeshRenderer. An empty list means this light affects every MeshRenderer in the scene (the default, including newly spawned meshes). A non-empty list means only those MeshRenderers are affected by this light (illumination and/or this light’s shadows). Authors edit the list in the Inspector Light section (add/remove MeshRenderer entities). Missing or non-MeshRenderer ids are ignored. It is not Shadow linking, not a render layer, not a Collection, and not a Mesh Asset GUID.
_Avoid_: Empty list meaning affect none; exclusive blocklists as the first model; linking to Mesh Assets; Unity Light Layers as the product name; putting caster/receiver shadow sets in this same list; a viewport light-link mode as this slice; Hierarchy drag as the required authorship path

**Light evaluation cap**:
This slice evaluates at most **8** lights per MeshRenderer. Candidates are Light enabled lights that affect that MeshRenderer under **Light linking** and whose **Light contribution** is not a no-op for that draw. Take the first 8 in stable EntityId order; drop the rest. A scene may contain more than 8 lights. The single shadow-casting Directional is chosen by the **Light shadows** rule, not by this cap.
_Avoid_: A global first-8 that ignores linking; overflow by intensity or selection; treating the cap as a forever product limit; a 9th light stealing the Directional shadow map

**Light color**:
Linear RGB on a Light Component. Default white. Not a color temperature.
_Avoid_: Kelvin as the first-slice authoring path; hiding intensity inside an HDR color

**Light intensity**:
A scalar multiplier on a Light Component (default 1). Shading energy is **Light color** × **Light intensity**. Not candela, lux, or nits in this slice.
_Avoid_: Physical light units as the first-slice product; type-specific unit labels; intensity-only with an implied white color

**Inspector field reset**:
A per-row control between the Inspector property-field label and the value cell. It writes that one field back to the Unique attachment’s **component default** (Light intensity → 1; **Light color** → white; Camera FOV → 45°, Near → 0.1, Far → 1000). Behaviour bag numbers reset to **0**. SkeletonModifier numbers reset to that modifier field’s default (PaperMouth Open Amount → 0; LookAt Target → (0, 0, 1)). It is shown only when the cell is not at that default. A click seals one **Document Command** for that field only (not Global Command). Same control on **Attachment property preview** rows. Pointer scrubbing still applies live and seals at pointer-up / Enter / focus loss, not per pixel. It is not **Reset overrides**, not a whole-component reset, and not Unity prefab revert.
_Avoid_: Reset overrides; reverting a Mesh material override bag; resetting every Light or Camera field at once; Global Command for scene Light/Camera fields; Unity Prefab override revert; a reset control that stays visible at the default value; reading C# field initializers for Behaviour reset in this slice; Inspector field reset on Material Inspector rows (including color cells); Inspector field reset on Transform axis cells in this slice

**Light emit axis**:
The entity local **-Z** axis — the same look axis as the **Camera Gizmo** (glTF camera / `KHR_lights_punctual`). **Directional Light**, **Spot Light**, and **Area Light** emit along it. **Point Light** has no axis. Direction is the entity’s orientation, not a stored vector on Light Component.
_Avoid_: World +Y forward as emit; Unity +Z forward as emit; a separate direction property on Light Component; a different emit axis per light type

**Directional Light**:
A Light Component type that emits parallel rays along the **Light emit axis**; position does not change the lighting. It has no range.
_Avoid_: Sun-as-huge-Area-Light; treating the editor Blinn-Phong direction slider as the scene Directional Light; DirectionalLight3D as the product name; a range field on Directional

**Point Light**:
A Light Component type that emits from the entity origin in all directions, with an authored **Light range** and **Light distance falloff**.
_Avoid_: Omni, OmniLight3D, treating point as a tiny Area Light; using entity scale as range

**Spot Light**:
A Light Component type that emits from the entity origin into a **Spot cone** along the **Light emit axis**, with an authored **Light range** and **Light distance falloff**.
_Avoid_: Spotlight, SpotLight3D, flashlight-as-Behaviour; using entity scale as range

**Spot cone**:
Two authored angles in degrees on a **Spot Light**: **inner** (full-intensity cone) and **outer** (cutoff). The cone axis is the **Light emit axis**. Constraint: `0 ≤ inner < outer ≤ 90`. Defaults: inner 0°, outer 45°. Falloff lives between inner and outer — not a single angle plus a hidden exponent.
_Avoid_: One-angle Spot as the product model; storing cone in radians in the Scene Asset; inner ≥ outer; outer > 90°

**Light range**:
A positive distance from the entity origin. **Point Light** and **Spot Light** use it as the cutoff beyond which the light does not contribute. **Directional Light** and **Area Light** do not use it in this slice.
_Avoid_: Zero or negative range; intensity-as-range; entity scale as range

**Light distance falloff**:
How Point and Spot intensity changes with distance: inverse-square, reaching 0 at **Light range**. A smooth window near range kills a hard sphere edge; there is no authored attenuation exponent. Directional has no distance falloff. Area has no range cutoff in this slice and does not use this falloff.
_Avoid_: Constant/Linear/Quadratic as authorable models; a hard cut at range with no window; using falloff as a substitute for range

**Area Light**:
A Light Component type that emits from a finite **rectangle** on the entity. The rectangle lies in the local XY plane, centered on the origin, faces the **Light emit axis** (shining along local -Z), and has authored **width** (local X) and **height** (local Y) on the Light Component — two positive numbers, not entity scale. Shading is a front-facing rectangle (the emit-axis side only), not a Point Light with a quad gizmo; width and height change the lighting, not only the Light Gizmo. It is not a disk and not an infinite directional sun. This slice it does not cast shadows.
_Avoid_: Rect Light as a separate Unique kind; AreaLight3D; disk/ellipse as the first Area shape; using entity scale as Area size; shading Area as a single point; lighting the rectangle’s back face; using Area Light to mean environment / HDRI / sky; LTC/GGX area specular as this-slice requirement

**Studio lighting**:
A fixed, non-document key/fill/ambient rig used by **Mesh Preview Render**, and as the **Scene Thumbnail Render** fallback when that scene has no Light Component. It is not authored, not a Light Component, not a **Scene environment**, and not used by the editor viewport, Player, Camera Preview, or Placement Preview.
_Avoid_: Scene light rig; **Editor shading overrides** as studio; using studio lighting to light the live scene when Light Components are missing

**Editor shading overrides** (retired):
Process-global Blinn-Phong direction/color/ambient/diffuse/specular/shininess/unlit and SSAO knobs that used to sit at the bottom of the Entity Inspector. They are not selection state, not a Light Component, not Material, not **Studio lighting**, and not **Scene environment**. They are not a product surface. The editor viewport and Player must not apply them.
_Avoid_: Putting them back in Inspector; a Viewport tool strip or settings dock as a substitute for Light Component / Material / Scene environment; treating the old sliders as document lighting; a silent C++ copy of the same bag still shading the live view

**Mesh shading defaults**:
Fallback BRDF for an editor-viewport or Player mesh draw with no MaterialAsset: white albedo/diffuse, specular 0.4, shininess 32, ambient 0, not unlit. A present MaterialAsset supplies its own albedo, PBR, unlit, and stored Blinn-Phong fields. Not **Studio lighting** and not **Editor shading overrides**.
_Avoid_: Default ka as a hidden ambient floor; a process-global kd/ks bag for live views; using Studio lighting as the missing-material BRDF

**Scene environment** (deferred):
Authored ambient, sky, or IBL on a Scene Asset. Not in this slice. Live shading has no non-document ambient floor. SSAO authorship waits here.
_Avoid_: Blinn-Phong ambient slider as the scene Environment; a hidden grey fill when lights are absent; treating Studio lighting as the scene Environment; Inspector SSAO knobs as environment

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
The scene asset the Player loads when Play Mode starts — captured for that **Play session** as the editor's then-active scene already saved on disk (path/GUID), not a live memory clone of the editable SceneInstance. **Play Reload** instantiates that same captured On-disk asset. Opening a different scene in the editor during the session does not change this entry.
_Avoid_: Play from unsaved buffer without an explicit save step; Play always using a project default scene unrelated to the active document; treating the Live document as the Reload source; retargeting Reload to whichever scene is active now

**Play Reload**:
Replacing the Play Process gameplay world by instantiating that session's captured On-disk **Play entry scene** again, without ending the Play session. The Live document is not this source. Sealed Live Commands reach the Player via **Play authorship patch**, not Reload. Reload itself only instantiates saved On-disk entry. It does not replay sealed Live Commands onto the new world. Started by a dedicated Reload control on **Play controls**. Save does not Reload. Play while already in Play Mode is still Stop then a new Play Process, not this. Reload does not run **Play Scripts build** and does not replace the Scripts assembly already loaded in that Play Process; Behaviours remount from that assembly. Ready runs on that remount even while **Play Pause** is active. Reload does not change the session's Playing versus Paused flag; Tick stays skipped while Paused. A Warning-grade Issue when Scripts are dirty does not block Reload. New C# requires Stop then Play. Failed Reload preflight or instantiate leaves the current Player world in place. Decision: [ADR 0047](docs/adr/0047-play-reload.md).
_Avoid_: Dumping the Live SceneInstance into the Player; Stop then Play as this action; **Editor Asset Hot Reload**; Apply as a merge of Live onto the ticking world; treating unsaved Inspector edits as the Reload source; Save as the Reload gesture; Play-while-Playing as Reload; ALC or `dotnet build` as part of Reload; blocking Reload because Scripts are dirty; tearing down the Play Process because Reload preflight or instantiate failed; treating Play authorship patch as Reload; replaying Live Commands onto a world just created by Play Reload

**Play authorship patch**:
A Document-History-sealed **Editor Command** on the Live document applied onto the running Play Process world without **Play Reload** and without requiring Save. Only Commands whose Live document is this session's **Play entry scene** (same Scene Asset GUID) become patches; Commands on any other document stay editor-only with no channel message and no Issue. v1 patches are authored data on entities that already exist in both documents: Local Transform, Unique attachments, MeshRenderer, Behaviour bags, SkeletonModifiers, and animation-host fields. Spawn, delete, rename, and reparent stay editor-only until **Play Reload**. The address is **Authorship Address** (scene-unique entity name), not EntityId or ObjectId. Uncommitted gizmo or field samples are not this. A patch writes at Command seal whether the session is Playing or Paused; later Behaviour Tick may overwrite the same fields. There is no authorship lock until Reload. Undo, Redo, and History Jump of a v1-patchable Command also patch, using the document state after that history step. A failed patch does not roll back Editor History. If the Play Process has no entity for that Authorship Address, the editor records a Warning-grade Issue and skips the write — not a Request failure, and not **Error Pause**. Decision: [ADR 0048](docs/adr/0048-play-authorship-patch.md).
_Avoid_: Dumping the Live SceneInstance into the Player; streaming uncommitted pointer moves; requiring Save before a patch; Stop then Play or Play Reload as this; EntityId or ObjectId as the patch key; ALC or Asset hot-reload under this name; Pause-only patches; locking patched properties against Tick; patching a Live document that is not the session Play entry; treating Spawn, delete, rename, or reparent as v1 patches; Global Commands (Material override, Browser delete) as Play authorship patches; Undo that changes Live without patching a v1-patchable Command; rolling back History because the Player missed a patch; blocking Live edits when the Player lacks that entity; Error Pause on an unknown patch address; silent drop of a missed v1 patch

**Play pose preview**:
Observation of Play Process entity poses in the editor viewport for entities that exist in this session's **Play entry scene**, keyed by **Authorship Address**. It does not write the Live document, does not dirty, and does not push Document History. Inspector values and Transform gizmo handles stay on Live authorship. It is not **Capture**, not a **Play frame**, not Query, not **Play authorship patch**, and not in-process PIE. Runtime-spawned Player entities with no Authorship Address are not this. Stop or **Play Reload** ends the current preview stream (Reload starts a new world, then preview follows that world). Decision: [ADR 0049](docs/adr/0049-standalone-play-preview-and-patch.md).
_Avoid_: Writing Tick into Live; Keep Simulation Changes as this; Unity in-place Play; a second product PIE; treating Play frame stills as this live viewport draw; Query of the Play Process as an Authorship Subject; a second Play-session socket for poses; gizmo handles that edit Play poses as if they were Live

**Play dirty prompt**:
When Play or **Play Reload** is requested on a windowed Editor Session and the active scene document is dirty, the editor asks how to proceed: save then Play/Reload, Play/Reload from the last saved asset, or cancel. Play Mode does not silently discard or silently auto-write authorship edits. A Headless Editor does not show that prompt; Play and Play Reload use the last saved Play entry asset (the same choice as windowed "last saved").
_Avoid_: Always auto-save on Play or Reload with no prompt; blocking Play or Reload with save-only and no choice; playing or Reloading the on-disk asset with no indication when the editor view is dirty; a second Reload-only prompt with different choices

**Play Scripts build**:
Before starting the Player, the editor builds the Project `Scripts/` output when those sources (or their build inputs) are newer than the last successful scripts output — otherwise it reuses `.blunder/scripts_bin`. This build is not Diagnose and not an Op. A failed build keeps the session in Edit Mode and reports Error-grade Issues; Diagnose may report Scripts dirty or missing output without compiling. **Play Reload** does not run this build.
_Avoid_: Building Scripts on every Play with no dirtiness check; requiring the Player process to run `dotnet build`; starting Play against a stale missing assembly with no build attempt when Scripts are dirty; treating the build itself as Diagnose or as an Editor Command; treating Play Reload as a Scripts rebuild or ALC reload

**Play live sync (deferred)**:
Scripts assembly replacement (ALC) or Player-side Asset hot-reload into a running Play Process without **Play Reload** or ending the session. **Play Reload**, **Play authorship patch**, and **Play pose preview** are the product bridges; they are not this leftover bag.
_Avoid_: Treating ALC or asset hot-reload into a live Player as required for Play Reload; using this bag as the name for Play Reload, Play authorship patch, or **Play pose preview**

**Play session**:
At most one Play Process for the editor at a time. Stop ends that process (graceful close first, then force if needed). Starting Play while already in Play Mode stops the existing session first. That replacement spawn is not **Play Reload**.
_Avoid_: Multiple concurrent Players as the v1 default; Stop that only clears UI state while leaving a live Player running; treating Play-while-Playing as Play Reload

**Edit Mode scripting**:
Normal scene authorship does not start a .NET script host in the editor process. Gameplay Behaviour peers and Tick run in the Player during Play Mode. An env-gated editor host remains a debug/test escape hatch, not the product Play path.
_Avoid_: Requiring `BLUNDER_DOTNET_SCRIPTS` for Play Mode; dual Tick in editor and Player as the default

**Play controls**:
The editor exposes Play, Pause, Stop, and Reload for the Play session. Play starts (or resumes) the single Play Process; Pause freezes gameplay simulation while the Player stays alive; Stop ends the Play Process and returns the author to Edit Mode; Reload runs **Play Reload** on the existing Play Process.
_Avoid_: Play-only toggle with no Pause; Pause that exits Play Mode; Stop that leaves the Player process running; hiding Reload behind Save or behind Play-while-Playing

**Play Pause**:
While paused, the Player skips realtime gameplay Behaviour Tick (and other gameplay simulation time) but keeps the process and window alive so the author can still view the frozen world through the resolved scene Camera. **Play step** is the way to advance that frozen world. Resume continues realtime Tick from the paused world state. Pause does not enable Editor Camera orbit or other authorship input in the Player. **Play Reload** does not change this flag: a Paused session stays Paused after a successful Reload; Ready on remount still runs.
_Avoid_: Pause that tears down the Player; Pause that freezes rendering as the only definition; Pause as a synonym for Stop; Pause-time Editor Camera orbit in the Player window; treating Pause as blocking Play step; using Play Reload to force Playing or Paused

**Play camera preflight**:
Before spawning the Player, and again before **Play Reload** commits a new world, the editor runs Diagnose (Play rule set) on the Play entry as it will be loaded (after dirty-prompt save rules) and requires at least one valid Camera Component. Error-grade Issues keep the session in Edit Mode when spawning. On Reload, those Errors abort Reload: the Play Process and current gameplay world stay, and the session remains Playing or Paused. This is not a separate stringly error type beside Issue.
_Avoid_: Starting Player with no Camera and relying on a black screen; auto-injecting a Camera at Play time; a parallel preflight error that is not an Issue; Reload that tears down the Play Process because preflight failed; skipping preflight on Reload

**Play control channel**:
A local IPC link between the editor and the single Play Process. Editor → Player carries session commands (at least pause, resume, stop, **Play step**, a **Play frame** request, **Play Reload**, and **Play authorship patch**). Player → editor carries **Play log forwarding** (Console Messages) on the same connection, Play frames, and **Play pose preview** poses. Process exit is also treated as leaving Play Mode. It is not a networked multiplayer protocol. Decision: [ADR 0040](docs/adr/0040-console-play-log-on-control-channel.md).
_Avoid_: Editor Pause with no way to reach the Player; Stop that only kills without a graceful command path; designing the first channel as internet-facing RPC; treating the Player OS terminal as the author's diagnostic surface; a second Play-session socket just for logs, frames, Reload, patches, or poses; capturing Player stdout as the Console feed; Query of the Play Process world as an Authorship Subject

**Play frame**:
A still of the Play Process world through the Play-rule camera. Host observation, not Capture, not a Scene still, not Query. It shows the ticking (or paused) Player simulation, including AnimationPlayer and gameplay. Same 16:9 capped-longest-edge product aspect as Capture, different world. Rides the Play control channel.
_Avoid_: HWND scrape; calling this Capture; Query of the Player; a second Play socket; Editor Camera in the Player; treating this still as **Play pose preview**

**Play step**:
A Play control channel request, legal only while **Play Pause**. It advances the paused Play Process by N gameplay Ticks at fixed dt 1/60, stays paused, then a **Play frame** can follow. Unpaused Play stays realtime. v1 Play observation is Play step + Play frame. Not an Op, not Query, not Diagnose. Play dump is not this slice.
_Avoid_: Sleep-as-step; stepping while Playing; variable dt; an Authorship Op that ticks the Player; a second Play socket; treating `BLUNDER_PLAYER_MAX_FRAMES` as this

**Player window close**:
On a windowed Player, closing the OS window ends the Play Process and therefore ends Play Mode (same outcome as Stop). A **Headless** Player has no OS window; ending it is Stop on the Play control channel or process exit.
_Avoid_: Closing a windowed Player's window while leaving the process simulating; tray-minimize as the v1 close behavior; treating Headless as this orphan-after-close case

**Edit during Play**:
While a Play session is running, the author may keep editing the Project in the editor. Uncommitted Live edits do not appear in the Player. Sealed Live **Editor Commands** may appear via **Play authorship patch**. The editor viewport may draw **Play pose preview** without changing Live. A newly saved Play entry scene replaces the whole Player world only after **Play Reload** or a later Play (after save/build rules). Play Mode does not lock the authorship document.
_Avoid_: Freezing the editor for the whole Play session as the v1 rule; streaming uncommitted gizmo samples into the Player; implying Save alone Reloads the Player world; writing Play poses into Document History

**Editor Command**:
A single reversible unit on Editor History (Document or Global). It exposes undo and redo. Continuous interactions (e.g. a Translate Modal Session) become one Command at confirm — not one Command per pointer move. New Commands are new types pushed onto History — not a Seam type catalog.
_Avoid_: Per-frame history entries, full-scene snapshot as the default history unit, an Editor Command type registry / palette as the first composition Seam; treating an Editor Command as the machine-facing request (that request is an Op)

**Op**:
A single mutating request on the machine authorship contract. One Op commits as exactly one Editor Command on Document History or Global History. It is not the Command itself, not a Query, and not a Message.
_Avoid_: An Op type registry as a Seam; Message; GameCommand; pushing Op onto History as a second undo unit; mutating the document without a Command; batching several Commands under one Op in v1

**Query**:
A read-only request on the machine authorship contract. It does not push Editor History.
_Avoid_: Disguising a Query as an Editor Command; using Message for editor reads; treating a Query as an undoable unit; returning pixels; dumping the Play Process world

**Diagnose**:
A read-only request on the machine authorship contract whose result is a list of Issues, not a world snapshot. It does not push Editor History.
_Avoid_: Treating Diagnose as a Query projection; making validate an Op; using Console Message or a log line as the Diagnose result contract; Message; compiling Scripts or Cooking as Diagnose; returning a screenshot; using Diagnose as an event stream

**Issue**:
One Diagnose result: a stable code, a severity, an optional Authorship Address, and an explanation. It is not a Console Message, not an Editor Command, and not a log line.
_Avoid_: Scraping Console text as the machine result; stringly-typed errors with no code; EntityId as the Issue target; treating a Console Message as an Issue; using Issue for a failed Query or Op

**Issue severity**:
The grade on an Issue: Log, Warning, or Error. Same three names as Console severity; not Console Message and not LogLevel.
_Avoid_: A second severity vocabulary; calling this LogLevel; treating a Warning as a Play blocker

**Request failure**:
A stable code when a Query or Op cannot be carried out (unknown Authorship Address, no Live document, Op aimed at an On-disk Project). It is not an Issue and does not push Editor History. CLI maps this to a non-zero exit and a JSON object on stdout; Diagnose that ran is not this, even when it returns Error Issues.
_Avoid_: Reporting request errors as Diagnose Issues; a successful no-op on unknown address; using Console Message as this result; treating Diagnose Error Issues as a CLI request failure

**Authorship Address**:
The public target of Query, Op, and Diagnose. For a scene entity it is that entity's scene-unique name — the same string the Scene Asset already persists. For an Asset it is the Asset Reference (GUID). It is not EntityId and not ObjectId in this slice.
_Avoid_: EntityId on the machine contract; ObjectId as a durable scene address before it is persisted on the scene; virtual path as Asset identity; hierarchy path as entity identity

**Live document**:
The open editable document in an Editor Session (v1: the active scene, including unsaved edits). Op always mutates this. Each Editor Session has its own Live document — a CLI or MCP process does not see unsaved edits from a different windowed editor. A CLI or MCP session has a Live document only when launched with `--scene` (virtual path of that scene). Without `--scene`, Live verbs fail closed; On-disk verbs may still target a saved Scene Asset.
_Avoid_: The Play Process world; treating the last-saved Scene Asset as this when the editor is dirty; treating another process's dirty buffers as this Live document; opening last-opened GUI scene as the machine Live document; guessing a main scene when `--scene` is omitted

**On-disk Project**:
The Project as stored on disk (Scene Assets, descriptors, Scripts), independent of an Editor Session. Op may not target this.
_Avoid_: Reading dirty editor buffers as this; requiring the editor UI to Diagnose an On-disk Project

**Authorship Subject**:
The world a Query or Diagnose is evaluated against: the Live document or the On-disk Project. Every Query and Diagnose states this explicitly. Op has no subject choice.
_Avoid_: A silent default that mixes dirty editor state with saved files; a third unnamed world; treating Player simulation as an Authorship Subject

**Authorship contract**:
The first-party Query / Op / Diagnose contract for reading and changing a Project. GUI, CLI, and MCP adapt this contract; the same adapters also speak Host observation and Play Session control. They do not merge stills into Query, Op, or Diagnose. CLI and MCP share one verb set — two presentations, not two catalogs. CLI and MCP run in the Headless Editor Session process; Player does not host them. The MCP adapter uses stdio on that process and does not listen on a port. The CLI adapter is one verb, one process, then exit. `--mcp` or a CLI verb implies Headless. Adapter launches require `--project-root` (no Debug compiled-root fallback). MCP `save` persists the Live document and is not an Op; CLI Op requires `--save`. Not a Seam, not Message, and not the C-ABI bridge. Decision records: [ADR 0041](docs/adr/0041-authorship-contract.md), [ADR 0044](docs/adr/0044-machine-adapters.md).
_Avoid_: Agent API as the product name; Editor Command type registry; MCP as the domain name; exposing this contract from the Player; a fourth Observe intent; putting screenshots or Play dumps on this contract; a third process kind whose only job is to adapt (`engine_agent`); a second machine catalog beside this contract; HTTP or a listen port as the v1 MCP path; a long-lived CLI REPL beside MCP; adapter launches using `BLUNDER_PROJECT_ROOT` when `--project-root` is omitted

**Authorship System**:
The Registered System that hosts the Authorship contract in an Editor Session. It routes Op to Editor Commands and Live Query / Diagnose against the Live document. Player does not mount it. On-disk Diagnose reuses the same rule code from a tool without mounting this System in the Player.
_Avoid_: Context System; Privileged core; Seam; mounting this in the Player; a second mutation path that bypasses Editor Commands

**Authorship contract v1**:
The first slice of the Authorship contract: Query of the scene's entity names and of one entity (name, parent name, local TRS) by Authorship Address; one Op that sets local transform; Diagnose of the Play rule set (missing Camera; Scripts dirty or missing output). Not the full Editor Command catalog.
_Avoid_: Diagnose-only; Query/Op with no Command path; Import, Cook, or Play-start as this slice; exposing EntityId

**Host observation**:
Stills a Host can emit for machines, plus Play step. Not Query, not Op, not Diagnose, and not a fourth Authorship intent. Editor stills are **Capture** (a Scene still). Play Process v1 observation is **Play step** plus **Play frame** on the Play control channel. Diagnostic utterances stay **Console Messages** (Player origin via Play log forwarding). There is no Host event stream beside Console. CLI and MCP adapt this from the Editor Session process (same Play Session as the GUI), not from the Player process. Decision records: [ADR 0043](docs/adr/0043-host-observation.md), [ADR 0044](docs/adr/0044-machine-adapters.md).
_Avoid_: Observe as an Authorship intent; Query of the Player simulation; Message; Diagnose that returns PNG; scraping the OS or Slint window composite; calling Play frame Capture; a Host event bus or NDJSON log beside Console; a Player-hosted observation adapter; treating **Play pose preview** as Host observation v1 (it is editor viewport observation, not Capture / Play frame)

**Host observation v1**:
Capture, Play step, and Play frame. Play dump is out. **Headless** uses this same observation; no OS window is not a different API. MCP keeps granular Play Session verbs. CLI presents Play observation as one process (Play, optional steps, one Play frame, Stop, exit). Capture and Play frame leave the process as PNG: CLI writes `--out`; MCP returns ImageContent. The CLI command and the MCP tool for that still are both **play-frame** — same product, different presentation.
_Avoid_: Shipping Headless as a second observation protocol; Play dump in this slice; HWND Capture; a second Play protocol for CLI; chaining play/step/frame across CLI processes; returning stills as a filesystem path from MCP; CLI stdout as a raw PNG stream; naming the CLI episode `capture-play`

**Capture**:
A 16:9 **Scene still** (capped longest edge) returned as Host observation, not written to the Thumbnail cache. Play-rule camera, no Editor Overlays. No Camera is failure — not an Editor Camera fallback. Not Query, not a Content Browser Thumbnail, not the OS window, not Camera Preview's selected camera, not a **Play frame**.
_Avoid_: HWND screenshot; Query kind=image; Editor Camera fallback; Gizmos in the frame; writing the Thumbnail cache as this; using the Camera Preview selection; scraping the main viewport offscreen; square Capture; live window aspect; calling a Play Process still Capture

**Document History**:
The scene-scoped Editor History for one open editable document — for v1, the active scene (`activeScenePath` / active `SceneInstance`). Opening another scene replaces or clears that history. It is not Global History and does not hold editor-preference commands.
_Avoid_: Cross-scene undo continuum, surviving history after the document instance is unloaded, stuffing settings edits into the scene stack, leaving **Attachment property preview** edits off Document History

**Global History**:
The separate Editor History stack for non-document editor actions. Content Browser filesystem mutations — **New Folder**, **Rename Folder**, Browser Folder delete, Browser Folder reparent, Asset rename, and Asset delete — are Global Commands. **Mesh material override** writes from **Material Inspector** (field commits and **Reset overrides**) are Global Commands too (Mesh descriptor data, not the open scene). Independent from Document History: different filter, different jump target. Decision: [ADR 0037](docs/adr/0037-content-browser-global-history.md) (filesystem); material-override Commands extend the same stack.
_Avoid_: Merging into Document History, stuffing scene entity edits onto Global (including **Attachment property preview**), treating Browser file ops or Mesh descriptor material edits as Document History, leaving Global as an empty placeholder once those Commands exist

**History Panel**:
The dock UI that lists Editor Commands from the filtered history stacks. It lives as a sibling tab to Content Browser (filesystem) in the same tab group. Clicking an entry performs a History Jump on that entry's stack. Filters, rows, and grouping stay as authored; chrome uses Editor controls (same rule as Content Browser chrome).
_Avoid_: Output log, read-only audit trail, a floating panel unbound from the Content Browser tab group (for this milestone); cloning Unity Console as this panel

**History scope filter**:
The Scene / Global checkboxes on the History Panel. Scene shows Document History; Global shows Global History. Default: both checked. Both checked lists each stack as its own group (no interleaved merge). Both unchecked shows an empty list.
_Avoid_: Interleaved timeline merge of Scene and Global; fake filters with no backing stack; starting with Global-only checked so the panel looks broken

**History Jump**:
Seeking Document History or Global History to a chosen command by running undo/redo until the stack cursor matches that entry. The History Panel click path and keyboard Undo/Redo share the same stack APIs.
_Avoid_: Selection-only highlight without mutating the document; a second undo implementation only for the panel

**Focus-routed Undo**:
Keyboard Undo/Redo target **Global History** when the Content Browser panel has input focus, **or** when the Inspector panel has input focus and presentation is **Asset Inspector**. Otherwise **Document History** (including when an **Attachment property preview** card has focus). The two stacks stay separate; this is routing, not a merged timeline. Decision: [ADR 0037](docs/adr/0037-content-browser-global-history.md) (Browser); Asset Inspector is the same Global stack, not a third timeline.
_Avoid_: Interleaving Scene and Global into one Ctrl+Z timeline; always-Document shortcuts after Browser or Material Inspector Global Commands exist; routing Undo by whichever stack was last pushed regardless of focus; Inspector Ctrl+Z undoing the scene while an Asset is being edited in Asset Inspector

**Command label**:
The English display string for an Editor Command in the History Panel. Prefer an action plus entity name (e.g. `Move Player`, `Delete Cube`); fall back to a type-only phrase (`Set Transform`, `Spawn Entity`, `Delete Entity`) when no name is available. The entity name is snapshotted when the command is pushed; later renames do not rewrite older labels. Not a localization key in this milestone.
_Avoid_: Chinese UI copy for this panel, code identifiers as the only label, omitting the entity name when it is known, live-resolving names so rename rewrites past history rows

**History row state**:
How a Document History (or Global History) entry appears relative to the stack cursor: applied entries (at or before the cursor) use full emphasis; the redo tail (after the cursor) is visually muted (dimmed/gray). The current position is highlighted. Muted rows remain clickable for History Jump. The panel lists entries oldest-at-top, newest-at-bottom.
_Avoid_: Hiding the redo tail, identical styling for applied and redo rows with selection as the only cue, newest-first ordering for this panel

**Command target (v1)**:
Editor Commands address scene entities by `EntityId` within the active `SceneInstance`, matching current selection/gizmo/Inspector paths. ObjectId targeting is deferred until scene editing is Object-backed. The machine contract does not use this as its public target; it uses Authorship Address and translates to EntityId when pushing a Command.
_Avoid_: Requiring ObjectId for the first undo milestone, dual-ID on every Command, exposing EntityId on Query / Op / Diagnose

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
Undo/Redo are reachable via editor shortcuts (Ctrl+Z; Redo accepts Ctrl+Y and Ctrl+Shift+Z) and Edit menu items. Enabled state follows the focus-routed stack: Content Browser focus, or Inspector focus in **Asset Inspector**, uses Global History canUndo/canRedo; otherwise Document History. Inline Rename claims Ctrl+Z (text). History Jump targets the stack of the clicked row. Decision: [ADR 0037](docs/adr/0037-content-browser-global-history.md).
_Avoid_: API-only milestone with no user-facing undo; menu-only without shortcuts; merging Scene and Global into one Ctrl+Z timeline

**Linear history (v1)**:
Document History is a single linear stack. After undoing, executing a new Editor Command discards the redo tail beyond the current cursor. No branched timelines in v1.
_Avoid_: Multi-branch undo trees; preserving redo after a divergent new edit

**Axis number field**:
Reusable Inspector property-field cell: optional axis-colored letter, editable number, unit suffix, and **pointer scrubbing on the value cell** (not the row label) that applies live. **Shift** held during that scrub uses one-tenth the cell’s base scrub rate; pressing or releasing Shift mid-drag switches immediately. This slice uses it for Transform axes, Light numeric cells, Camera FOV / Near / Far, Behaviour bag numbers, SkeletonModifier numeric cells (not bone/name strings), **Color picker** channel fields (no axis letter), and the matching **Attachment property preview** rows. Compact Godot metrics (22px cell, 11px value type); the exception to Editor controls' single-line scale.
_Avoid_: Plain LineEdit; slider-only row (for Position/Scale); Unity Numeric Field; dragging the left row label to scrub (Unity label-drag); EditorCompactField for these cells; Shift-fine as an Intensity-only special case; Ctrl snap in this slice

**Inspector Transform session state**:
Editor-session UI toggles for Rotation Edit Mode (default Euler), Scale link (default on), and Multi-edit mode (default Absolute). They persist across selection changes within a run and are not saved to the scene.
_Avoid_: Per-entity UI prefs, disk-persisted Inspector toggles

**Transform section chrome**:
Godot-like Transform layout and controls: AxisNumberField (axis-colored cells, scrub, rotation under-slider), Scale-link, and Euler / Absolute·Delta layout. The Inspector panel surface fill and default window text follow Editor Theme Window. The Transform header may use an Editor controls Foldout; the vector cells themselves are not Editor controls Numeric Fields.
_Avoid_: Godot theme skin as a second palette; replacing AxisNumberField or Camera FOV rows with Unity Numeric Field; restyle entire Inspector as Unity component inspectors

### Console

**Console**:
The editor's diagnostic message list — a docked authorship panel of **Console Messages** from the **Editor Session** and from the **Play Process**. It is a viewer, not a command prompt, not an in-game overlay, not History Panel, and not the OS terminal. Default home: sibling tab to Content Browser in the same bottom dock group (not an inner tab of History).
_Avoid_: Command REPL; Unreal-style command line on this panel; tilde overlay in the Player; cloning this as History Panel; treating Win32 AllocConsole as the product Console; a Player-only or editor-only second log panel as the product surface; nesting Console inside FilesystemHistoryHost; requiring a floating Console as the default; AllocConsole as the Editor Session or Player product path

**Console Message**:
One listed diagnostic utterance in the Console. Produced in the Editor Session or in the Play Process. Not an Editor Command and not a History Panel row.
_Avoid_: Undo unit; History entry; treating stdout bytes as the authored list identity; a second Host event stream for spawn, cook fail, or Play crash

**Console origin**:
Whether a Console Message was produced in the Editor Session process or in the Play Process. The Console is one list; origin is an attribute of the message.
_Avoid_: Two dock panels for the two processes; hiding Player messages in the OS terminal instead of tagging origin

**Play log forwarding**:
Play Process diagnostics appear in the editor Console over the **Play control channel**. The author does not use the Player OS terminal as the diagnostic surface. Stop does not imply the Player rows vanish (clear is a separate gesture). Decision: [ADR 0040](docs/adr/0040-console-play-log-on-control-channel.md).
_Avoid_: Play logs only on the Player console window; requiring a networked log service; dropping Player rows automatically on Stop as the product default; a second log socket; stdout capture as the feed

**Attached terminal**:
An OS stdout/stderr stream already present when the process starts (cmd, IDE). LogSystem may write there, including `debug`. Editor Session and Player do not `AllocConsole`. Not the product Console. Without an attached terminal, `LOG_DEBUG` has no author-facing surface.
_Avoid_: AllocConsole for engine_editor or engine_player as the product path; treating the black system window as Console; requiring an attached terminal to see Log/Warning/Error

**Console severity**:
The author-facing grade of a Console Message: **Log**, **Warning**, or **Error**. The Console filter uses these three. Distinct from LogSystem's five `LogLevel` values (`debug` / `info` / `warn` / `error` / `fatal`).
_Avoid_: A five-chip Console filter; a separate Fatal chip; calling Console severity `LogLevel`; treating `debug` as a Console severity

**Console severity map**:
How `LOG_*` becomes a Console Message: `info` → Log; `warn` → Warning; `error` and `fatal` → Error. `debug` does not create a Console Message (OS terminal only). `fatal` still throws as today; the Console row is Error.
_Avoid_: Showing `LOG_DEBUG` in the Console as the product default; mapping `fatal` to a fourth grade; dropping `LOG_INFO` from the Console; mapping `Debug.Log` to `LOG_DEBUG`

**Debug API**:
The C# author API on `Blunder.Api` (`Debug.Log` / `LogWarning` / `LogError`) that writes Console Messages: Log, Warning, and Error. Each call takes a string (plus Console stack). It runs where the .NET script host runs (Play Process; env-gated editor host). Not `System.Console`, not `LOG_DEBUG`, and not Debug Project. No context Object / Ping in this slice.
_Avoid_: Mapping `Debug.Log` to `LOG_DEBUG` (which is excluded from the Console); requiring Edit Mode script host for logging; treating `Console.WriteLine` as a Console Message; naming this API Console; conflating this with Debug Project; `Debug.Log(message, object)` Ping of the editor Hierarchy; selecting an authorship entity from a Play Process ObjectId

**Console stack**:
The call stack captured with a **Debug API** Console Message. Shown in **Console detail** when that message is selected. `LOG_*` messages have no Console stack in this slice. Not an IDE jump.
_Avoid_: Requiring stacks on engine `LOG_*` for this slice; double-click-opens-IDE as this slice; putting the full stack in the list row as the only view

**Console detail**:
The Console region that shows the selected Console Message's full text and its Console stack (empty stack area for `LOG_*` rows). Distinct from the message list.
_Avoid_: A separate Stack panel; hiding the body once a stack exists

**Lifecycle exception**:
An exception that escapes an engine-invoked C# entry on a Behaviour — **Ready**, **Tick**, **OnMessage**, and host-invoked callbacks of the same class (including **PoseApplied** subscribers). The engine records it as an Error Console Message with Console stack, aborts that invocation, continues other Behaviours / remaining Message receivers, and does not end the Play Process. Auto-Stop / Error Pause is not this term.
_Avoid_: Killing the Player on the first script throw as the product default; swallowing with no Console Message; auto-Stop on exception as this slice; catching only Ready/Tick while OnMessage or PoseApplied can still kill the process

**Console collapse**:
A Console toolbar toggle. When on, Console Messages that share a **Collapse key** appear as one list row with a count. Default off. Distinct from discarding messages (Clear) and from a consecutive-only fold.
_Avoid_: Adjacent-only merge as this feature; Collapse on by default; treating Collapse as deleting the underlying messages

**Collapse key**:
The identity used by Console collapse: message text + Console severity + Console stack (empty for `LOG_*`) + Console origin. Two messages with different origins do not collapse together.
_Avoid_: Collapsing Editor Session and Play Process rows into one; ignoring stack so two call sites with the same string merge

**Console clear**:
The author gesture that removes every Console Message from the Console (both origins). Not an Editor Command (not undoable). Distinct from Stop and from Console collapse.
_Avoid_: Clear that drops only Play Process rows; undoable Clear; treating Stop as Clear

**Clear on Play**:
A Console toolbar toggle. When on, starting a Play session performs Console clear. Default on. Stop does not clear. **Play Reload** does not clear. When off, rows from earlier in the Editor Session remain.
_Avoid_: Clearing on Stop as this toggle; default off so every Play starts on a dirty list; a second toggle that clears only Player rows; treating Play Reload as a Play session start for this clear

**Error Pause**:
A Console toolbar toggle. When on, an Error Console Message with Play Process origin issues **Play Pause**. Default off. Editor Session origin Errors do not pause the Player. Distinct from Lifecycle exception (which does not pause by itself) and from Stop.
_Avoid_: Pausing the Player because an editor-origin Error was logged; default on; treating Error Pause as Stop; pausing on Warning

**Console capacity**:
The Console keeps a ring of at most **10000** Console Messages (pre-collapse emits). Pushing past the limit drops the oldest. Not a memory-byte budget.
_Avoid_: Unbounded growth; using the History stack limit of 100 as this cap; counting collapsed rows instead of emits

**Console filter**:
The Console toolbar's three Console severity toggles (Log, Warning, Error) plus a text search. Toggles default all on. Search is case-insensitive and matches message text; a row shows when its severity toggle is on and (if the search is non-empty) the text matches. Counts on the toggles are how many Console Messages of that severity sit in the ring (pre-collapse), not the visible row count.
_Avoid_: Filters off by default so the list looks empty; search that requires regex as the only mode; counting collapsed rows as the badge

**Console time**:
The local wall-clock time of a Console Message, shown on the list row as `HH:mm:ss`. A collapsed row shows the time of the latest emit that shares that Collapse key. The list is oldest-at-top, newest-at-bottom (same order as History Panel).
_Avoid_: Relative-to-Play time as this slice; hiding time unless the detail pane is open; newest-first ordering

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
A directed graph of Asset→Asset references used to invalidate Finals and to know what to Cook. v1 minimal edges: Scene→Mesh Asset; Mesh Asset→Texture Asset via the mesh descriptor’s authoritative `texture_guids` (not via paths inside mesh Intermediate alone) — that list is Import-discovered GUIDs union **Mesh material override** non-empty slot GUIDs; textures referenced only by embedded image URIs and not registered as Assets are out of graph; each Asset→its Intermediate inputs (descriptor + `source` file) as leaves for freshness. No Material Asset nodes, audio/shader edges, or Source-file parsing in v1. Consumers (e.g. Scene / AnimationTree maps)→AnimationClip use Asset References; there is **no** Mesh→AnimationClip graph edge — Clip independence is product law ([ADR 0028](docs/adr/0031-animation-clip-independent-of-mesh.md)).
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
A first-class Asset whose Intermediate body is readable YAML of skeletal TRS keyframes (bone names, times, values, interpolation). Cook produces a Final runtime clip. Distinct from Mesh Asset; not a Behaviour and not the AnimationTree. The Clip descriptor owns its Intermediate `source`; deleting a Mesh does not delete Clips. Phase 1 sampler supports **Constant** and **Linear** interpolation (glTF STEP/LINEAR map in); Cubic/Bezier later. Importing a multi-animation glTF or **Companion Animation glTFs** yields **N AnimationClip Assets** (and a Mesh Asset only when a mesh host was also Imported).
_Avoid_: Embedding durable clip authorship only inside the mesh Intermediate with no Clip Asset, treating clip bytes as Mesh payload, path-only clip identity without GUID, Phase 1 Cubic/Bezier as required, bundling all clips only inside the Mesh Asset with no Clip GUIDs, requiring a Godot-style AnimationLibrary Asset as the container for imported clips, Mesh→Clip product dependency or cascade delete from Mesh

**Clip track**:
One bone plus one TRS channel (**Position**, Rotation, or Scale) and that channel's keys on an **AnimationClip**. The Position channel is the clip's translation keys. Distinct from **Method track**. Row labels in **Clip anatomy** match **Local Transform**: Position, Rotation, Scale.
_Avoid_: Godot NodePath property tracks as the product row, Object Transform as a clip track, audio tracks, treating a Method track as a clip track, `translation` as the product row label

**Companion Animation glTF**:
An authored glTF/GLB that carries skeletal animations for a character but is not the Mesh Intermediate body (Godot-style split exports). **Acceptance:** has animations and **no meshes** (meshes empty / count zero); skins may be present (Chocomel LOOP files ship skins=1, meshes=0). Import registers independent **AnimationClip** Assets (stem names); Intermediate exchange bodies live under `Resources/Animations/<stem>/` — folders are organization only, not Mesh ownership. **Discovery gestures:** multi-select (disconnected trees) and near-disk scan (mesh dir + parent’s immediate child dirs) may Import clips alongside a skinned mesh without persisting Mesh↔Clip packaging. Same-batch bone-name mismatch against a skinned host: warn and still register. No Mesh descriptor field lists companion sources.
_Avoid_: Treating companion files as Mesh Assets, AnimationLibrary as the companion container, path-only durable clip identity, recursive whole-tree or DogWalk-hardcoded `animations/**` scans as the product default, requiring zero skins to accept a companion, requiring Godot AnimationLibrary paths for companion clip names, failing Import solely because companion bones do not match, extract-only companions with no Intermediate `source` on the Clip, forcing `Models/{mesh}/companions/` or `_standalone_companions` as the product layout, writing `companion_animation_sources` on Mesh, auto-filling AnimationTree maps as part of companion Import packaging

**AnimationPlayer**:
Retired as a product Unique attachment and as the play path. **Clip Bindings**, **TimeScale**, Travel / Start, **Fire slot**, and Edit preview live on **AnimationTree**. Engine-internal types may remain; they are not an Add… kind, not a Hierarchy Unique icon, and not an Inspector section.
_Avoid_: Add… AnimationPlayer, AnimationPlayer.Play, Clip Binding or TimeScale on AnimationPlayer, treating AnimationPlayer as a Unique attachment

**Clip Binding**:
An authored row on the **AnimationTree** clip map: logical clip name → AnimationClip Asset Reference. StateMachine states, BlendSpace points, authored OneShot, Add2, **Fire slot**, **Clip Play**, scripts that pick a **Behaviour clip name**, and Sync Group address the clip by that logical name — not by AnimationPlayer.Play, not by a map on AnimationPlayer, not by GUID on a StateMachine node. The GUID stays the durable reference inside the binding; authors assign the AnimationClip via Content Browser drop or an AnimationClip picker, they do not paste the GUID. The Inspector Clip Binding row shows logical name + assigned AnimationClip (display identity) + Remove; GUID is not shown on the regular Inspector surface (it remains the durable serialized Asset Reference inside the binding). The logical name is an authorable alias: it may differ from the Asset stem; a new binding’s name defaults to the clip’s registered stem; replacing the row’s AnimationClip keeps the existing name. Logical names on one AnimationTree are unique: an append or a rename whose default or chosen name is already taken is rejected (no silent overwrite). Retarget is drop-on-row or that row’s picker. Two different logical names MAY share the same AnimationClip Asset Reference. Content Browser drop is location-sensitive: drop on the empty list or Add clip area appends a new binding; drop on an existing row retargets only that row’s AnimationClip and keeps its logical name; drop elsewhere on Inspector (including Behaviour fields) does not mutate the map. The per-row picker is always retarget-this-row. Empty name/GUID drafts are not a product path. On scene load, rows whose logical name and Asset Reference are both empty are discarded; half-filled rows (name without clip or clip without name) are kept and shown invalid for repair. Bindings are scene assembly on the Object's AnimationTree — Import does not create them. Clip Binding fill lives on the AnimationTree Inspector. Not a Unique attachment, not a Hierarchy row icon, not a separate scene-mounted object.
_Avoid_: Treating the GUID text field as the product fill gesture, showing GUID as a regular Inspector edit field on Clip Binding rows, storing the AnimationClip Asset Reference on the Behaviour instead of the tree map, storing the clip map on AnimationPlayer, storing the clip map on a StateMachine node, Import auto-fill as packaging, Godot AnimationLibrary as the binding container, locking Play keys to file stems, overwriting the logical name when retargeting a row’s clip, treating every drop onto the AnimationTree clip list as append, treating every drop onto the AnimationTree clip list as replace, last-write-wins on duplicate logical names, forbidding two names that point at one clip, empty name→GUID rows as the Add clip result, keeping both-empty draft rows after load, silently deleting half-filled bindings on load, Clip Binding as a scene Unique or Hierarchy-mounted attachment

**Clip Play**:
An AnimationTree script operation that names a **Clip Binding** logical name and **replaces the tree base** with that one clip. **v1 is a hard cut** — no fade duration. It does not insert on the **Fire slot**, does not add an Add2 layer, and does not auto-return when the clip ends. After the last key, pose **holds that key** and the **Clip Play override** stays until **Travel** / **Start**. Travel / Start clear the replacement and drive StateMachine again (including `Travel` to the remembered current state). **Fails with no mutation** when the name is empty, the logical name is not a complete Clip Binding, or the AnimationTree is not active — it does not auto-activate. C# may still spell the method `Play`; the product term is Clip Play. Not **Play** (the Player session). **v1 ship:** AnimationTree runtime, C-ABI, and Blunder.Api, plus engine tests. Soft cuts (snapshot or mixer fade, per-edge fade), unified BlendSpec, Animation Window Clip Play chrome, Canvas edits, and DogWalk content Play bars are follow-ups, not Clip Play v1.
_Avoid_: AnimationPlayer.Play as the shipping call, Play Mode, PlayableGraph, treating Clip Play as Travel by state name, treating Clip Play as RequestOneShot, a second Unique for Clip Play, Clip Play as an extra Add2 bus, wrapping Clip Play as an AnimationClip loop field, auto-clearing override at clip end, Clip Play v1 as Crossfade, AnimationPlayer two-slot fade as Clip Play, latching Clip Play on an inactive tree, treating an unbound logical name as a deferred Clip Play

**Clip Play override**:
The runtime condition that **Clip Play** owns the tree base. At most one per AnimationTree. A new **Clip Play** **hard-cuts** this condition: the named clip replaces the previous override immediately and the clip clock starts at 0, including when the logical name is the same. The StateMachine **current state name is unchanged** but that state is not sampled. Authored **StateMachine transitions** do not auto-Travel while this is set. Script Travel / Start still run and clear the override. **Fire slot** and **Add2** still run: Fire occupies the sampled pose while active (same exclusive sample as today), then the override is the base again; Fire does not clear the override. Add2 still applies after that sample. The Clip Play clock keeps advancing during Fire. **Not serialized**: not a Scene field, not an AnimationTree Asset instance override, not Document History. Animation Window **Stop** clears it in Edit preview.
_Avoid_: Queuing multiple Clip Play bases, a Canvas-visible synthetic state for Clip Play, sampling StateMachine and Clip Play into one Local Pose in the same evaluate, Fire clearing a Clip Play override, skipping Add2 solely because a Clip Play override is set, treating same-name Clip Play as a no-op, persisting Clip Play override in the scene or Tree Asset

**Behaviour clip name**:
A Behaviour string property that addresses a **Clip Binding** by logical name on the co-located **AnimationTree** (content examples: `IdleClip`, `WalkClip`). It is not an AnimationClip Asset Reference on the Behaviour. Only members explicitly marked `[BehaviourClipName]` (ScriptsCatalog emits `kind: "clip_name"`) use the Tree logical-name dropdown; unmarked string members stay free text. Authors fill a marked field only by picking from that tree's current logical names — never by typing a GUID, never by dropping an AnimationClip onto the Behaviour field. Binding must already exist on the AnimationTree; the Behaviour field does not create or retarget Clip Bindings. It is a weak name reference: renaming or removing a Clip Binding does not rewrite Behaviour fields; a name that no longer resolves stays stored and shows as invalid until the author re-picks. When the Object has no AnimationTree or the map is empty, the dropdown still works with an empty choice only (clear allowed; no free-typed names). An empty clip name means that role does not Travel / Start a clip.
_Avoid_: Behaviour-owned clip GUIDs as the Play key, a second name→GUID map on the Behaviour, requiring a typed GUID to set Idle/Walk, drop-on-Behaviour that silently appends or retargets the tree map, free-form string typing as the primary fill for marked clip-name fields when the tree already has names, treating every Behaviour string as a clip name, hard-coding only IdleClip/WalkClip as the product rule with no general mark, cascading rename of Behaviour strings when a Clip Binding is renamed, blocking rename/remove of a binding solely because a Behaviour still names it, disabling the field until a binding exists, falling back to free text when the tree map is missing or empty

**Crossfade**:
Retired as a product play API on AnimationPlayer. Soft cuts live in **AnimationTree** (StateMachine Travel, OneShot, later per-edge fade) — not player two-slot fade. **Clip Play v1 is not a Crossfade**; it hard-cuts the base.
_Avoid_: AnimationPlayer.Play(name, fade) as the shipping cut, calling every tree weight change a Crossfade, calling Clip Play v1 a Crossfade

**Weighted dual-track blend**:
Retired as a product play path. Pose blending is **AnimationTree** (BlendSpace, StateMachine, OneShot, Add2), not player two-slot weight. Historical Phase 2 engine work used two sample slots and a single blendWeight; that is not the shipping play API.
_Avoid_: Equating dual-track blend with BlendSpace1D/2D authorship, `Play(name, fade)` as sugar over two slots, shipping two-slot Play beside Travel/Start

**TimeScale**:
A single playback-rate multiplier on the **AnimationTree** that scales tree advance (base including **Clip Play**, Fire slot, authored OneShot, and Add2 together). No per-track TimeScale. No separate Player-owned rate.
_Avoid_: Per-clip TimeScale, treating TimeScale as a Behaviour-only float with no engine advance effect, requiring per-node TimeScale, AnimationPlayer.TimeScale as the product rate

**Skeleton**:
The runtime bone hierarchy (rest/bind poses and current pose) that skinned meshes deform against. Pose is advanced by the co-located **AnimationTree** (clip names resolve on that tree's **Clip Binding** map). SkeletonModifiers may override after apply. Skeleton and AnimationTree live on the same Object; cross-Object skeleton references are out of scope. Holds the authoritative **Local Pose**; **Global Pose** and **Matrix Palette** are Animation Pipeline products derived from it (not duplicate authored pose stores).
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
The discrete pose-change boundary in Stepped (on-2s/on-3s) playback that visual systems sync to (e.g. quantized facing). Gameplay motion and input stay real-time; step-synced visuals subscribe to pose-applied timing rather than raw frame rate. Phase 1: engine emits PoseApplied after sampling; C# detects steps (e.g. frametime modulo) — the engine does not define on-2s as a builtin event. Phase 2: under two-slot blend, step detection uses the **dominant slot** playback position (higher weight, or Crossfade target while fading), still advanced under the global TimeScale — not a blended time clock and not a separate engine AnimationStep event. Phase 4: with **AnimationTree** active, PoseApplied still fires after tree sample; step detection uses the **base dominant clip** clock (BlendSpace1D: highest-weight neighbor; StateMachine: current state's clip/dominant point; while **Clip Play override** is set: the Clip Play clip; while OneShot plays: the OneShot clip) — **Add2 does not** supply the step clock; still under global TimeScale.
_Avoid_: Updating facing/blend visuals every render frame as the DogWalk look, conflating physics tick with animation step, requiring engine-builtin AnimationStep events for Phase 1, blending two clip times into one step clock in Phase 2, blending BlendSpace neighbor times into one step clock in Phase 4, using Add2 as the Stepped clock source, dropping PoseApplied while AnimationTree is active

**ValueSlicer**:
A C# gameplay utility that quantizes a continuous value into slices with hysteresis so Stepped visuals do not flicker at boundaries. Not an engine ClassDB type in Phase 1; scripts own DogWalk-style stepping policy.
_Avoid_: Baking ValueSlicer into the engine as the only facing model, overwriting gameplay floats with sliced visuals in place

**DogWalk animation Phase 1**:
The first engine+content milestone after the Move-only DogWalk character slice: P0 skeletal skinning + AnimationClip playback, plus P1 Stepped feel via C# ValueSlicer and Animation step sync — not full AnimationTree, SYNC/CINE, or procedural bone modifiers. Per-frame order: Behaviour Tick (gameplay Play/move) → AnimationPlayer sample → PoseApplied (step-synced visuals). Engineering gate: minimal skinned test rig + idle/walk. **Done criteria** also require Chocomel (or an agreed subset) Play acceptance: idle↔walk hard cut, real-time move, stepped facing. **Edit Mode** may preview pose through **AnimationTree** (not AnimationPlayer-only clip scrub) without starting the .NET host or Behaviour Tick; Stepped feel (ValueSlicer) is Play-validated. Phase 2 engine work **may proceed in parallel** with unfinished Phase 1 Chocomel hard-cut acceptance; Phase 1 Done criteria themselves are not rewritten away.
_Avoid_: Shipping full Godot AnimationTree parity as Phase 1, treating Phase 1 as content-only with no engine animation runtime, reading final skeleton pose inside Tick as the supported Phase 1 pattern, declaring Phase 1 done on test-rig-only without Chocomel feel acceptance, Play-Mode-only clip visibility as the Phase 1 editor rule, in-editor Behaviour Tick as required for Edit clip preview, silently dropping Phase 1 Chocomel hard-cut Done when starting Phase 2

**DogWalk animation Phase 2**:
The milestone after Phase 1 that adds a **unified two-slot** playback model (**weighted dual-track blend** via single blendWeight ∈ [0,1] + **Crossfade** as weight ramps; local TRS lerp / rotation slerp) and a **global TimeScale** on the AnimationPlayer — **SYNC/CINE** land in **Phase 3**; still without AnimationTree / state machine / BlendSpace graph, additive layers, or procedural bone modifiers. Phase 1 **hard cut** remains: `Play` with fade duration 0 (or equivalent default) snaps slots/weights immediately; fade > 0 Crossfades on the same model. Animation step sync under blend uses the **dominant slot** clock. **Edit Mode** authorship controls may scrub TimeScale, fade, and two-slot weights without DotNetHost / Behaviour Tick; Stepped facing remains Play-validated. Scenes persist clip name→GUID map plus **defaults** (TimeScale, optional default slots + initial weight). **Done criteria**: engineering gate on test-rig (or equivalent) for two-slot blend, Crossfade, TimeScale, and dominant-slot step sync, **plus** Chocomel (or agreed subset) Play acceptance with weighted idle↔walk (not hard-cut-only), perceptible TimeScale, and stepped facing still on the dominant slot. Engine implementation **may start in parallel** with unfinished Phase 1 Chocomel hard-cut work. Decision record: [ADR 0020](docs/adr/0020-animation-player-two-slot-blend.md).
_Avoid_: Equating Phase 2 with full Godot AnimationTree parity, folding state machines into Phase 2 by default, requiring per-track TimeScale in Phase 2, treating Crossfade as a third sample path beside the two slots, removing hard-cut Play as the Phase 2 default path, requiring additive blend in Phase 2, requiring Edit Mode to match Play Stepped feel as Phase 2 Done, declaring Phase 2 done on test-rig-only without Chocomel blend feel acceptance, folding SYNC/CINE into Phase 2

**Sync Group**:
An engine-owned **runtime** set of **AnimationTree** members that can be triggered so members **start (or seek) at the same logical moment**. Created, joined, fired, and released by scripts/API for the session (or until explicitly destroyed) — **not** a required serialized scene graph of members. A **Fire** carries **per-member instructions** `(AnimationTree, clipName[, seek])` (heterogeneous clip names are first-class). Same logical name across all members MAY be offered as convenience sugar, not the only path. **Fire** applies to that tree's **Fire slot** (not StateMachine Travel, not by deactivating the tree, not by requiring an authored **OneShot** node). A member with an inactive Tree **fails that instruction** (visible — log / API result, not silent skip). Other members in the same Fire that are active still insert on their Fire slot. The group does not roll back. Decision record: [ADR 0023](docs/adr/0023-animation-sync-group.md). Used for DogWalk-style multi-Object coordination (character + prop + partner). Not a shared continuous playback head across members, not a cutscene timeline, and not a second playback Unique beside AnimationTree.
_Avoid_: Relying only on ad-hoc same-Tick Play calls as the supported SYNC product path, a single shared sample clock across all members as the default, treating Sync Group as a full sequencer/director, requiring a baked scene-component member list as the only way to form a group, making one Tree a permanent master with followers, requiring every synced Object to share one identical clip logical name, making a shared Crossfade fade the default Sync Group Fire path, requiring Fire to deactivate AnimationTree as the default path, AnimationPlayer.Play as a Fire fallback, Fire as StateMachine Travel, silent-skip of an inactive member, aborting the whole Fire because one member is inactive, requiring an authored OneShot node before Fire can insert, Sync members as AnimationPlayer Uniques

**SYNC**:
The Phase 3 authorship concern for **coordinated multi-Object AnimationTree starts** via a **Sync Group** (clip names may use a `SYNC-` prefix by convention). Distinct from **CINE** (over-scene / handoff clips) and from Animation step / Stepped facing sync.
_Avoid_: Equating SYNC with Stepped pose sync, treating `SYNC-` filename prefix alone as an engine feature, Sync Fire via AnimationPlayer.Play

**CINE**:
A Phase 3 **cinematic segment** contract: a short authored takeover (often prop/partner clips named `CINE-*`) that **enters** with pose/control handoff and **exits** returning control to gameplay. Engine provides thin start/end hooks plus optional **in-CINE** marking / gameplay-input suppression; **pose alignment and gameplay state transitions stay in C# Behaviours**. **Segment end is authoritative via an explicit End API** (scripts call end); member `finished` signals may assist but do not alone define exit. Playback still uses AnimationPlayer (+ **Sync Group** when multiple Objects must start together). Not a multi-track cutscene director.
_Avoid_: Equating CINE with a full Cutscene Director / timeline editor, treating `CINE-` prefix alone as an engine type, requiring AnimationTree OneShot for every CINE segment, baking DogWalk state machines or automatic TRS snap/restore into the engine as the Phase 3 path, requiring “all members finished” or a fixed wall-clock duration as the only way to end a CINE segment

**DogWalk animation Phase 3**:
The milestone after Phase 2 that adds **SYNC** (**Sync Group** runtime coordination of multi-Object AnimationPlayer starts) and **CINE** (short cinematic segment contract with thin enter/exit hooks and optional gameplay-input suppression). Still without AnimationTree / state machine / BlendSpace graph, additive layers, procedural bone modifiers, or a full cutscene director. Phase 1 co-located Skeleton↔AnimationPlayer and Phase 2 two-slot blend / TimeScale remain. **Edit Mode** supports Sync Group Fire and CINE Enter/End preview: authors can see **in-CINE** / input-suppression marking and multi-Skeleton playback **without** DotNetHost / Behaviour Tick, and **without** automatic Object TRS or gameplay state-machine handoff (those remain Play / C#). **Done criteria**: engineering gate for Sync Group Fire (per-member clip instructions) + CINE in/out hooks, **plus** a DogWalk-style mini Play acceptance (character + prop/partner synchronized start and CINE handoff returning control — simplified props OK). Engine work **may proceed in parallel** with unfinished Phase 2 Chocomel weighted acceptance; Phase 2 Done criteria are not cancelled. Decision record: [ADR 0023](docs/adr/0023-animation-sync-group.md).
_Avoid_: Shipping AnimationTree / BlendSpace as Phase 3, treating SYNC as Stepped facing sync, requiring full Chocomel/Pinda ending sequences as the only acceptance bar, declaring Phase 3 done on Sync Group unit tests alone without the mini multi-Object Play bar, silently dropping Phase 2 weighted Chocomel Done when starting Phase 3, Play-only SYNC/CINE with no Edit multi-Object preview path, requiring Edit Mode to auto-snap Object TRS or run gameplay state machines for CINE preview

**AnimationTree**:
The engine-owned ClassDB **playback graph**, co-located with **Skeleton**. It is the product play path: scripts drive pose with `Travel` / `Start` (plus per-node BlendSpace scalars, `RequestOneShot`, Add2 setters) and **Clip Play**. Clip Play lives on this same Unique — it names a **Clip Binding**, not a StateMachine state, and is not a second play path. Without a co-located tree, or when the tree is not active, the Skeleton is not sampled from clips. It owns the **Clip Binding** map and **TimeScale**. It samples **StateMachine**, **BlendSpace1D**, **OneShot**, **Add2**. **Sample stack:** base pose from the StateMachine (a state may be BlendSpace1D or a single clip; OneShot may insert over that base) then optional Add2 — except while **Clip Play** is active, the base is that one clip instead of the StateMachine. Additive deltas are relative to bind/rest. Not Godot-style `parameters/…` paths as the primary surface. Graph topology is scene-embedded. Edit Mode may activate the tree and scrub those drives (plus TimeScale, **Fire slot**) without Behaviour Tick. Every tree has a **Fire slot**. Sync Group **Fire** and script **RequestOneShot** insert on that slot. **Edit animation preview** uses this same tree path. There is no product AnimationPlayer Unique. Decision records: [ADR 0025](docs/adr/0025-animation-tree-phase-4.md) — playback exclusivity still holds; the Phase 1–3 Player.Play fallback is retired; [ADR 0045](docs/adr/0045-code-first-play-on-animation-tree.md) — no Animancer Unique.
_Avoid_: AnimationPlayer.Play as the product play path, hosting the graph only inside AnimationPlayer, Clip Binding map on AnimationPlayer, Clip Binding map on a StateMachine node, TimeScale on AnimationPlayer, C#-only graph topology as the product path, AnimationLibrary as the graph host, using AnimationGraph as a second product name, Animancer as a Unique attachment, Unity PlayableGraph as a product play path, a second playback Unique beside AnimationTree, letting Player two-slot write bones, feeding Tree output into Player slots, deactivating the tree as the default Sync Fire path, treating Add2 as a lerp dual-track slot, Godot AnimationTree parameter-path APIs as the primary script surface, requiring a standalone AnimationTree Asset as the only way to have a tree

**BlendSpace1D**:
A Phase 4 **AnimationTree** node that blends among discrete authored clip points along **one** scalar parameter (DogWalk: speed-like walk/trot/run). Neighbor blends use the same local TRS lerp / rotation slerp family as Phase 2 — not additive. Points reference clips by **logical name** on the co-located AnimationTree map. Not BlendSpace2D.
_Avoid_: Equating Phase 2 two-slot weight with BlendSpace1D authorship, requiring BlendSpace2D in Phase 4, using Add2 as the speed blend mechanism, requiring a standalone BlendSpace Asset as Phase 4 Done

**StateMachine** (animation):
A Phase 4 **AnimationTree** node that selects among **named animation states** (each state's playback is a BlendSpace1D, BlendSpace2D, or a single clip). Phase 4/5 primary drive is script **Travel** / **Start**. With **AnimationTree Canvas** v1, the StateMachine MAY also auto-Travel via authored **StateMachine transitions** (parameter conditions), except while a **Clip Play override** is set. Distinct from the gameplay / character state machine in C# Behaviours. **Clip Play** does not address these state names.
_Avoid_: Merging gameplay state machine into the AnimationTree as one type, requiring Godot-complete transition graphs and nested state machines as Phase 4 Done, removing Travel/Start when transitions exist without an explicit product decision, treating Clip Play as Travel by state name, auto-Travel while a Clip Play override is set

**OneShot**:
A Phase 4 **AnimationTree** node that **inserts** a clip (or short sub-pose) over the base graph, then **returns** to the underlying StateMachine/BlendSpace base. Used for authored trip-like interrupts in the graph. Sync Group **Fire** and script **RequestOneShot** use the **Fire slot**, not this node. **Clip Play** is not a OneShot: it replaces base and does not return by itself.
_Avoid_: Equating every Crossfade with OneShot, requiring AnimationPlayer fade as the only interrupt path, treating CINE Enter as an engine OneShot, requiring an authored OneShot node for Fire or RequestOneShot, treating Clip Play as OneShot

**Fire slot**:
The always-present OneShot insert on every AnimationTree. Sync Group Fire and script **RequestOneShot** play a clip there, then return to the current base (empty graph: rest; **Clip Play override**: that Play clip, possibly already on its last key). A new Fire or RequestOneShot **hard-cuts** the slot: the new clip replaces the in-flight insert immediately (Fire seek still applies); the old insert is dropped. No queue. Occupied slot does not fail the new request. Authors do not place this slot. Distinct from an authored **OneShot** node. Fire does not clear a Clip Play override.
_Avoid_: Fire as Travel, empty-graph Fire failing solely because no OneShot node exists, RequestOneShot targeting only authored OneShot nodes, queuing Fire-slot clips, failing a Fire because the slot is already playing, a second clip map on AnimationPlayer or on a StateMachine node, Fire as the way to clear Clip Play

**Animation Window**:
Persistent docked chrome for **Edit animation preview** on the current single selection's **AnimationTree**. Transport and timeline scrub advance that tree; the ruler is the **base dominant clip** clock (insert clip while Fire/OneShot occupies; **Clip Play override** clip while that override is set and Fire is not occupying). The dock stays open when unbound (no selection, multi-select, or no Tree) with transport and timeline disabled. Changing Hierarchy selection **Stop**s the previously bound tree (ruler to 0, clear Fire slot, clear **Clip Play override**, halt transport, **End CINE**) then binds the new single Tree selection or the empty state; an unbound previous tree does not keep preview-advancing. **Stop** itself also **End**s CINE. The **End** control exits CINE only (clears in-CINE and input-suppression marks) without seeking or clearing Fire slot or Clip Play override. **Enter** is the CINE session mark only — it does not Fire a clip. Window **Loop** is Edit preview wrap of the current ruler clip while Playing; with Loop off, reaching the end Pauses on the last frame and leaves the tree active. **Stop** seeks the ruler to 0, clears the **Fire slot** insert, clears **Clip Play override**, **End**s CINE, and leaves the tree active. **Play** activates an inactive tree before advancing; Play from last-frame Pause starts again at 0. Loop is not a durable flag on **AnimationClip** and not AnimationPlayer loop. v1 also shows the clock source's logical name, **TimeScale** (the same AnimationTree field as Inspector — dual-track, document-dirty; the only Animation Window control on **Document History**), Edit **CINE** enter-end plus in-CINE and input-suppression marks, and a **Fire target** dropdown of that tree's existing Clip Binding logical names. Transport **Play / Pause / Stop / Loop**, **Fire**, and **Enter / End CINE** follow **Icon-first chrome**. Transport, playhead, Loop, Fire slot occupancy, Clip Play override, and CINE marks are session preview and do not dirty the document. Window **Fire** inserts on that tree's **Fire slot**; it is not Sync Group `fireSameName`. There is **no v1 Clip Play control** in this window. The dropdown picks the Fire instruction only — it does not retarget the playhead, Travel, author bindings, or Clip Play. The timeline is a ruler and playhead plus **Clip anatomy**. Replaces the overlay preview toolbar. Defaults to a bottom dock under the viewport (Godot Animation-panel rhythm) as its own dock panel kind; authors may retile it. Clip Binding authorship, Travel/BlendSpace/Add2, **AnimationTree Canvas**, and multi-object Sync Group Edit stay elsewhere.
_Avoid_: AnimationPlayer as host; S0/S1/BW/Fd; Canvas inside this window; keyframe/track authorship; Clip Binding rows in this window; Travel/BlendSpace/Add2 in this window; Camera Preview pin; Godot auto-reveal/hide; a second overlay toolbar; a playhead tied to a picked Clip Binding while another clip is dominant; window Fire as Sync Group fireSameName; a Sync member list in this window; Loop as an AnimationClip Asset field; AnimationPlayer.m_loop as the preview wrap; Stop deactivating the tree; a preview-only TimeScale that is not the Tree field; keeping v1 as a viewport overlay instead of a bottom dock; putting transport/playhead/Loop/Fire/CINE on Document History; continuing preview on a tree after selection leaves it; leaving CINE marks on after Stop or rebind; Enter CINE as a Fire; a Clip Play dropdown beside Fire in v1; Stop leaving a Clip Play override set

**Clip anatomy**:
The read-only Animation Window listing of the **clip tracks that already exist** on the current ruler **AnimationClip**, plus those tracks' key times on the timeline. Rows are grouped by bone name; each group lists that bone's existing **clip tracks**. Bone groups follow the bone's first appearance in the clip; within a group the channel order is Position, then Rotation, then Scale, omitting channels the clip does not have. Missing TRS channels are omitted (no Skeleton × Position/Rotation/Scale fill). Channel rows show the Position / Rotation / Scale **Editor Icon** plus that word. Bone group titles are the bone name only (no Skeleton glyph). Key times are drawn as diamonds on that clip-track's lane; diamonds are not selectable or draggable. **Method tracks** are not this surface. Not clip authorship and not a second play host.
_Avoid_: Inserting, deleting, or retiming keys or tracks in this window; mute/enable, interpolation, wrap, or delete controls as this surface; Godot AnimationPlayer as host; treating Object Transform tracks as clip tracks; synthesizing empty clip tracks for unanimated bones; Method track rows or method-key diamonds in Clip anatomy; a Skeleton3D or scene-tree root as the group parent; Skeleton-hierarchy or alphabetic reorder of bone groups; a selected or dragged key diamond

**Edit animation preview**:
Authorship-viewport pose preview without Behaviour Tick. Chrome is the **Animation Window**. Always through the co-located **AnimationTree** (Travel / Start, **Fire slot**, named BlendSpace drives, TimeScale; **Clip Play** has no v1 Window control). No AnimationPlayer-only clip scrub. No Tree, or Tree inactive, means preview fails — same as Play.
_Avoid_: Edit toolbar Play/Stop on AnimationPlayer, bypassing the tree to scrub a Clip Binding onto the Skeleton, treating Edit preview as Camera Preview, a second overlay toolbar beside the Animation Window

**Add2**:
A Phase 4 **AnimationTree** additive layer that combines an additive clip/pose onto the **base** pose. Additive deltas are relative to **bind/rest**. Used for DogWalk-style turn / bark overlays on locomotion. Not a third Phase 2 sample slot and not BlendSpace. Still applies when the base is a **Clip Play override** or a Fire-slot sample.
_Avoid_: Calling Phase 2 dual-track blend Add2, requiring N independent additive buses as Phase 4 Done, using world-space additive as the Phase 4 default reference, skipping Add2 solely because a Clip Play override is set

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
_Avoid_: AnimationLibrary as the tree container, requiring a visual graph editor to create a Tree Asset for Phase 5 Done, path-only tree identity without GUID, ignoring embedded topology when no Asset is set, treating full in-scene graph copy as the primary reuse path when an Asset reference exists, using the canvas to edit scene-embed as the product truth source when an Asset is in play, storing Clip Play override as an instance override

**AnimationTree Canvas**:
A Phase 7 visual node-graph editor whose **product truth source** is the **AnimationTree Asset** (create/edit topology on the Asset body). Scene instances keep Asset reference + small overrides; Phase 4 embed-only remains when there is no Asset. **v1 Done node set** matches Phase 5 graph capabilities: **StateMachine**, **BlendSpace1D**, **BlendSpace2D**, **OneShot**, **Add2** — not a Godot-complete tree. **Authorship:** Canvas and Inspector are **dual-track** — both may edit the same Asset topology; neither is read-only after Canvas ships. **Editor layout** (node positions / view) persists **in the Asset body** with topology — not only as per-machine editor prefs. **v1 preview:** topology-only — live Skeleton preview while editing the canvas is **not** required for Done (use a scene Object referencing the Asset + existing Edit scrub); **bound preview character** on the canvas is an explicit follow-up. **Open paths (v1):** Content Browser on an AnimationTree Asset **and** Inspector on a scene Object that references that Asset — both edit the same Asset. **StateMachine on canvas:** v1 includes a **real transition graph** (edges drive runtime), not decoration-only links. Distinct from a Godot-complete AnimationTree editor. Decision record: [ADR 0030](docs/adr/0030-animation-tree-canvas-phase-7.md).
_Avoid_: Calling scene-embed JSON hand-edit the Canvas, requiring Canvas for Phase 4/5/6 Done, treating Canvas and full in-scene graph duplicate as the same authorship path, shipping Godot-parity transition graphs / extra node types beyond the locked v1 surface as Canvas Done, leaving BlendSpace2D Inspector-only while 1D is canvas-only as the product split, removing Inspector topology authorship as soon as Canvas lands, storing canvas layout only in ephemeral local prefs as the product path, requiring an in-canvas mini viewport or bound preview Object as Canvas v1 Done, supporting only Asset-browser or only Object-Inspector open as the sole v1 entry, treating transition edges as visual-only with no runtime effect

**StateMachine transition (Canvas v1)**:
A directed edge on the AnimationTree StateMachine that can **automatically Travel** when its **condition** holds. **v1 condition shape:** a **single predicate** per edge — `param op value` with ops `==` `!=` `<` `<=` `>` `>=`, or a bool param truth check — not AND/OR bundles or a free expression language on one edge (compose via multiple edges + priority). Conditions evaluate on the animation advance path — not decoration-only, and not clip-end as the only v1 trigger (clip-end auto edges remain a possible follow-up). **Condition inputs (v1):** a **hybrid** — may read existing named tree drives (BlendSpace1D scalar, BlendSpace2D x/y, Add2 weight, etc.) **and** a small set of independent bool/float **tree parameters** set by script/Inspector. **Conflict rule:** when multiple outgoing edges from the current state are true in one evaluation, pick the highest author **priority**; ties break by a stable order (e.g. declaration order). **Switch pose (v1):** auto transition applies a **hard cut** into the target state (same family as today's Travel) — no per-edge fade required for Done (edge fade is a follow-up). **Script `Travel` / `Start` remain first-class** and MAY force a state change even when no transition edge applies, including while a **Clip Play override** is set (that Travel / Start clears the override). Distinct from Sync Group Fire / OneShot.
_Avoid_: Decoration-only edges as the product transition model, requiring full Godot expression/transition stacks as v1, treating Sync Group Fire or OneShot as a StateMachine transition edge, removing Travel/Start as the only switch path once transitions ship, requiring every transition condition to invent a parallel param when a BlendSpace drive already exists, forbidding independent bool flags for non-blend gameplay signals, leaving multi-true edges undefined or random, requiring per-edge Crossfade/mixer as Canvas v1 Done, routing tree state switches through AnimationPlayer two-slot Crossfade while the tree is active, requiring AND/OR or scripted expressions on a single edge as Phase 7 Done, auto-Travel while a Clip Play override is set

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
Creating a new starter Scene Asset on disk (new GUID) under the **Browser folder context**, then opening it as the active scene (with the existing dirty-open prompt if the current document is dirty). The starter document is not entity-empty: it includes one default **Main Camera** entity (`isMain: true`, engine CameraComponent defaults) and a second default entity with a **Directional Light** (not the Camera entity), placed above the XY ground with its **Light emit axis** slanted toward that plane so the ground is lit. Play / Scene Thumbnail resolve a camera and have lighting out of the box. Distinct from Duplicate Scene Asset and from Save As. Primary entry: Content Browser Create/New Scene, including a folder/empty-area right-click menu (**New Scene**) on the grid and the folder tree, sharing **Browser folder context** with New Folder. Name: auto `NewScene.scene.asset` in that context folder, with `_1`, `_2`, … on collision — no naming dialog in this slice.
_Avoid_: Truly entity-empty New as the product default; omitting Main Camera; omitting the default Directional Light; putting the starter Directional Light on the Main Camera entity; identity rotation at the origin for the starter Directional; cloning the current document; treating New as Save As; creating without opening; putting New only on the editor top bar; requiring a name dialog for v1; shipping meshes as part of the New starter; New Scene and New Folder using different parents from the same menu

**Duplicate Scene Asset**:
Copying an existing on-disk Scene Asset to a new path with a new GUID, preserving the source file's authored content (not the live dirty SceneInstance unless that content was already saved). Selects the new entry in the Content Browser and does not open it or change the active scene. Distinct from Save As. Primary entry: Content Browser action on a selected `.scene.asset`, including that asset's right-click menu (**Open**, **Duplicate**, **Rename**, **Delete**). Non-scene Assets' right-click exposes **Rename** and **Delete** in this slice. Name: same folder as the source, `{stem}_Copy.scene.asset`, with `_Copy_1`, … on collision — no naming dialog in this slice.
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
