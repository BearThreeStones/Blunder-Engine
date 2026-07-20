## 1. Import / Source Export routing

- [x] 1.1 Update Intermediate-direct vs Source Export extension tables: `.dae` + images direct; FBX/OBJ/glTF/GLB → Source Export; keep `.blend` reject
- [x] 1.2 Change Assimp Source Export target from glTF to COLLADA (`.dae`); dual-write Source archive + Intermediate; descriptor `source` / `archived_source`
- [ ] 1.3 Tests: Import `.dae` Intermediate-direct; Import OBJ/glTF dual-write to `.dae`; glTF not registered as Intermediate `source`; `.blend` still rejected

## 2. Fast Path / Cook COLLADA

- [ ] 2.1 Mesh Fast Path and Cook load Intermediate `.dae` via Assimp; gate cgltf (or legacy path) only when `source` is still glTF/GLB
- [ ] 2.2 Update cook freshness / watch path helpers and fixtures that assume Intermediate `.gltf`
- [ ] 2.3 Tests: missing/stale Final Fast Path from `.dae`; fresh Final preferred; Intermediate `.dae` change invalidates Final

## 3. Intermediate Upgrade

- [ ] 3.1 Implement lazy Intermediate Upgrade on project open / registry scan (GUID preserve, archive former glTF, rewrite `source`, invalidate Finals)
- [ ] 3.2 Fail-soft: conversion failure leaves descriptor/`source` unchanged; no partial `.dae` as `source`; legacy load still works
- [ ] 3.3 Tests: successful upgrade glTF→`.dae`; failed upgrade leaves legacy; Reimport from archived Source regenerates `.dae`

## 4. Validation

- [ ] 4.1 Update `asset_pipeline_smoke_test` and related import/cook/watch tests for COLLADA Intermediate
- [ ] 4.2 Run targeted asset/import/cook/scene/smoke suite under project validation presets
- [ ] 4.3 Confirm docs (CONTEXT / CONTENT_LAYOUT / ADR 0013) still match shipped behavior; fix only if code diverged
