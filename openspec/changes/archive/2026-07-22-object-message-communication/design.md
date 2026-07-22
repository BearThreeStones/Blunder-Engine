## Context

Blunder already has ObjectId identity, ordered Behaviours with Script Peers, and LifecycleDispatch (Ready/Tick) via a type-level hook registered by ScriptHost. Cross-Object gameplay still lacks a Harmon-style directed message channel. Glossary and ADR 0017 lock the product rules from the grilling session.

## Goals / Non-Goals

**Goals:**
- Game-registered MessageId + synchronous directed Send to one ObjectId
- Fan-out to every Behaviour with a peer on that Object (list snapshot; skip null/gone peers)
- Managed `Message.Register` / `Message.Send` and `Behaviour.OnMessage`
- Native `MessageDispatch` as the single delivery path (C# calls it now; C++ can later)
- NativeAbi / C-ABI extension (version **4**) for register, send, message hook

**Non-Goals:**
- Signal Connect/Emit, broadcast/subtree, deferred Post product API
- Query Messages / out-param GETPOS
- Engine-builtin Hit/Damage vocabulary
- Physics or other native systems calling Send in this slice
- Named `OnDamage`-style receive methods
- Unbounded arg arrays; Vec3/Quat/String as first-slice wire types (Bool/Int/Float/ObjectId only on the C-ABI wire)

## Decisions

### 1. Native owns delivery; C# is the MVP caller
**Choice:** `MessageDispatch::send` in C++ looks up ObjectDB, snapshots BehaviourId list, invokes a process-wide message hook per peer (same pattern as LifecycleDispatch).
**Why:** Same path for future native senders; avoids a second managed-only fan-out that drifts from C++.
**Rejected:** Pure-managed Send that walks `ObjectHandle` Behaviour lists only.

### 2. Wire args are a fixed MessageArg tagged union (�?)
**Choice:** C-ABI `BlunderMessageArg` with kinds Nil/Bool/Int/Float/ObjectId; managed API exposes the same kinds (product language still calls this the Variant argument bag subset).
**Why:** Full C++ `Variant` (String/Vec3) across the hook is heavy for MVP; four slots match ADR.
**Rejected:** Raw Harmon `(int,int)` casts; unbounded `Variant[]`.

### 3. Register is idempotent by name
**Choice:** `Message.Register("Hit")` returns stable MessageId; re-register same name returns same id.
**Why:** Behaviours can Register in static constructors safely.
**Rejected:** Compile-only enums as the only path.

### 4. Invalid target is a no-op
**Choice:** Send to invalid/destroyed ObjectId returns OK and delivers to nobody.
**Why:** Matches “safely ignore�? avoids throwing across gameplay hot paths.

### 5. Message hook cleared with host shutdown
**Choice:** `blunder_message_clear_hook` (or clear alongside lifecycle clear in ScriptHost `ShutdownCleanup`).
**Why:** Avoid dangling unmanaged function pointers after CoreCLR teardown.

### 6. ABI bump to 4
**Choice:** Add `message_register`, `message_send`, `message_set_hook`, `message_clear_hook` to `BlunderNativeAbi`; bump `BLUNDER_ENGINE_C_ABI_VERSION` to 4.
**Why:** Same table as Input/Lifecycle; completeness checks stay one place.

## Risks / Trade-offs

- **[Risk]** Reentrant Destroy during OnMessage �?**Mitigation:** snapshot BehaviourIds before fan-out; re-resolve peer each step; skip if Object/peer gone.
- **[Risk]** Authors use Message for queries �?**Mitigation:** glossary/ADR forbid; no out-param API.
- **[Risk]** Arg type subset feels incomplete �?**Mitigation:** document follow-on for String/Vec3; ObjectId covers “who did it.�?
- **[Trade-off]** Sync Send can deep-recurse �?Accept for MVP; Post later if needed.

## Migration Plan

- New symbols only; existing hosts must rebuild against ABI v4.
- No scene format change.
- Rollback: revert NativeAbi fields and ignore Message APIs (unused until games call them).

## Open Questions

None for MVP �?grilling locked the product rules. Follow-ons (Post, Signal, String args, native physics Send) stay explicit non-goals.
