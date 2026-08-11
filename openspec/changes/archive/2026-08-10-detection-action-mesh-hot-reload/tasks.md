## 1. Tests first (TDD)

- [x] 1.1 TDD: Intermediate-direct glTF overwrite attributes GUID via descriptor `source` and triggers Detection → Reimport (Auto) refreshing Intermediate-derived data
- [x] 1.2 TDD: Source-archive change uses Detection Action (Prompt path does not silent-Reimport; Auto does)
- [x] 1.3 TDD: Sibling `.bin` / glTF-relative texture sidecar changes coalesce to the same GUID set in one debounce window
- [x] 1.4 TDD: Prompt coalesce — multiple attributed GUIDs yield one pending Detection set (Reimport All applies to all)
- [x] 1.5 TDD: After successful Mesh Reimport, session Mesh presentation reload hook is invoked (AssetManager / preview seam); Clip-only Reimport does not require Mesh hot reload

## 2. Detection Action + watch attribution

- [x] 2.1 Add editor-user Detection Action preference (Prompt default, Auto) and wire into Content Browser / Asset Watch consume path
- [x] 2.2 Extend path→GUID mapping for Intermediate `source` (parallel to `archived_source`); attribute glTF sidecars to parent exchange Asset(s)
- [x] 2.3 Replace silent Source auto-Reimport with Detection Action (Prompt queue vs Auto `requestReimports`)
- [x] 2.4 Intermediate mapped changes: Detection → Reimport; on Prompt Dismiss still invalidate Finals
- [x] 2.5 Keep debounce + `suppressNotificationsFor` for engine self-writes

## 3. Prompt UI

- [x] 3.1 Coalesced toast / notification: Reimport All / Dismiss for pending Detection GUID set
- [x] 3.2 Reimport All calls existing `requestReimports`; Dismiss clears pending without Reimport (invalidate already applied as required)

## 4. Editor Mesh hot reload

- [x] 4.1 After successful Mesh Reimport, reload/rebind Mesh in AssetManager and refresh viewport / Mesh Preview / placed scene meshes for that GUID
- [x] 4.2 Fail soft on reload errors (log; leave prior presentation); do not block Reimport success reporting

## 5. Docs + validation

- [x] 5.1 Confirm ADR 0029 / CONTEXT Detection Action + Editor Asset Hot Reload stay aligned with shipped behavior
- [x] 5.2 Manual smoke: overwrite Intermediate Mesh glTF → Prompt → Reimport All → viewport updates; toggle Auto; overwrite Source archive with Auto
