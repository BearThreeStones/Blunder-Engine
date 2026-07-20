## 1. Docs & vocabulary

- [x] 1.1 Update `CONTENT_LAYOUT.md` for three-tier roles, GUID Asset References, Scene `guid`, Pull/Fast Path, Source Export dual-write, and watch/Reimport
- [x] 1.2 Cross-link ADR 0012 and `CONTEXT.md` Asset pipeline section from `CONTENT_LAYOUT.md` / agent docs as needed

## 2. Asset identity

- [ ] 2.1 Extend descriptor schema with optional archived Source path field; keep Intermediate `source` as Cook/Fast Path input
- [ ] 2.2 Register Scene documents in `AssetRegistry` (scan `*.scene.asset`); allocate/persist `guid` on create/open/upgrade
- [ ] 2.3 Scene serializer: read/write `guid`; mesh field as GUID Asset Reference; migrate legacy path→GUID on load and rewrite on save
- [ ] 2.4 Add GUID-based load/resolve helpers on `AssetManager` / registry (path remains for migration/display)
- [ ] 2.5 Unit tests: registry GUID resolve; scene guid upgrade; path→GUID mesh migration

## 3. Pull Cook & Fast Path

- [ ] 3.1 Formalize freshness API: mark stale, cook one Asset, cook dependents; keep startup `cookIfStale` as optional warm-up only
- [ ] 3.2 Ensure load path: fresh Final → Final; else Fast Path Intermediate + request Cook (mesh and texture)
- [ ] 3.3 Unit/integration tests: missing Final Fast Path; stale meta Fast Path; fresh Final preferred

## 4. Dependency graph & watch

- [ ] 4.1 Build minimal Asset Dependency Graph (Scene→Mesh, Mesh→explicit Texture, Asset→Intermediate leaves)
- [ ] 4.2 Propagate Final invalidation along reverse edges
- [ ] 4.3 Extend file watch to Intermediate Resources (exclude Source for Intermediate invalidation; debounce; suppress self-writes)
- [ ] 4.4 Watch Source root: debounced auto-Reimport for Assets archiving that Source
- [ ] 4.5 Tests: texture change invalidates mesh Final; descriptor change invalidates; Source change triggers Reimport hook

## 5. Import & Source Export

- [ ] 5.1 Keep glTF/image Import as Intermediate register + Resources copy (terminology: not “Source”)
- [ ] 5.2 Implement Assimp whitelist Source Export (FBX/OBJ→glTF), dual-write Source archive + Intermediate, descriptor fields
- [ ] 5.3 Implement Reimport preserving GUID (from archived Source or Intermediate + settings)
- [ ] 5.4 Reject or clearly non-support `.blend` automatic export in v1 (copy-to-Source-only or error)
- [ ] 5.5 Tests: FBX/OBJ Import dual-write; Reimport preserves GUID; glTF Import unchanged success path

## 6. Validation

- [ ] 6.1 Run targeted asset/import/cook/scene tests under the project validation presets
- [ ] 6.2 Manual smoke: Import FBX → preview Fast Path → Cook Final → edit Source → auto-Reimport → scene GUID mesh reference
