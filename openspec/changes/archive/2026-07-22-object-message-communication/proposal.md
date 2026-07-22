## Why



Behaviours on different Objects have no first-class way to notify each other (Hit, OpenDoor, TriggerEntered). Direct method calls and sibling `GetBehaviour` only work on one Object; without a directed Message primitive, gameplay couples through ad-hoc statics or premature Signals. Harmon???s entity messaging (GPG4) maps cleanly onto Blunder???s ObjectId + Behaviour list once scoped away from Lifecycle and property queries.



## What Changes



- Add **Message** / **MessageId** gameplay communication: game-registered names, directed `Send` to one ObjectId, sync fan-out to all Behaviours on that Object

- Payload: small argument bag (**???** Variant-compatible args); unknown MessageIds safely ignored

- Behaviour receive path: virtual **`OnMessage`** (named `OnX` sugar later, not MVP)

- Global managed fa??ade **`Message.Register` / `Message.Send`**; native `MessageDispatch` owns delivery so C++ can Send later on the same path

- Extend **NativeAbi** (ABI version bump) for register / send / message hook

- **Out of scope:** Signal subscribe/emit, query Messages, broadcast/subtree, deferred Post queue as product API, engine-builtin Hit/Damage ids, physics/native Send callers, named OnX codegen



## Capabilities



### New Capabilities

- `object-message`: Directed Message register/send, fan-out semantics, Behaviour `OnMessage`, managed Message fa??ade



### Modified Capabilities

- `csharp-behaviour`: Behaviour base gains `OnMessage`; lifecycle remains Ready/Tick only

- `script-native-abi`: NativeAbi table gains message register/send/hook entries; completeness rules include them

- `engine-c-abi`: New C-ABI symbols + ABI version bump for message entry points



## Impact



- Native: `MessageDispatch` / registry next to `LifecycleDispatch`; ObjectDB lookup on Send; CMake/runtime sources

- C-ABI / NativeAbi: new function pointers; managed `Native.cs` / `NativeAbi.cs` / completeness tests

- Managed: `Blunder.Api` `Message`, `MessageId`, arg types; `Behaviour.OnMessage`; ScriptHost message hook beside Tick/Ready

- Tests: native unit tests + editor/dotnet host smoke for cross-Object Send ???OnMessage

- Docs: ADR 0017; glossary terms Message / MessageId already in `CONTEXT.md`

- Plan: `docs/superpowers/plans/2026-07-22-object-message-communication.md`

