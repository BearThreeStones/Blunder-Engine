# Task 3.1 Review — Cook freshness API

**Scope:** Read-only review of `5df42cb` vs base `52fb160` (brief/report/diff package)
**Verdict:** Spec ✅ · Quality **Approved**

## Spec checklist

| Requirement | Evidence | Result |
|-------------|----------|--------|
| `markFinalStale(guid)` | Deletes mesh+texture Final bin+meta under `.blunder/cooked/`; empty/uninit no-op | ✅ |
| `cookAsset(guid)` cook one | Registry resolve → `.mesh.yaml` / `.texture.yaml` → private cook helpers; returns true only when Final written | ✅ |
| `cookDependents` stub OK | Documented no-op until OpenSpec 4.x; callable from tests | ✅ |
| `cookIfStale` remains warm-up | Still `cookAll(false)`; header + cpp comments reframe as optional warm-up | ✅ |
| TDD RED→GREEN | RED: C2039 missing members (`task-9-red-excerpt.txt`); GREEN: test build+run exit 0 | ✅ |
| OpenSpec 3.1 marked done | `openspec/changes/asset-pipeline-pull/tasks.md` | ✅ |

## Findings

None Critical / Important.

## Minor

1. **Texture `cookAsset` path untested** — wired via `cookTextureDescriptor`; mesh covers resolve + freshness skip/recook. Acceptable for 3.1; texture cook-one coverage can land with 3.2/3.3.
2. **`markFinalStale` is type-agnostic** — removes both mesh and texture Finals for a GUID. Safe if GUIDs are globally unique; report already notes this.
3. **`removeIfExists` uses `std::filesystem::remove`** after `FileSystem::exists` — consistent with cooked paths being absolute host paths today; no `FileSystem::remove` API. Fine until VFS abstraction grows.

## Notes (non-blocking)

- Brief preferred pure freshness helpers; implementation uses real mesh cook without Vulkan — stronger behavioral coverage, still unit-testable.
- `cookAsset` boolean conflates skip / unknown / failure (documented); callers needing distinction will need a richer result later.
- `cookDependents` is pure no-op (not “cook self”); clearer stub than silent self-cook under a dependents name.
