using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Blunder;

namespace Blunder.Api.NativeAbiTests;

/// <summary>
/// TDD harness for Native ABI registration (OpenSpec 2.1–2.2).
/// Exit 0 = all checks passed.
/// </summary>
static unsafe class Program
{
    static int s_failures;

    static int Main()
    {
        Expect(
            sizeof(BlunderNativeAbi) == 17 * sizeof(nint),
            "BlunderNativeAbi layout size is 17 pointers");

        Native.ClearRegistrationForTests();

        bool threw = false;
        try
        {
            _ = Native.blunder_engine_abi_version();
        }
        catch (InvalidOperationException ex)
        {
            threw = true;
            Expect(
                ex.Message.Contains("not registered", StringComparison.OrdinalIgnoreCase),
                "exception mentions not registered");
        }
        catch (Exception ex)
        {
            Fail($"before register: expected InvalidOperationException, got {ex.GetType().Name}: {ex.Message}");
        }

        Expect(threw, "call before Register must throw InvalidOperationException (no DllImport fallback)");

        BlunderNativeAbi abi = default;
        abi.engine_abi_version = &StubAbiVersion;
        abi.object_create = &StubObjectCreate;
        abi.object_destroy = &StubObjectDestroy;
        abi.object_is_valid = &StubObjectIsValid;
        abi.object_set_bool_property = &StubSetBool;
        abi.object_get_bool_property = &StubGetBool;
        abi.object_add_behaviour = &StubAddBehaviour;
        abi.object_remove_behaviour = &StubRemoveBehaviour;
        abi.object_behaviour_count = &StubBehaviourCount;
        abi.object_behaviour_id_at = &StubBehaviourIdAt;
        abi.object_set_behaviour_peer = &StubSetPeer;
        abi.object_get_behaviour_peer = &StubGetPeer;
        abi.object_set_vec3_property = &StubSetVec3;
        abi.object_get_vec3_property = &StubGetVec3;
        abi.lifecycle_set_tick_hook = &StubSetTickHook;
        abi.lifecycle_set_ready_hook = &StubSetReadyHook;
        abi.lifecycle_clear_hooks = &StubClearHooks;

        Native.Register(in abi);

        Expect(Native.blunder_engine_abi_version() == 42, "version after register");
        Expect(Native.blunder_object_create() == 7UL, "create after register");
        Expect(Native.blunder_object_is_valid(7) == 1, "is_valid after register");
        Expect(
            Native.blunder_object_set_bool_property(1, "Object", "flag", 1) == Native.Ok,
            "set_bool after register");

        BlunderNativeAbi incomplete = default;
        incomplete.engine_abi_version = &StubAbiVersion;
        bool rejected = false;
        try
        {
            Native.Register(in incomplete);
        }
        catch (ArgumentException)
        {
            rejected = true;
        }

        Expect(rejected, "Register must reject incomplete (null) tables");

        if (s_failures == 0)
        {
            Console.WriteLine("Blunder.Api.NativeAbiTests: OK");
            return 0;
        }

        Console.Error.WriteLine($"Blunder.Api.NativeAbiTests: {s_failures} failure(s)");
        return 1;
    }

    static void Expect(bool condition, string label)
    {
        if (condition)
        {
            Console.WriteLine($"OK: {label}");
            return;
        }

        Fail(label);
    }

    static void Fail(string label)
    {
        Console.Error.WriteLine($"FAIL: {label}");
        ++s_failures;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAbiVersion() => 42;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static ulong StubObjectCreate() => 7;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubObjectDestroy(ulong id) => id == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubObjectIsValid(ulong id) => id == 0 ? 0 : 1;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSetBool(ulong id, byte* className, byte* propertyName, int value) =>
        id == 0 || className == null || propertyName == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubGetBool(ulong id, byte* className, byte* propertyName, int* outValue)
    {
        if (outValue == null || id == 0 || className == null || propertyName == null)
        {
            return Native.Error;
        }

        *outValue = 1;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static ulong StubAddBehaviour(ulong id, byte* typeName) =>
        id == 0 || typeName == null ? 0UL : 99UL;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubRemoveBehaviour(ulong id, ulong behaviourId) =>
        id == 0 || behaviourId == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubBehaviourCount(ulong id) => id == 0 ? 0 : 1;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static ulong StubBehaviourIdAt(ulong id, int index) =>
        id == 0 || index < 0 ? 0UL : 99UL;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSetPeer(ulong id, ulong behaviourId, void* peer) =>
        id == 0 || behaviourId == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static void* StubGetPeer(ulong id, ulong behaviourId) =>
        id == 0 || behaviourId == 0 ? null : (void*)0x1;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSetVec3(
        ulong id, byte* className, byte* propertyName, float x, float y, float z) =>
        id == 0 || className == null || propertyName == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubGetVec3(
        ulong id, byte* className, byte* propertyName, float* x, float* y, float* z)
    {
        if (outMissing(id, className, propertyName, x, y, z))
        {
            return Native.Error;
        }

        *x = 1;
        *y = 2;
        *z = 3;
        return Native.Ok;
    }

    static bool outMissing(
        ulong id, byte* className, byte* propertyName, float* x, float* y, float* z) =>
        id == 0 || className == null || propertyName == null || x == null || y == null ||
        z == null;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSetTickHook(byte* className, void* hook) =>
        className == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSetReadyHook(byte* className, void* hook) =>
        className == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubClearHooks() => Native.Ok;
}
