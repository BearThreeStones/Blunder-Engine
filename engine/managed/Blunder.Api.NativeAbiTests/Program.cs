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
    static int s_treeActive;
    static string s_lastTravelState = "";
    static string s_lastStartState = "";
    static string s_lastBlendNode = "";
    static float s_blendSpaceScalar;
    static float s_blendSpace2DX;
    static float s_blendSpace2DY;
    static string s_lastBlend2DNode = "";
    static string s_treeAssetGuid = "";
    static string s_lastOneShotClip = "";
    static float s_add2Weight;
    static int s_modifierCount;
    static bool[] s_modifierEnabled = [true, true];
    static string s_methodKeyName = "FootStep";
    static float s_methodKeyTime = 0.25f;

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

    static float s_paperMouthOpenAmount;
    static string s_paperMouthBoneName = "Jaw";
    static string s_attachBoneName = "Hand";
    static ulong s_attachChildObjectId;
    static string s_lookAtBoneName = "Head";
    static float s_lookAtTargetX;
    static float s_lookAtTargetY;
    static float s_lookAtTargetZ = 1.0f;

    static int s_lastLogSeverity = -1;
    static string s_lastLogText = "";
    static string s_lastLogStack = "";

    static int Main()
    {
        Expect(
            sizeof(BlunderNativeAbi) == 84 * sizeof(nint),
            "BlunderNativeAbi layout size is 84 pointers");

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
        abi.animation_tree_set_active = &StubAnimationTreeSetActive;
        abi.animation_tree_get_active = &StubAnimationTreeGetActive;
        abi.animation_tree_travel = &StubAnimationTreeTravel;
        abi.animation_tree_start = &StubAnimationTreeStart;
        abi.animation_tree_set_blend_space_scalar = &StubAnimationTreeSetBlendSpaceScalar;
        abi.animation_tree_get_blend_space_scalar = &StubAnimationTreeGetBlendSpaceScalar;
        abi.animation_tree_request_one_shot = &StubAnimationTreeRequestOneShot;
        abi.animation_tree_set_add2_weight = &StubAnimationTreeSetAdd2Weight;
        abi.animation_tree_get_add2_weight = &StubAnimationTreeGetAdd2Weight;
        abi.animation_tree_set_blend_space_2d_param = &StubAnimationTreeSetBlendSpace2DParam;
        abi.animation_tree_get_blend_space_2d_param = &StubAnimationTreeGetBlendSpace2DParam;
        abi.animation_tree_set_asset_guid = &StubAnimationTreeSetAssetGuid;
        abi.animation_tree_get_asset_guid = &StubAnimationTreeGetAssetGuid;
        abi.animation_tree_set_tree_param_bool = &StubAnimationTreeSetTreeParamBool;
        abi.animation_tree_get_tree_param_bool = &StubAnimationTreeGetTreeParamBool;
        abi.animation_tree_set_tree_param_float = &StubAnimationTreeSetTreeParamFloat;
        abi.animation_tree_get_tree_param_float = &StubAnimationTreeGetTreeParamFloat;
        abi.skeleton_modifier_count = &StubSkeletonModifierCount;
        abi.skeleton_modifier_set_enabled = &StubSkeletonModifierSetEnabled;
        abi.skeleton_modifier_get_enabled = &StubSkeletonModifierGetEnabled;
        abi.skeleton_modifier_move = &StubSkeletonModifierMove;
        abi.skeleton_modifier_set_paper_mouth_open_amount =
            &StubSkeletonModifierSetPaperMouthOpenAmount;
        abi.skeleton_modifier_get_paper_mouth_open_amount =
            &StubSkeletonModifierGetPaperMouthOpenAmount;
        abi.skeleton_modifier_set_paper_mouth_bone_name =
            &StubSkeletonModifierSetPaperMouthBoneName;
        abi.skeleton_modifier_get_paper_mouth_bone_name =
            &StubSkeletonModifierGetPaperMouthBoneName;
        abi.skeleton_modifier_set_attach_bone_name = &StubSkeletonModifierSetAttachBoneName;
        abi.skeleton_modifier_get_attach_bone_name = &StubSkeletonModifierGetAttachBoneName;
        abi.skeleton_modifier_set_attach_child_object_id =
            &StubSkeletonModifierSetAttachChildObjectId;
        abi.skeleton_modifier_get_attach_child_object_id =
            &StubSkeletonModifierGetAttachChildObjectId;
        abi.skeleton_modifier_set_look_at_target = &StubSkeletonModifierSetLookAtTarget;
        abi.skeleton_modifier_get_look_at_target = &StubSkeletonModifierGetLookAtTarget;
        abi.skeleton_modifier_set_look_at_bone_name = &StubSkeletonModifierSetLookAtBoneName;
        abi.skeleton_modifier_get_look_at_bone_name = &StubSkeletonModifierGetLookAtBoneName;
        abi.animation_player_get_method_key_count = &StubAnimationGetMethodKeyCount;
        abi.animation_player_get_method_key = &StubAnimationGetMethodKey;
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
        abi.log = &StubLog;

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

        RunDebugLogSmokeTests();

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
        RunAnimationTreeSmokeTests();
        RunSkeletonModifierProductFacadeSmokeTests();
        RunSyncGroupAndCineSmokeTests();
        RunSyncFireOneShotSmokeTests();

        if (s_failures == 0)
        {
            Console.WriteLine("Blunder.Api.NativeAbiTests: OK");
            return 0;
        }

        Console.Error.WriteLine($"Blunder.Api.NativeAbiTests: {s_failures} failure(s)");
        return 1;
    }

    static void RunDebugLogSmokeTests()
    {
        s_lastLogSeverity = -1;
        s_lastLogText = "";
        s_lastLogStack = "";
        Blunder.Debug.Log("hello-console");
        Expect(s_lastLogSeverity == Blunder.Debug.SeverityLog, "Debug.Log severity");
        Expect(s_lastLogText == "hello-console", "Debug.Log text");
        Expect(!string.IsNullOrEmpty(s_lastLogStack), "Debug.Log captured stack");
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

        Expect(player.GetMethodKeyCount("walk") == 1, "AnimationPlayer.GetMethodKeyCount");
        Expect(
            player.TryGetMethodKey("walk", 0, out string methodName, out float methodTime) &&
            methodName == "FootStep" &&
            Math.Abs(methodTime - 0.25f) < 0.0001f,
            "AnimationPlayer.TryGetMethodKey");
    }

    static void RunAnimationTreeSmokeTests()
    {
        s_treeActive = 0;
        s_lastTravelState = "";
        s_lastStartState = "";
        s_lastBlendNode = "";
        s_blendSpaceScalar = 0.5f;
        s_lastBlend2DNode = "";
        s_blendSpace2DX = 0.1f;
        s_blendSpace2DY = 0.2f;
        s_treeAssetGuid = "";
        s_lastOneShotClip = "";
        s_add2Weight = 0.25f;
        s_modifierCount = 2;
        s_modifierEnabled = [true, true];

        ObjectHandle handle = ObjectHandle.GetOrCreate(7);
        AnimationTree tree = handle.EnsureAnimationTree();

        tree.Active = true;
        Expect(s_treeActive == 1, "AnimationTree.Active set");
        Expect(tree.Active, "AnimationTree.Active get");

        Expect(tree.Travel("Locomotion"), "AnimationTree.Travel");
        Expect(s_lastTravelState == "Locomotion", "Travel forwarded to native");
        Expect(tree.Start("Locomotion"), "AnimationTree.Start");
        Expect(s_lastStartState == "Locomotion", "Start forwarded to native");

        tree.SetBlendSpaceScalar("Locomotion", 0.75f);
        Expect(s_lastBlendNode == "Locomotion" &&
               Math.Abs(s_blendSpaceScalar - 0.75f) < 0.0001f,
            "SetBlendSpaceScalar forwarded");
        Expect(Math.Abs(tree.GetBlendSpaceScalar("Locomotion") - 0.75f) < 0.0001f,
            "GetBlendSpaceScalar forwarded");

        tree.SetBlendSpace2DParam("Locomotion2D", 0.4f, 0.6f);
        Expect(s_lastBlend2DNode == "Locomotion2D" &&
               Math.Abs(s_blendSpace2DX - 0.4f) < 0.0001f &&
               Math.Abs(s_blendSpace2DY - 0.6f) < 0.0001f,
            "SetBlendSpace2DParam forwarded");
        (float bx, float by) = tree.GetBlendSpace2DParam("Locomotion2D");
        Expect(Math.Abs(bx - 0.4f) < 0.0001f && Math.Abs(by - 0.6f) < 0.0001f,
            "GetBlendSpace2DParam forwarded");

        tree.AssetGuid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
        Expect(s_treeAssetGuid == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
            "AssetGuid set forwarded");
        Expect(tree.AssetGuid == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
            "AssetGuid get forwarded");

        Expect(tree.RequestOneShot("trip"), "AnimationTree.RequestOneShot");
        Expect(s_lastOneShotClip == "trip", "RequestOneShot forwarded");

        tree.Add2Weight = 0.9f;
        Expect(Math.Abs(s_add2Weight - 0.9f) < 0.0001f, "Add2Weight set");
        Expect(Math.Abs(tree.Add2Weight - 0.9f) < 0.0001f, "Add2Weight get");

        Expect(handle.SkeletonModifierCount == 2, "ObjectHandle.SkeletonModifierCount");
        Expect(handle.SetSkeletonModifierEnabled(1, false), "SetSkeletonModifierEnabled");
        Expect(!handle.IsSkeletonModifierEnabled(1), "IsSkeletonModifierEnabled false");
        Expect(handle.MoveSkeletonModifier(0, 1), "MoveSkeletonModifier");
        Expect(handle.IsSkeletonModifierEnabled(0) == false, "Move preserves enabled flags");
        Expect(handle.IsSkeletonModifierEnabled(1), "Move swaps enabled flags");

        s_paperMouthOpenAmount = 0.0f;
        Expect(
            Native.blunder_skeleton_modifier_set_paper_mouth_open_amount(7, 0, 0.8f) ==
            Native.Ok,
            "Native set paper mouth open amount after register");
        Expect(
            Native.blunder_skeleton_modifier_get_paper_mouth_open_amount(
                7, 0, out float openAmount) == Native.Ok &&
            Math.Abs(openAmount - 0.8f) < 0.0001f,
            "Native get paper mouth open amount after register");
        Expect(
            Native.blunder_skeleton_modifier_set_attach_child_object_id(7, 1, 42UL) ==
            Native.Ok,
            "Native set attach child object id after register");
        Expect(
            Native.blunder_skeleton_modifier_get_attach_child_object_id(
                7, 1, out ulong childId) == Native.Ok &&
            childId == 42UL,
            "Native get attach child object id after register");
        Expect(
            Native.blunder_skeleton_modifier_set_look_at_target(7, 2, 1.0f, 2.0f, 3.0f) ==
            Native.Ok,
            "Native set look at target after register");

        Expect(
            Native.blunder_animation_tree_set_active(7, 0) == Native.Ok,
            "Native animation_tree_set_active after register");
        Expect(
            Native.blunder_animation_tree_get_active(7, out int active) == Native.Ok &&
            active == 0,
            "Native animation_tree_get_active after register");
    }

    static void RunSkeletonModifierProductFacadeSmokeTests()
    {
        s_paperMouthOpenAmount = 0.0f;
        s_paperMouthBoneName = "Jaw";
        s_attachBoneName = "Hand";
        s_attachChildObjectId = 0;
        s_lookAtBoneName = "Head";
        s_lookAtTargetX = 0.0f;
        s_lookAtTargetY = 0.0f;
        s_lookAtTargetZ = 1.0f;

        ObjectHandle host = ObjectHandle.GetOrCreate(7);
        ObjectHandle child = ObjectHandle.GetOrCreate(42);

        PaperMouth paperMouth = host.PaperMouthAt(0);
        Expect(paperMouth.Index == 0, "PaperMouth.Index");
        paperMouth.OpenAmount = 0.8f;
        Expect(
            Math.Abs(s_paperMouthOpenAmount - 0.8f) < 0.0001f,
            "PaperMouth.OpenAmount set forwarded");
        Expect(
            Math.Abs(paperMouth.OpenAmount - 0.8f) < 0.0001f,
            "PaperMouth.OpenAmount get forwarded");
        paperMouth.BoneName = "LowerJaw";
        Expect(s_paperMouthBoneName == "LowerJaw", "PaperMouth.BoneName set forwarded");
        Expect(paperMouth.BoneName == "LowerJaw", "PaperMouth.BoneName get forwarded");

        SkeletonAttachModifier attach = host.AttachAt(1);
        Expect(attach.Index == 1, "SkeletonAttachModifier.Index");
        attach.BoneName = "PropSocket";
        Expect(s_attachBoneName == "PropSocket", "SkeletonAttachModifier.BoneName set forwarded");
        Expect(attach.BoneName == "PropSocket", "SkeletonAttachModifier.BoneName get forwarded");
        attach.Child = child;
        Expect(s_attachChildObjectId == 42UL, "SkeletonAttachModifier.Child set forwarded");
        Expect(attach.Child != null && attach.Child.Id == 42UL,
            "SkeletonAttachModifier.Child get forwarded");

        LookAt lookAt = host.LookAtAt(2);
        Expect(lookAt.Index == 2, "LookAt.Index");
        lookAt.Target = new Vec3(1.0f, 2.0f, 3.0f);
        Expect(
            Math.Abs(s_lookAtTargetX - 1.0f) < 0.0001f &&
            Math.Abs(s_lookAtTargetY - 2.0f) < 0.0001f &&
            Math.Abs(s_lookAtTargetZ - 3.0f) < 0.0001f,
            "LookAt.Target set forwarded");
        Expect(lookAt.Target == new Vec3(1.0f, 2.0f, 3.0f), "LookAt.Target get forwarded");
        lookAt.BoneName = "Neck";
        Expect(s_lookAtBoneName == "Neck", "LookAt.BoneName set forwarded");
        Expect(lookAt.BoneName == "Neck", "LookAt.BoneName get forwarded");
    }

    static void RunSyncFireOneShotSmokeTests()
    {
        s_treeActive = 1;
        s_lastOneShotClip = "";

        AnimationSyncGroup group = AnimationSyncGroup.Create();
        ObjectHandle player = ObjectHandle.GetOrCreate(7);
        Expect(group.Join(player), "Sync Fire OneShot group join");

        SyncGroupFireInstruction[] instructions =
        [
            new SyncGroupFireInstruction
            {
                Player = player,
                ClipName = "trip",
            },
        ];
        Expect(group.Fire(instructions), "Sync Fire on active-tree member");
        Expect(s_lastOneShotClip == "trip", "Sync Fire routes to native OneShot path");
        Expect(s_treeActive == 1, "Sync Fire keeps tree active");

        Expect(group.Destroy(), "Sync Fire OneShot group destroy");
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
    static int StubAnimationTreeSetActive(ulong id, int active)
    {
        if (id == 0)
        {
            return Native.Error;
        }

        s_treeActive = active;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetActive(ulong id, int* outActive)
    {
        if (id == 0 || outActive == null)
        {
            return Native.Error;
        }

        *outActive = s_treeActive;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeTravel(ulong id, byte* stateName)
    {
        if (id == 0 || stateName == null)
        {
            return Native.Error;
        }

        s_lastTravelState = Utf8ToString(stateName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeStart(ulong id, byte* stateName)
    {
        if (id == 0 || stateName == null)
        {
            return Native.Error;
        }

        s_lastStartState = Utf8ToString(stateName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeSetBlendSpaceScalar(ulong id, byte* nodeName, float scalar)
    {
        if (id == 0 || nodeName == null)
        {
            return Native.Error;
        }

        s_lastBlendNode = Utf8ToString(nodeName);
        s_blendSpaceScalar = scalar;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetBlendSpaceScalar(ulong id, byte* nodeName, float* outScalar)
    {
        if (id == 0 || nodeName == null || outScalar == null)
        {
            return Native.Error;
        }

        *outScalar = s_blendSpaceScalar;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeRequestOneShot(ulong id, byte* clipName)
    {
        if (id == 0 || clipName == null)
        {
            return Native.Error;
        }

        s_lastOneShotClip = Utf8ToString(clipName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeSetAdd2Weight(ulong id, float weight)
    {
        if (id == 0)
        {
            return Native.Error;
        }

        s_add2Weight = weight;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetAdd2Weight(ulong id, float* outWeight)
    {
        if (id == 0 || outWeight == null)
        {
            return Native.Error;
        }

        *outWeight = s_add2Weight;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeSetBlendSpace2DParam(
        ulong id, byte* nodeName, float x, float y)
    {
        if (id == 0 || nodeName == null)
        {
            return Native.Error;
        }

        s_lastBlend2DNode = Utf8ToString(nodeName);
        s_blendSpace2DX = x;
        s_blendSpace2DY = y;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetBlendSpace2DParam(
        ulong id, byte* nodeName, float* outX, float* outY)
    {
        if (id == 0 || nodeName == null || outX == null || outY == null)
        {
            return Native.Error;
        }

        *outX = s_blendSpace2DX;
        *outY = s_blendSpace2DY;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeSetAssetGuid(ulong id, byte* guid)
    {
        if (id == 0 || guid == null)
        {
            return Native.Error;
        }

        s_treeAssetGuid = Utf8ToString(guid);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetAssetGuid(ulong id, byte* outGuid, int capacity)
    {
        if (id == 0 || outGuid == null || capacity <= 0)
        {
            return Native.Error;
        }

        WriteUtf8(s_treeAssetGuid, outGuid, capacity);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeSetTreeParamBool(ulong id, byte* name, int value)
    {
        if (id == 0 || name == null)
        {
            return Native.Error;
        }

        _ = value;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetTreeParamBool(ulong id, byte* name, int* outValue)
    {
        if (id == 0 || name == null || outValue == null)
        {
            return Native.Error;
        }

        *outValue = 0;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeSetTreeParamFloat(ulong id, byte* name, float value)
    {
        if (id == 0 || name == null)
        {
            return Native.Error;
        }

        _ = value;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationTreeGetTreeParamFloat(ulong id, byte* name, float* outValue)
    {
        if (id == 0 || name == null || outValue == null)
        {
            return Native.Error;
        }

        *outValue = 0f;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierCount(ulong id, int* outCount)
    {
        if (id == 0 || outCount == null)
        {
            return Native.Error;
        }

        *outCount = s_modifierCount;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetEnabled(ulong id, int index, int enabled)
    {
        if (id == 0 || index < 0 || index >= s_modifierEnabled.Length)
        {
            return Native.Error;
        }

        s_modifierEnabled[index] = enabled != 0;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetEnabled(ulong id, int index, int* outEnabled)
    {
        if (id == 0 || outEnabled == null || index < 0 ||
            index >= s_modifierEnabled.Length)
        {
            return Native.Error;
        }

        *outEnabled = s_modifierEnabled[index] ? 1 : 0;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierMove(ulong id, int fromIndex, int toIndex)
    {
        if (id == 0 || fromIndex < 0 || toIndex < 0 ||
            fromIndex >= s_modifierEnabled.Length ||
            toIndex >= s_modifierEnabled.Length)
        {
            return Native.Error;
        }

        (s_modifierEnabled[fromIndex], s_modifierEnabled[toIndex]) =
            (s_modifierEnabled[toIndex], s_modifierEnabled[fromIndex]);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetPaperMouthOpenAmount(ulong id, int index, float openAmount)
    {
        if (id == 0 || index != 0)
        {
            return Native.Error;
        }

        s_paperMouthOpenAmount = openAmount;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetPaperMouthOpenAmount(
        ulong id, int index, float* outOpenAmount)
    {
        if (id == 0 || index != 0 || outOpenAmount == null)
        {
            return Native.Error;
        }

        *outOpenAmount = s_paperMouthOpenAmount;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetPaperMouthBoneName(ulong id, int index, byte* boneName)
    {
        if (id == 0 || index != 0 || boneName == null)
        {
            return Native.Error;
        }

        s_paperMouthBoneName = Utf8ToString(boneName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetPaperMouthBoneName(
        ulong id, int index, byte* outBoneName, int nameCapacity)
    {
        if (id == 0 || index != 0 || outBoneName == null || nameCapacity <= 0)
        {
            return Native.Error;
        }

        WriteUtf8(s_paperMouthBoneName, outBoneName, nameCapacity);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetAttachBoneName(ulong id, int index, byte* boneName)
    {
        if (id == 0 || index != 1 || boneName == null)
        {
            return Native.Error;
        }

        s_attachBoneName = Utf8ToString(boneName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetAttachBoneName(
        ulong id, int index, byte* outBoneName, int nameCapacity)
    {
        if (id == 0 || index != 1 || outBoneName == null || nameCapacity <= 0)
        {
            return Native.Error;
        }

        WriteUtf8(s_attachBoneName, outBoneName, nameCapacity);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetAttachChildObjectId(
        ulong id, int index, ulong childObjectId)
    {
        if (id == 0 || index != 1)
        {
            return Native.Error;
        }

        s_attachChildObjectId = childObjectId;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetAttachChildObjectId(
        ulong id, int index, ulong* outChildObjectId)
    {
        if (id == 0 || index != 1 || outChildObjectId == null)
        {
            return Native.Error;
        }

        *outChildObjectId = s_attachChildObjectId;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetLookAtTarget(
        ulong id, int index, float x, float y, float z)
    {
        if (id == 0 || index != 2)
        {
            return Native.Error;
        }

        s_lookAtTargetX = x;
        s_lookAtTargetY = y;
        s_lookAtTargetZ = z;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetLookAtTarget(
        ulong id, int index, float* outX, float* outY, float* outZ)
    {
        if (id == 0 || index != 2 || outX == null || outY == null || outZ == null)
        {
            return Native.Error;
        }

        *outX = s_lookAtTargetX;
        *outY = s_lookAtTargetY;
        *outZ = s_lookAtTargetZ;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierSetLookAtBoneName(ulong id, int index, byte* boneName)
    {
        if (id == 0 || index != 2 || boneName == null)
        {
            return Native.Error;
        }

        s_lookAtBoneName = Utf8ToString(boneName);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubSkeletonModifierGetLookAtBoneName(
        ulong id, int index, byte* outBoneName, int nameCapacity)
    {
        if (id == 0 || index != 2 || outBoneName == null || nameCapacity <= 0)
        {
            return Native.Error;
        }

        WriteUtf8(s_lookAtBoneName, outBoneName, nameCapacity);
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetMethodKeyCount(ulong id, byte* clipName, int* outCount)
    {
        if (id == 0 || clipName == null || outCount == null)
        {
            return Native.Error;
        }

        *outCount = 1;
        return Native.Ok;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubAnimationGetMethodKey(
        ulong id, byte* clipName, int index, byte* outName, int nameCapacity, float* outTime)
    {
        if (id == 0 || clipName == null || outName == null || outTime == null ||
            nameCapacity <= 0 || index != 0)
        {
            return Native.Error;
        }

        WriteUtf8(s_methodKeyName, outName, nameCapacity);
        *outTime = s_methodKeyTime;
        return Native.Ok;
    }

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
        if (s_treeActive != 0 && instructionCount > 0 && instructions != null)
        {
            s_lastOneShotClip = Utf8ToString(instructions[0].clip_name);
        }
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

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    static int StubLog(int severity, byte* text, byte* stack)
    {
        s_lastLogSeverity = severity;
        s_lastLogText = Utf8ToString(text);
        s_lastLogStack = Utf8ToString(stack);
        return Native.Ok;
    }
}
