# Final Review Fix Report

## 2026-07-31 — Companion persistence and batch ambiguity

### Fixes

- Companion `.gltf` persistence now parses buffer and image URIs with cgltf,
  copies non-data relative resources beside the persisted glTF while preserving
  URI-relative paths, rejects escaping/absolute references, and rolls copied
  sidecars back if persistence fails.
- Near-disk companion discovery is gated to a batch containing exactly one glTF
  mesh candidate. Multi-host selection keeps ambiguous companions orphaned
  instead of rediscovering and attaching them to every host.

### TDD evidence

- RED: built `asset_import_test`, then ran
  `.\build\vs2026-debug\engine\src\tests\Debug\asset_import_test.exe`.
  Exit code 1 with 7 expected failures:
  - external-buffer companion clip was not extracted;
  - `LOOP-idle.bin` was not persisted;
  - multi-host orphan was attached to both hosts and produced clip descriptors.
- GREEN: rebuilt and reran the same executable after the implementation.
  Exit code 0 with `asset_import_test: all passed`.

### Regression coverage

- `importExternalBufferCompanionPersistsSidecarAndExtractsClip`
  uses a real external `LOOP-idle.bin`, verifies it is copied under
  `Resources/Models/Chocomel/companions/`, and proves extraction succeeds from
  the persisted companion.
- `multiHostBatchDoesNotRediscoverOrphanCompanions` verifies a two-host batch
  imports only the hosts and creates no orphan-derived animation descriptor.
