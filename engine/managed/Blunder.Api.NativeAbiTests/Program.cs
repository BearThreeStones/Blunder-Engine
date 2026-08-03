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
    static float s_lastFadeSeconds;
    static int s_lastSlotIndex;
    static string s_lastSlotClip = "";
    static string s_slot0Clip = "";
    static string s_slot1Clip = "";
    static float s_blendWeight = 0.5f;
    static float s_timeScale = 1.0f;

    static ulong s_syncGroupId = 100;
    static int s_lastSuppressFlag = -1;
    static int s_inCine;
    static int s_inputSuppressed;
    static ulong s_lastJoinGroupId;
    static ulong s_lastJoinPlayerId;
    static ulong s_lastLeaveGroupId;
    static ulong s_lastLeavePlayerId;
    static int s_lastFireCount;
    static string s_lastFireSameNameClip = "";
    static float s_lastFireSameNameSeek = -1f;

    static int Main()
    {
        Expect(
            sizeof(BlunderNativeAbi) == 48 * sizeof(nint),
            "BlunderNativeAbi layout size is 48 pointers");

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
        abi.gameplay_input_get_move = &StubGetMove;
        abi.gameplay_input_was_jump_pressed = &StubWasJump;
        abi.message_register = &StubMessageRegister;
        abi.message_send = &StubMessageSend;
        abi.message_set_hook = &StubMessageSetHook;
        abi.message_clear_hook = &StubMessageClearHook;
        abi.animation_player_play = &StubAnimationPlay;
        abi.animation_player_play_with_fade = &StubAnimationPlayWithFade;
        abi.animation_player_stop = &StubAnimationStop;
        abi.animation_player_set_slot = &StubAnimationSetSlot;
        abi.animation_player_get_slot = &StubAnimationGetSlot;
        abi.animation_player_set_blend_weight = &StubAnimationSetBlendWeight;
        abi.animation_player_get_blend_weight = &StubAnimationGetBlendWeight;
        abi.animation_player_set_time_scale = &StubAnimationSetTimeScale;
        abi.animation_player_get_time_scale = &StubAnimationGetTimeScale;
        abi.animation_player_set_loop = &StubAnimationSetLoop;
        abi.animation_player_get_playback_position = &StubAnimationGetPosition;
        abi.animation_player_get_clip_length = &StubAnimationGetLength;
        abi.animation_player_add_pose_applied_listener = &StubAnimationAddPoseListener;
        abi.animation_player_clear_pose_applied_listeners = &StubAnimationClearPoseListeners;
        abi.sync_group_create = &StubSyncGroupCreate;
        abi.sync_group_destroy = &StubSyncGroupDestroy;
        abi.sync_group_join = &StubSyncGroupJoin;
        abi.sync_group_leave = &StubSyncGroupLeave;
        abi.sync_group_fire = &StubSyncGroupFire;
        abi.sync_group_fire_same_name = &StubSyncGroupFireSameName;
        abi.sync_group_fire_same_name_seek = &StubSyncGroupFireSameNameSeek;
        abi.cine_enter = &StubCineEnter;
        abi.cine_end = &StubCineEnd;
        abi.cine_is_in_cine = &StubCineIsInCine;
        abi.cine_is_gameplay_input_suppressed = &StubCineIsGameplayInputSuppressed;

        Native.Register(in abi);

        Expect(Native.blunder_engine_abi_version() == 42, "version after register");
        Expect(Native.blunder_object_create() == 7UL, "create after register");
        Expect(Native.blunder_object_is_valid(7) == 1, "is_valid after register");
        Expect(
            Native.blunder_object_set_bool_property(1, "Object", "flag", 1) == Native.Ok,
            "set_bool after register");

        Vec2 move = Input.GetMove();
        Expect(move.X == 0.3f && move.Y == -0.4f, "Input.GetMove via stub");
        Expect(Input.WasJumpPressed(), "Input.WasJumpPressed via stub");

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

        RunAnimationPlayerSmokeTests();
        RunSyncGroupAndCineSmokeTests();

        if (s_failures == 0)
        {
            Console.WriteLine("Blunder.Api.NativeAbiTests: OK");
            return 0;
        }

        Console.Error.WriteLine($"Blunder.Api.NativeAbiTests: {s_failures} failure(s)");
        return 1;
    }

    static void RunAnimationPlayerSmokeTests()
    {
        s_lastFadeSeconds = 0f;
        s_lastSlotIndex = -1;
        s_lastSlotClip = "";
        s_slot0Clip = "idle";
        s_slot1Clip = "";
        s_blendWeight = 0.25f;
        s_timeScale = 1.5f;

        ObjectHandle handle = ObjectHandle.GetOrCreate(7);
        AnimationPlayer player = handle.EnsureAnimationPlayer();

        Expect(player.Play("walk"), "AnimationPlayer.Play");
        Expect(player.Play("run", 0.35f), "AnimationPlayer.Play with fade");
        Expect(Math.Abs(s_lastFadeSeconds - 0.35f) < 0.0001f, "Play fade forwarded to native");

        Expect(player.SetSlot(0, "idle"), "AnimationPlayer.SetSlot");
        Expect(s_lastSlotIndex == 0 && s_lastSlotClip == "idle", "SetSlot forwarded to native");
        Expect(player.GetSlot(0) == "idle", "AnimationPlayer.GetSlot");

        player.BlendWeight = 0.75f;
        Expect(Math.Abs(s_blendWeight - 0.75f) < 0.0001f, "BlendWeight set");
        Expect(Math.Abs(player.BlendWeight - 0.75f) < 0.0001f, "BlendWeight get");

        player.TimeScale = 2.0f;
        Expect(Math.Abs(s_timeScale - 2.0f) < 0.0001f, "TimeScale set");
        Expect(Math.Abs(player.TimeScale - 2.0f) < 0.0001f, "TimeScale get");

        Expect(
            Native.blunder_animation_player_set_slot(7, 1, "walk") == Native.Ok,
            "Native set_slot after register");
        Expect(
            Native.blunder_animation_player_get_slot(7, 1, out string slotClip) == Native.Ok &&
            slotClip == "walk",
            "Native get_slot after register");
        Expect(
            Native.blunder_animation_player_set_blend_weight(7, 0.1f) == Native.Ok,
            "Native set_blend_weight after register");
        Expect(
            Native.blunder_animation_player_get_blend_weight(7, out float weight) == Native.Ok &&
            Math.Abs(weight - 0.1f) < 0.0001f,
            "Native get_blend_weight after register");
        Expect(
            Native.blunder_animation_player_set_time_scale(7, 0.5f) == Native.Ok,
            "Native set_time_scale after register");
        Expect(
            Native.blunder_animation_player_get_time_scale(7, out float scale) == Native.Ok &&
            Math.Abs(scale - 0.5f) < 0.0001f,
            "Native get_time_scale after register");
        Expect(
            Native.blunder_animation_player_play_with_fade(7, "trot", 1.25f) == Native.Ok,
            "Native play_with_fade after register");
    }

    static void RunSyncGroupAndCineSmokeTests()
    {
        s_syncGroupId = 100;
        s_lastSuppressFlag = -1;
        s_inCine = 0;
        s_inputSuppressed = 0;
        s_lastFireCount = 0;
        s_lastFireSameNameClip = "";
        s_lastFireSameNameSeek = -1f;

        AnimationSyncGroup group = AnimationSyncGroup.Create();
        Expect(group.GroupId == 101UL, "AnimationSyncGroup.Create");

        ObjectHandle player = ObjectHandle.GetOrCreate(7);
        Expect(group.Join(player), "AnimationSyncGroup.Join");
        Expect(s_lastJoinGroupId == 101UL && s_lastJoinPlayerId == 7UL, "Join forwarded");

        SyncGroupFireInstruction[] instructions =
        [
            new SyncGroupFireInstruction
            {
                Player = player,
                ClipName = "walk",
                SeekSeconds = 0.25f,
            },
        ];
        Expect(group.Fire(instructions), "AnimationSyncGroup.Fire");
        Expect(s_lastFireCount == 1, "Fire forwarded instruction count");
        Expect(
            !group.Fire(ReadOnlySpan<SyncGroupFireInstruction>.Empty),
            "AnimationSyncGroup.Fire empty");

        Expect(group.FireSameName("idle"), "AnimationSyncGroup.FireSameName");
        Expect(s_lastFireSameNameClip == "idle", "FireSameName clip forwarded");
        Expect(group.FireSameName("run", 0.5f), "AnimationSyncGroup.FireSameName seek");
        Expect(
            s_lastFireSameNameClip == "run" &&
            Math.Abs(s_lastFireSameNameSeek - 0.5f) < 0.0001f,
            "FireSameName seek forwarded");

        Expect(group.Leave(player), "AnimationSyncGroup.Leave");
        Expect(s_lastLeaveGroupId == 101UL && s_lastLeavePlayerId == 7UL, "Leave forwarded");
        Expect(group.Destroy(), "AnimationSyncGroup.Destroy");

        Expect(CineSegment.Enter(), "CineSegment.Enter");
        Expect(s_lastSuppressFlag == 0, "Enter default suppress flag");
        Expect(CineSegment.IsInCine, "CineSegment.IsInCine");
        Expect(!CineSegment.IsGameplayInputSuppressed, "CineSegment not suppressed");
        Expect(CineSegment.Enter(suppressGameplayInput: true), "CineSegment.Enter suppress");
        Expect(s_lastSuppressFlag == 1, "Enter suppress forwarded");
        Expect(CineSegment.IsGameplayInputSuppressed, "CineSegment suppressed");
        Expect(CineSegment.End(), "CineSegment.End");
        Expect(!CineSegment.IsInCine, "CineSegment not in cine after End");

        Expect(
            Native.blunder_sync_group_create() == 102UL,
            "Native sync_group_create after register");
        Expect(
            Native.blunder_cine_enter(1) == Native.Ok,
            "Native cine_enter after register");
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

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubGetMove(float* x, float* y)
    {
        *x = 0.3f;
        *y = -0.4f;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubWasJump(int* pressed)
    {
        *pressed = 1;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubMessageRegister(byte* name, uint* outId)
    {
        if (name == null || outId == null)
        {
            return Native.Error;
        }

        *outId = 42;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubMessageSend(ulong target, uint id, BlunderMessageArg* args, int argc) =>
        target == 0 || id == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubMessageSetHook(
        delegate* unmanaged[Cdecl]<void*, uint, BlunderMessageArg*, int, void> hook) =>
        hook == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubMessageClearHook() => Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationPlay(ulong id, byte* clipName) =>
        id == 0 || clipName == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationPlayWithFade(ulong id, byte* clipName, float fadeSeconds)
    {
        if (id == 0 || clipName == null)
        {
            return Native.Error;
        }

        s_lastFadeSeconds = fadeSeconds;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationStop(ulong id) => id == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationSetSlot(ulong id, int slotIndex, byte* clipName)
    {
        if (id == 0 || clipName == null || slotIndex < 0 || slotIndex > 1)
        {
            return Native.Error;
        }

        s_lastSlotIndex = slotIndex;
        s_lastSlotClip = Utf8ToString(clipName);
        if (slotIndex == 0)
        {
            s_slot0Clip = s_lastSlotClip;
        }
        else
        {
            s_slot1Clip = s_lastSlotClip;
        }

        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetSlot(ulong id, int slotIndex, byte* outName, int nameCapacity)
    {
        if (id == 0 || outName == null || nameCapacity <= 0 || slotIndex < 0 || slotIndex > 1)
        {
            return Native.Error;
        }

        string clip = slotIndex == 0 ? s_slot0Clip : s_slot1Clip;
        WriteUtf8(clip, outName, nameCapacity);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationSetBlendWeight(ulong id, float weight)
    {
        if (id == 0)
        {
            return Native.Error;
        }

        s_blendWeight = weight;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetBlendWeight(ulong id, float* outWeight)
    {
        if (id == 0 || outWeight == null)
        {
            return Native.Error;
        }

        *outWeight = s_blendWeight;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationSetTimeScale(ulong id, float scale)
    {
        if (id == 0)
        {
            return Native.Error;
        }

        s_timeScale = scale;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetTimeScale(ulong id, float* outScale)
    {
        if (id == 0 || outScale == null)
        {
            return Native.Error;
        }

        *outScale = s_timeScale;
        return Native.Ok;
    }

    static string Utf8ToString(byte* utf8)
    {
        if (utf8 == null)
        {
            return "";
        }

        int length = 0;
        while (utf8[length] != 0)
        {
            ++length;
        }

        return length == 0 ? "" : System.Text.Encoding.UTF8.GetString(utf8, length);
    }

    static void WriteUtf8(string value, byte* outBuffer, int capacity)
    {
        outBuffer[0] = 0;
        if (capacity <= 0)
        {
            return;
        }

        byte[] bytes = System.Text.Encoding.UTF8.GetBytes(value);
        int copyLen = bytes.Length < capacity - 1 ? bytes.Length : capacity - 1;
        for (int i = 0; i < copyLen; ++i)
        {
            outBuffer[i] = bytes[i];
        }

        outBuffer[copyLen] = 0;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationSetLoop(ulong id, int loop) =>
        id == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetPosition(ulong id, float* position)
    {
        if (id == 0 || position == null)
        {
            return Native.Error;
        }

        *position = 0.5f;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetLength(ulong id, float* length)
    {
        if (id == 0 || length == null)
        {
            return Native.Error;
        }

        *length = 2.0f;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationAddPoseListener(
        ulong id,
        delegate* unmanaged[Cdecl]<ulong, void*, void> hook,
        void* userdata) =>
        id == 0 || hook == null ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationClearPoseListeners(ulong id) =>
        id == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static ulong StubSyncGroupCreate()
    {
        ++s_syncGroupId;
        return s_syncGroupId;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSyncGroupDestroy(ulong id) => id == 0 ? Native.Error : Native.Ok;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSyncGroupJoin(ulong groupId, ulong playerObjectId)
    {
        if (groupId == 0 || playerObjectId == 0)
        {
            return Native.Error;
        }

        s_lastJoinGroupId = groupId;
        s_lastJoinPlayerId = playerObjectId;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSyncGroupLeave(ulong groupId, ulong playerObjectId)
    {
        if (groupId == 0 || playerObjectId == 0)
        {
            return Native.Error;
        }

        s_lastLeaveGroupId = groupId;
        s_lastLeavePlayerId = playerObjectId;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSyncGroupFire(
        ulong groupId, BlunderSyncGroupFireInstruction* instructions, int instructionCount)
    {
        if (groupId == 0 || instructionCount < 0 ||
            (instructionCount > 0 && instructions == null))
        {
            return Native.Error;
        }

        s_lastFireCount = instructionCount;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSyncGroupFireSameName(ulong groupId, byte* clipName)
    {
        if (groupId == 0 || clipName == null)
        {
            return Native.Error;
        }

        s_lastFireSameNameClip = Utf8ToString(clipName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSyncGroupFireSameNameSeek(ulong groupId, byte* clipName, float seekSeconds)
    {
        if (groupId == 0 || clipName == null)
        {
            return Native.Error;
        }

        s_lastFireSameNameClip = Utf8ToString(clipName);
        s_lastFireSameNameSeek = seekSeconds;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubCineEnter(int suppressGameplayInput)
    {
        s_lastSuppressFlag = suppressGameplayInput;
        s_inCine = 1;
        s_inputSuppressed = suppressGameplayInput;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubCineEnd()
    {
        if (s_inCine == 0)
        {
            return Native.Error;
        }

        s_inCine = 0;
        s_inputSuppressed = 0;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubCineIsInCine(int* outValue)
    {
        if (outValue == null)
        {
            return Native.Error;
        }

        *outValue = s_inCine;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubCineIsGameplayInputSuppressed(int* outValue)
    {
        if (outValue == null)
        {
            return Native.Error;
        }

        *outValue = s_inputSuppressed;
        return Native.Ok;
    }
}
