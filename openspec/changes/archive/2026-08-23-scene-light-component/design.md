## Context

See proposal.md for motivation. Camera Component is the template: `CameraComponent` on `SceneInstance`, `"camera"` JSON, Unique Add…, no Object. Viewport shading today is one `BlinnPhongEditorSettings` directional plus an ambient floor (`ForwardMeshUniformData` / `basic.slang` / `pbr.slang`) and one directional shadow map. Mesh Preview / Scene Thumbnail already distinguish Studio lighting vs scene lights in glossary; Scene Thumbnail fallback stays Studio lighting. OverlaySystem already hosts Camera Gizmo.

## Goals / Non-Goals

**Goals:**

- `LightComponent` + serialize/load + Inspector + Add… Unique, parallel to Camera.
- Replace the hidden editor directional/ambient in live views with gathered Light Components (cap 8 per MeshRenderer).
- One Directional shadow map using the grilled Light shadows rule.
- Light Gizmo draw + pick; New Scene starter Directional on its own entity.

**Non-Goals:**

- Clustered/deferred lighting, IBL, LTC.
- Point/Spot/Area shadow maps.
- Viewport linking mode or Light Gizmo property handles.
- Changing Mesh Preview’s Studio lighting.

## Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Storage | `LightComponent` + `SceneInstance` map keyed by EntityId, like cameras | Same Unique ECS-style attachment as Camera |
| JSON | `"light"` object on the entity; linking persisted as **entity names** (resolve to EntityId on load) | EntityId is a session-dense handle; parent / SkeletonAttach already persist names |
| Emit | World-space emit = entity world matrix × local (0,0,-1); shading **L** for Directional/Spot/Area = normalize(-emit) | Matches Camera Gizmo / KHR punctual -Z; current UBO `lightDirection` is the N·L vector |
| GPU | Per-draw array of up to 8 lights (type, pose, color×intensity, range, cone, area size, contribution flags); CPU gather with linking + EntityId order | Spec cap; avoids a global first-8 that ignores linking |
| Ambient | Upload zeros / skip ambient term in live views | Grilled: no Scene environment this slice |
| Shadows | Keep one directional shadow map; bind from the chosen shadow Directional; `shadowParams.y=0` if none | Existing `ShadowMapTarget` |
| Area | Few samples on the rectangle (front hemisphere only), summed into the 8-light budget as one light | Not a disguised Point; no LTC this slice |
| Falloff | Inverse-square × smooth range window on CPU or shader | Grilled |
| Add… | `InspectorUniqueKind::Light`; default `type = Directional` | Grilled |
| New Scene pose | Separate entity above XY; rotate so local -Z slants toward the ground (same spirit as today’s `(0.45, 0.7, 0.55)` key, expressed as orientation) | Grilled; exact Euler is implementation |
| Light Gizmo | Overlay line/AA path like Camera Gizmo; pick before mesh; vs Camera Gizmo: closer wins | Grilled |
| Icon | One Light kind icon (Godot `Light.svg` / `GizmoLight.svg` family), not per-type icons | Unique kind, not four Unique rows |

**Alternatives considered:** four Unique kinds (rejected in grilling); persist linking as EntityId integers (breaks across spawn/delete reorder); keep Blinn-Phong sliders as fallback (rejected: hidden editor light).

## Risks / Trade-offs

- [UBO / descriptor size for 8 lights × PBR + skinned variants] → Keep a packed std140 light array; share gather CPU-side across `basic.slang` / `pbr.slang` / skinned.
- [Area sampling cost] → Low sample count; Area still consumes one of the 8 slots.
- [Stale linking names after rename] → Ignore missing names (spec); Inspector shows them as invalid until removed.
- [Existing scenes go dark] → New Scene gains a Directional; old scenes need the author to Add… Light (no hidden fallback).

## Migration Plan

Existing `.scene.asset` files without `"light"` load with no Light Component (dark live views until the author adds lights). New Scene and Add… Light provide the default Directional. No file-format version bump beyond the new optional `"light"` object. Mesh Preview / Scene Thumbnail Studio paths unchanged except Scene Thumbnail already prefers scene lights when present.

## Open Questions

None — grilled and confirmed 2026-08-20.
