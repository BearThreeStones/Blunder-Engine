### Task 4: DotNet integration smoke



**Files:**

- Modify: `engine/src/tests/fixtures/dotnet_host_game/` (add `MessageProbeBehaviour.cs` with static counter)

- Modify: `engine/src/tests/editor_dotnet_host_test.cpp` **or** create `object_message_dotnet_test.cpp` mirroring editor host pattern

- Modify: `engine/managed/Blunder.ScriptHost/HostExports.cs` (optional test seam `GetMessageProbeCount`)

- Modify: CMake test target as needed



**Interfaces:**

- Consumes: Tasks 1???

- Produces: e2e assertion that Send from native C-ABI (or managed via host) increments probe OnMessage count on a second Object



- [ ] **Step 1: Fixture Behaviour**



```csharp

namespace DotnetHostGame;

public class MessageProbeBehaviour : Behaviour

{

    public static int MessageCount;

    public static uint LastId;

    public override void OnMessage(MessageId id, ReadOnlySpan<MessageArg> args)

    {

        ++MessageCount;

        LastId = id.Value;

    }

}

```



Reset counter in test setup (static field = 0 before Attach).



- [ ] **Step 2: Failing e2e test**



Create two ObjectIds via process ABI, AttachBehaviour MessageProbe on target, Register `"Ping"`, Send from test (C-ABI `blunder_message_send` after hooks registered), assert MessageCount==1.



- [ ] **Step 3: Implement seams / fix until green**



```powershell

cmake --build build/vs2026-debug --config Debug --target editor_dotnet_host_test

.\build\vs2026-debug\engine\src\tests\Debug\editor_dotnet_host_test.exe

```



Expected: PASS (or dedicated `object_message_dotnet_test`).



- [ ] **Step 4: Commit**



```bash

git add engine/src/tests engine/managed/Blunder.ScriptHost

git commit -m "$(cat <<'EOF'

test: verify cross-Object Message reaches OnMessage



EOF

)"

```



---
