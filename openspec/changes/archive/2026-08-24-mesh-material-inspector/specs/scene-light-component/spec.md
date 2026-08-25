## ADDED Requirements

### Requirement: Live BRDF is MaterialAsset or Mesh shading defaults
Editor viewport, Player, Camera Preview, and Placement Preview SHALL shade mesh surfaces from that mesh’s MaterialAsset when present, otherwise from Mesh shading defaults: white albedo/diffuse, specular 0.4, shininess 32, ambient 0, not unlit. They SHALL NOT apply Editor shading overrides (process-global Blinn-Phong kd/ks/ka/shininess/unlit or SSAO knobs). A present MaterialAsset SHALL supply its own albedo, PBR factors, unlit, stored Blinn-Phong fields, and textures after Mesh material override overlay. Lights SHALL remain Light Components as already required. SSAO SHALL stay off until Scene environment exists.

#### Scenario: No MaterialAsset uses Mesh shading defaults
- **WHEN** a MeshRenderer in the editor viewport has no MaterialAsset
- **THEN** the surface uses white albedo/diffuse, specular 0.4, shininess 32, ambient 0, and is not unlit

#### Scenario: Retired Inspector knobs do not shade the viewport
- **WHEN** Editor shading override values still exist in process memory
- **AND** a lit MeshRenderer is visible in the editor viewport
- **THEN** that MeshRenderer’s BRDF does not use those override values

### Requirement: Mesh Preview and Scene Thumbnail ignore Editor shading overrides
Mesh Preview Render SHALL light with Studio lighting and SHALL shade surfaces from the mesh’s MaterialAsset (with Mesh material override on the Mesh Asset’s one surface) or Mesh shading defaults. Extra primitives SHALL keep Import-built materials. Scene Thumbnail Render SHALL light with Light Components when present, otherwise Studio lighting, and SHALL use the same surface rule. Both paths SHALL leave SSAO off and SHALL NOT read Editor shading overrides.

#### Scenario: Mesh Preview does not read the retired bag
- **WHEN** Mesh Preview Render draws a Mesh Asset
- **THEN** it does not sample Editor shading overrides for lights or BRDF
