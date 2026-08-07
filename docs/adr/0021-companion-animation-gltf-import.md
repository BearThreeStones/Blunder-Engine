# Companion Animation glTF Import (no AnimationLibrary)

**Status:** superseded by [ADR 0028](0028-animation-clip-independent-of-mesh.md) for Mesh↔Clip packaging, Intermediate layout, dependency-graph edges, and Reimport ownership.

Skeletal Import may consume **Companion Animation glTFs** (animations, no meshes; skins allowed — DogWalk Chocomel LOOP exports are skins=1, meshes=0) and register **AnimationClip** Assets by file stem — without inventing a Mesh host and without treating clips as Mesh children. **Multi-select** and **near-disk** discovery remain Import *gestures* for split layouts; they must not persist Mesh↔Clip packaging links. **Godot AnimationLibrary** remains rejected — flat name→GUID on AnimationPlayer stays. Hard-coded `animations/world` tree walks and filename-fuzzy host pairing remain rejected. Durable rules for independence, `Resources/Animations/<stem>/`, migration, and Reimport are in ADR 0028.

**See also:** [ADR 0028](0028-animation-clip-independent-of-mesh.md), [CONTEXT.md — Companion Animation glTF](../../CONTEXT.md), [ADR 0019](0019-gltf-intermediate.md), [ADR 0020](0020-animation-player-two-slot-blend.md).
