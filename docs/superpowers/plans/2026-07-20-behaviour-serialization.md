# Behaviour serialization / scene mount

> OpenSpec: `openspec/changes/behaviour-serialization/`  
> Branch: `feat/behaviour-serialization`

## Global Constraints

- TDD for every behavior change; watch tests fail first.
- Prerequisite: single ObjectDB (`RegisterNativeAbi` / process fill) already on main.
- Behaviours only create Objects when list non-empty.
- Persist BehaviourId; advance next-id past max restored.
- Mount peers only when DotNetHost running; load must succeed offline.
- Property bag: bool/number/string only; skip unknown.
- No Inspector UX, no hot reload, no Play UI.
- Scoped commits; do not commit unrelated WIP (Assets/PM/history).
- Mark OpenSpec checkboxes in `openspec/changes/behaviour-serialization/tasks.md`.
- Subagent model: `cursor-grok-4.5-high`.

---

### Task 1: Schema + serializer (OpenSpec 1.1–1.3)

**Files:** `scene.h`, `scene_serializer.cpp` (+ header if needed), new/extended test

**TDD:**
1. RED: round-trip JSON with `behaviours` array fails / missing fields.
2. GREEN: `SceneBehaviourDeclaration { type, id, properties }`; parse/write; legacy scenes OK.
3. Commit: `feat(scene): serialize Behaviour list on entities`

Mark 1.1–1.3.

---

### Task 2: Instantiate Object bind + slots (OpenSpec 2.1–2.3)

**Files:** `scene_instance.cpp` (or scene_system), Object restore API, test

**TDD:**
1. RED: load scene with behaviours → no Object/slots.
2. GREEN: create/bind Object to EntityId; restore slots+ids; advance next id; null peers without host.
3. Commit: `feat(scene): bind Objects and restore Behaviour slots on load`

Mark 2.1–2.3.

---

### Task 3: Mount when host running (OpenSpec 3.1–3.3)

**Files:** mount helper, DotNetHost/ScriptHost integration, test (editor_dotnet_host style)

**TDD:**
1. RED: load+host does not attach peers / Tick.
2. GREEN: AttachBehaviour in order; apply minimal property bag; Tick/peer assertion.
3. Commit: `feat(script): mount scene Behaviours when DotNetHost running`

Mark 3.1–3.3.

---

### Task 4: Export / save (OpenSpec 4.1–4.3)

**Files:** EntityId→Object lookup, scene export from SceneInstance, tombstone check

**TDD:**
1. RED: export omits behaviours / wrong source.
2. GREEN: write from bound Object; tombstones still omitted.
3. Commit: `feat(scene): export Behaviours from bound Objects`

Mark 4.1–4.3.

---

### Task 5: Docs + verify (OpenSpec 5.1–5.3)

Update testing.md / ADR 0011 or CONTEXT; note unify-script-objectdb prerequisite done; run serializer+mount tests green.

Commit if needed: `docs(script): Behaviour serialization slice notes`

Mark 5.1–5.3.
