#pragma once

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/inspector_add_ops.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"

namespace Blunder {

class AssetManager;
class SceneInstance;

eastl::unique_ptr<IEditorCommand> makeSetEntityTransformCommand(
    SceneInstance* scene, EntityId entity_id, const Vec3& before_position,
    const Quat& before_rotation, const Vec3& before_scale,
    const Vec3& after_position, const Quat& after_rotation,
    const Vec3& after_scale, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetCameraComponentCommand(
    SceneInstance* scene, EntityId entity_id, const CameraComponent& before_camera,
    const CameraComponent& after_camera, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetLightComponentCommand(
    SceneInstance* scene, EntityId entity_id, const LightComponent& before_light,
    const LightComponent& after_light, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetAnimationPlayerClipBindingsCommand(
    SceneInstance* scene, EntityId entity_id,
    eastl::vector<AnimationPlayer::ClipBinding> before_bindings,
    eastl::vector<AnimationPlayer::ClipBinding> after_bindings,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetAnimationPlayerTimeScaleCommand(
    SceneInstance* scene, EntityId entity_id, float before_scale, float after_scale,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeAlignCameraToViewCommand(
    SceneInstance* scene, EntityId entity_id, const Vec3& before_position,
    const Quat& before_rotation, const Vec3& before_scale,
    const CameraComponent& before_camera, const Vec3& after_position,
    const Quat& after_rotation, const Vec3& after_scale,
    const CameraComponent& after_camera, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::string deleteEntityCommandLabel(const eastl::string& entity_name);

eastl::unique_ptr<IEditorCommand> makeSoftDeleteEntityCommand(
    SceneInstance* scene, EntityId entity_id,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after,
    eastl::string label = eastl::string("Delete Entity"));

eastl::unique_ptr<IEditorCommand> makeSpawnEntityCommand(
    SceneInstance* scene, EntityId entity_id,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after,
    eastl::string label = eastl::string("Spawn Entity"));

eastl::unique_ptr<IEditorCommand> makeAddBehaviourCommand(
    SceneInstance* scene, EntityId entity_id, const eastl::string& clr_type,
    BehaviourId created_id, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeRemoveBehaviourCommand(
    SceneInstance* scene, EntityId entity_id, BehaviourId behaviour_id,
    size_t index_at_remove, const eastl::string& type_name,
    eastl::vector<SceneBehaviourProperty> properties,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeReorderBehavioursCommand(
    SceneInstance* scene, EntityId entity_id, size_t from_index,
    size_t to_index, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetBehaviourPropertyCommand(
    SceneInstance* scene, EntityId entity_id, BehaviourId behaviour_id,
    const eastl::string& key, Variant before_value, Variant after_value,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeAddSkeletonModifierCommand(
    SceneInstance* scene, EntityId entity_id, const eastl::string& type_name,
    size_t index_at_add, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeRemoveSkeletonModifierCommand(
    SceneInstance* scene, EntityId entity_id, size_t index_at_remove,
    SceneSkeletonModifierDef snapshot, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeReorderSkeletonModifiersCommand(
    SceneInstance* scene, EntityId entity_id, size_t from_index,
    size_t to_index, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetSkeletonModifierEnabledCommand(
    SceneInstance* scene, EntityId entity_id, size_t modifier_index,
    bool before_enabled, bool after_enabled,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSetSkeletonModifierDefCommand(
    SceneInstance* scene, EntityId entity_id, size_t modifier_index,
    SceneSkeletonModifierDef before_def, SceneSkeletonModifierDef after_def,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeAddUniqueAttachmentCommand(
    SceneInstance* scene, AssetManager* asset_manager, EntityId entity_id,
    InspectorUniqueKind kind, InspectorUniqueAddResult created,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeRemoveUniqueAttachmentCommand(
    SceneInstance* scene, AssetManager* asset_manager, EntityId entity_id,
    InspectorUniqueKind kind, InspectorUniqueRemoveSnapshot snapshot,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

}  // namespace Blunder
