## Why

GUI scene editing already has reversible **Editor Commands**, ClassDB, and Play preflight, but nothing a machine can call as one contract: reads are Inspector/Hierarchy, mutations are C++ factories, failures are log strings. Agents (and later CLI/MCP) need Query / Op / Diagnose that share History with the author, address entities by the same names the Scene Asset persists, and return stable Issue codes instead of Console text.

## What Changes

- Introduce the **Authorship contract**: Query (read), Op (mutate), Diagnose (Issue list). CLI/MCP are later adapters, not this slice
- One Op commits exactly one Editor Command (v1: set local transform via existing SetEntityTransform). Query and Diagnose do not push History
- Public **Authorship Address** is scene-unique entity name (Assets stay GUID). EntityId stays Command-internal. ObjectId is not the v1 public target
- Query / Diagnose take an explicit **Authorship Subject**: Live document or On-disk Project. Op always targets the Live document
- Diagnose returns **Issues** (stable code + Issue severity + optional address). Request errors are **Request failure**, not Issues
- **Play rule set** is Diagnose: missing Camera = Error (blocks Play); Scripts dirty / missing output = Warning. **Play Scripts build** still compiles; it is not Diagnose; build failure maps to Error Issues
- **Authorship System** is an editor-only Registered System. Player does not mount it. On-disk Diagnose reuses rule code from tests/tools without the Player
- **Authorship contract v1** catalog only: Query name list + one entity (name, parent name, local TRS); Op transform.set; Diagnose Play rule set
- ADR 0041 records the shape. CONTEXT terms already landed in grilling

**Out of scope:** CLI and MCP adapters; Command type registry / Seam; ObjectId as public address; full Editor Command catalog; Import / Cook / Play-start as Ops; compiling or Cooking inside Diagnose; headless windowless Editor Session as a product host

## Capabilities

### New Capabilities
- `authorship-contract`: Authorship System + Query / Op / Diagnose v1 (address, subject, Issues, Request failure, transform Op, Play rule set)

### Modified Capabilities
- `play-mode`: Play camera gate is Diagnose (Error Issues block spawn). Scripts dirty Diagnose does not compile. Play Scripts build stays a separate step; its failure is Error Issues, not a parallel stringly error

## Impact

- New editor runtime: Authorship System beside Scene Edit / Document History (`RuntimeGlobalContext`, Editor host only)
- Reuse `makeSetEntityTransformCommand`, `SceneInstance::findEntityByName`, `areProjectScriptsDirty`, scene Camera presence checks
- Play start path (`ui_host` / `play_session_controller` / `play_preflight`) consumes Issues instead of only `std::string error`
- Tests: `authorship_contract_test` (Query / Op / Diagnose / Request failure); extend `play_preflight_test` for Issue codes
- Docs: `CONTEXT.md` (already); `docs/adr/0041-authorship-contract.md`
