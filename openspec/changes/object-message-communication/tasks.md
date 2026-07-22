## 1. Native MessageDispatch + registry

- [x] 1.1 Add `message_dispatch.h/.cpp` with register (name鈫抜d), clear, setHook, send (ObjectDB lookup, BehaviourId snapshot, hook per peer, argc 0..4)
- [x] 1.2 Add `message_dispatch_test` covering register stability, fan-out order, null peer skip, invalid ObjectId no-op, argc>4 failure
- [x] 1.3 Wire sources into `engine/src/runtime/CMakeLists.txt` and test target into `engine/src/tests/CMakeLists.txt`

## 2. C-ABI + NativeAbi v4

- [x] 2.1 Add `BlunderMessageArg` + C-ABI register/send/set_hook/clear_hook; bump `BLUNDER_ENGINE_C_ABI_VERSION` to 4
- [x] 2.2 Fill new fields in `blunder_native_abi_fill_from_process` / `fill_from_module`
- [x] 2.3 Update `native_abi_test` completeness + version 鈮?4

## 3. Managed Api + ScriptHost

- [x] 3.1 Add `MessageArg` / `MessageId` / `Message` fa莽ade and `Behaviour.OnMessage` in Blunder.Api; extend NativeAbi/Native completeness
- [x] 3.2 Register message hook in ScriptHost beside Tick/Ready; clear on ShutdownCleanup
- [x] 3.3 Update NativeAbiTests stub size/completeness for new entries

## 4. Integration verification

- [x] 4.1 Extend editor_dotnet_host_test (or new `object_message_dotnet_test`) : two Objects, AttachBehaviour, Register+Send, assert OnMessage counter
- [x] 4.2 Confirm ADR 0017 + CONTEXT.md Message terms match shipped API names
