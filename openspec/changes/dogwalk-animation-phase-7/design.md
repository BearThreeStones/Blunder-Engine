## Context

Phase 5 locked AnimationTree Asset + Inspector (canvas optional) ([ADR 0026](../../../docs/adr/0026-animation-phase-5.md)); Phase 6 productized SkeletonModifiers and again excluded Tree canvas ([ADR 0027](../../../docs/adr/0027-animation-phase-6.md)). Authors still lack a graph UI, and StateMachine switches are script-Travel-only. Grilling locked **DogWalk animation Phase 7** ([ADR 0030](../../../docs/adr/0030-animation-tree-canvas-phase-7.md); CONTEXT AnimationTree Canvas / StateMachine transition / Phase 7). Stakeholders: runtime animation, Asset pipeline, Inspector, editor UI, C-ABI / Blunder.Api, Test Project. Phase 1–6 content Done gates may remain open.

## Goals / Non-Goals

**Goals:**

- AnimationTree Canvas v1 editing **AnimationTree Asset** topology + layout
- Dual-track with Inspector; Asset + Object open paths
- Phase 5 node set on canvas (StateMachine, BlendSpace1D/2D, OneShot, Add2)
- StateMachine **conditional transitions** (hybrid inputs, single predicate, priority, hard-cut)
- Travel/Start remain first-class
- Engineering + Edit authorship + lean Play (auto state change via conditions)

**Non-Goals:**

- Godot-complete AnimationTree / nested SM parity
- Canvas as editor of scene-embed truth when Asset is in play
- Removing Inspector topology authorship
- Bound live preview character / in-canvas mini viewport as Done
- Clip-end-only as sole transition trigger; per-edge fade; AND/OR on one edge
- Closing Phase 1–6 content Done via Phase 7 alone

## Decisions

1. **Phase 7 milestone (ADR 0030)**  
   New phase — not a silent Phase 5 D patch.  
   *Alternatives:* Phase 5 reopen; unnumbered OpenSpec-only.

2. **Canvas truth = AnimationTree Asset**  
   Scene keeps reference + small overrides; embed-only when no Asset.  
   *Alternatives:* embed-only canvas; both as dual truth.

3. **Dual-track Inspector + Canvas**  
   Both write the same Asset body.  
   *Alternatives:* Canvas-primary; Inspector read-only.

4. **Layout in Asset body**  
   Node positions travel with the Asset.  
   *Alternatives:* local prefs only; sidecar meta.

5. **Topology-only preview for Done**  
   Bound preview character is follow-up.  
   *Alternatives:* require live preview as Done.

6. **Both open paths**  
   Content Browser Asset + Object Inspector “open Canvas”.  
   *Alternatives:* Asset-only entry.

7. **Real transitions + single predicate + hybrid inputs + priority + hard-cut**  
   Runtime auto Travel; Travel/Start coexist.  
   *Alternatives:* decoration edges; clip-end-only; expressions; edge Crossfade.

8. **Done = eng/Edit + lean Play**  
   Earlier content gates stay separate.  
   *Alternatives:* engineering-only; full Chocomel/Pinda bar.

## Risks / Trade-offs

- [Transition eval vs sample order] → Evaluate outgoing edges on animation advance after drives/params update, before/at state sample; document one-frame rules
- [Param name collisions with BlendSpace node names] → Namespace or explicit “drive ref vs tree param” in condition binding; Inspector lists both
- [Canvas UI cost in Slint] → Start with lean node/edge widgets; no Godot feature parity
- [Dual-track merge conflicts] → Same Asset document; last-write / dirty flag; round-trip tests
- [Phase 1–6 content still open] → Checklist regression section; do not rewrite earlier Done
- [Hard-cut feel] → Accept for v1; edge fade follow-up

## Migration Plan

1. ADR 0030 + OpenSpec change (this propose)
2. Runtime: tree params + transition eval + Asset serialize (edges, params, layout)
3. Tests: transition priority, hard-cut, Travel coexist, Asset round-trip
4. Canvas UI + dual open paths; keep Inspector writable
5. C-ABI / Blunder.Api for params (+ any needed queries)
6. Engineering harness + lean Play + manual checklist
7. Rollback: disable transition eval / hide Canvas; Asset Inspector + Travel-only remain

## Open Questions

- Exact condition binding schema for “drive ref” vs independent tree param — lock at apply (recommend typed ref enum)
- Canvas dock vs modal document tab — lock at apply (recommend dockable editor tab on Asset)
- Whether creating Canvas on an Object with embed-only topology offers “promote to Asset” — recommend yes as apply UX, not a Done blocker
