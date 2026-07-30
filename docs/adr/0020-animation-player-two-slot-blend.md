# AnimationPlayer Phase 2 uses a unified two-slot blend model

DogWalk animation Phase 2 needs Crossfade and continuous idle↔walk weighting without shipping an AnimationTree. We decided the AnimationPlayer exposes **exactly two sample slots**, a single **blendWeight ∈ [0,1]**, optional **Crossfade** as a time-driven weight ramp on those slots, and one **global TimeScale** — not a separate Crossfade pipeline, not N-way graphs, not per-track TimeScale, and not additive layers. Hard cut remains `fade = 0`. This keeps Stepped sync on the dominant slot and avoids inventing a mini state machine in Phase 2.

**Considered options:** mutually exclusive Crossfade vs dual-track modes; Crossfade as a third sample path; BlendSpace1D/2D authorship in Phase 2; dual independent weights. Rejected for API/sync complexity and scope creep into later graph phases.
