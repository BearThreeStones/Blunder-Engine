### Task 3: Managed Api + ScriptHost hook



**Files:**

- Create: `engine/managed/Blunder.Api/MessageArg.cs`

- Create: `engine/managed/Blunder.Api/Message.cs`

- Modify: `engine/managed/Blunder.Api/Behaviour.cs`

- Modify: `engine/managed/Blunder.Api/NativeAbi.cs`

- Modify: `engine/managed/Blunder.Api/Native.cs`

- Modify: `engine/managed/Blunder.ScriptHost/HostExports.cs`

- Modify: `engine/managed/Blunder.Api.NativeAbiTests/Program.cs`



**Interfaces:**

- Consumes: NativeAbi message_* 

- Produces:

  - `public enum MessageArgKind : byte { Nil, Bool, Int, Float, ObjectId }`

  - `public struct MessageArg { ... static MessageArg FromInt(long); FromBool; FromFloat; FromObjectId; }`

  - `public readonly struct MessageId { public uint Value; }`

  - `public static class Message { static MessageId Register(string name); static void Send(ulong objectId, MessageId id, params MessageArg[] args); }`

  - `Behaviour.OnMessage(MessageId id, ReadOnlySpan<MessageArg> args)` virtual empty

  - ScriptHost: `OnMessage` unmanaged caller ???`behaviour.OnMessage`; register in `RegisterLifecycleHooks`; clear hook in `ShutdownCleanup`



- [ ] **Step 1: Update NativeAbiTests to expect 23 pointers (fails)**



Change `sizeof(BlunderNativeAbi) == 19 * sizeof(nint)` to `23 * sizeof(nint)`. Add stub function pointers for the four new fields in the complete abi. Build/run NativeAbiTests ???expect size fail until NativeAbi.cs updated.



- [ ] **Step 2: Extend NativeAbi + Native**



Add four `delegate* unmanaged[Cdecl]<...>` fields in the same order as C header. Extend `IsComplete`. Add wrappers `blunder_message_register`, `blunder_message_send`, `blunder_message_set_hook`, `blunder_message_clear_hook`.



- [ ] **Step 3: MessageArg + Message + Behaviour.OnMessage**



`Message.Register`: call native register, throw on Error. `Message.Send`: if args null treat as 0; if Length>4 throw `ArgumentException`; stackalloc/fixed `BlunderMessageArg` buffer; call send; throw on Error.



- [ ] **Step 4: ScriptHost hook**



```csharp

[UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]

static void OnMessage(IntPtr peer, uint id, BlunderMessageArg* args, int argc)

{

    if (peer == IntPtr.Zero) return;

    GCHandle handle = GCHandle.FromIntPtr(peer);

    if (handle.Target is not Behaviour behaviour) return;

    // copy to MessageArg[argc] then behaviour.OnMessage(new MessageId(id), span);

}



// In RegisterLifecycleHooks:

Native.blunder_message_set_hook((IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, uint, BlunderMessageArg*, int, void>)&OnMessage);



// In ShutdownCleanup:

Native.blunder_message_clear_hook();

```



Define a managed `BlunderMessageArg` struct with `[StructLayout(LayoutKind.Sequential)]` matching C (put shared layout in Api).



- [ ] **Step 5: Build managed + NativeAbiTests**



```powershell

dotnet build engine/managed/Blunder.Api/Blunder.Api.csproj -c Debug

dotnet build engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.csproj -c Debug

dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug

```



Expected: exit 0.



- [ ] **Step 6: Commit**



```bash

git add engine/managed/Blunder.Api engine/managed/Blunder.ScriptHost engine/managed/Blunder.Api.NativeAbiTests

git commit -m "$(cat <<'EOF'

feat: add Message fa??ade and Behaviour.OnMessage hook



EOF

)"

```



---
