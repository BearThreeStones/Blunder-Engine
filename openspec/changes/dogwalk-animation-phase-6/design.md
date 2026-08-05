## Context

Phase 5 locked the SkeletonModifier **chain**, a LookAt **sample**, method tracks, BlendSpace2D, and Tree Asset ([ADR 0026](../../../docs/adr/0026-animation-phase-5.md)). DogWalk still needs product mouth / prop-follow / aim feel. Grilling locked **DogWalk animation Phase 6** ([ADR 0027](../../../docs/adr/0027-animation-phase-6.md); CONTEXT Phase 6 / PaperMouth / SkeletonAttachModifier). Stakeholders: runtime animation, scene serialize, Inspector, C-ABI / Blunder.Api, Test Project content. Phase 1–5 content Done gates may remain open.

## Goals / Non-Goals

**Goals:**

- Product ClassDB modifiers: PaperMouth, SkeletonAttachModifier, configurable LookAt
- Scene persistence + Inspector for those three; ordered chain preserved
- Lean C-ABI / Blunder.Api bump for key drives
- Edit scrub + lean Play feel for these modifiers only
- Hybrid: thin engine seam + this-phase content Done

**Non-Goals:**

- Leash / rope constraint system
- Audio tracks; Cubic/Bezier; AnimationLibrary; Cutscene Director
- Tree visual canvas as Done
- Generic C# SkeletonModifier subclass hot-bridge
- Cross-Object Skeleton **animation** drive (Attach child Transform is allowed)
- Closing Phase 1–5 content Done via this phase alone
- Full Pinda parity as the sole Play bar

## Decisions

1. **Hybrid milestone (ADR 0027)**  
   Thin modifier productization + modifier-only content bars.  
   *Alternatives:* content-only; dump all Phase 5 deferrals.

2. **Three product types; leash out**  
   PaperMouth + Attach + LookAt product; leash deferred.  
   *Alternatives:* leash-in; only one sample-level type.

3. **SkeletonAttachModifier = host bone → child Object Transform**  
   Not remote Skeleton drive; glossary-safe.  
   *Alternatives:* same-Object-only; arbitrary Skeleton↔Skeleton bind.

4. **PaperMouth = jaw `openAmount`; optional attach-driven fill**  
   Same scalar; not blendshape / full facial rig.  
   *Alternatives:* Godot-parity mouth; fixed demo offset only.

5. **ClassDB + scene serialize for the three; no C# subclass bridge Done**  
   Extension point remains; hot C# subclass not required.  
   *Alternatives:* Behaviour-only assembly; serialize later.

6. **Lean ABI bump**  
   Key drives only (`openAmount`, LookAt target/bone, Attach child + bone).  
   *Alternatives:* full Inspector mirror; no new ABI.

7. **Content Done = this phase only**  
   Earlier gates stay tracked separately.  
   *Alternatives:* force-close Phase 5 checklist; close all 1–5 bars.

## Risks / Trade-offs

- [Attach vs hierarchy timing] → Define apply after skeleton sample; document child must be hierarchy child (or validated ObjectId) at apply-time
- [PaperMouth bone naming content drift] → Configurable bone name; harness uses fixture jaw bone
- [ABI table growth] → Keep lean; completeness tests only for key drives
- [Phase 1–5 content still open] → Checklist regression section; do not rewrite earlier Done
- [Optional attach→mouth coupling] → Same `openAmount` field; occupancy policy documented; disableable

## Migration Plan

1. Land ADR 0027 + OpenSpec change
2. LookAt product fields + scene serialize
3. PaperMouth + optional attach-driven mode
4. SkeletonAttachModifier + hierarchy validation
5. Inspector + Edit scrub
6. Lean ABI / Blunder.Api + tests
7. Engineering harness + lean Play + manual checklist
8. Rollback: disable product modifiers; Phase 5 chain + sample LookAt remain

## Open Questions

- ~~Exact Attach child identity (ObjectId vs scene path) at serialize time~~ — **locked (task 4.1)**: scene entity name (`childEntity`), resolved back to an ObjectId on instantiate; ObjectId is a session handle and cannot survive a save
- LookAt limit/weight field set — lock at apply (minimum: bone + target)
- Whether attach-driven PaperMouth is default-off — recommend default-off
