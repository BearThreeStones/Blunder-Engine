#pragma once

#include "EASTL/unique_ptr.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;

eastl::unique_ptr<IEditorCommand> makeSetEntityTransformCommand(
    SceneInstance* scene, EntityId entity_id, const Vec3& before_position,
    const Quat& before_rotation, const Vec3& before_scale,
    const Vec3& after_position, const Quat& after_rotation,
    const Vec3& after_scale, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSoftDeleteEntityCommand(
    SceneInstance* scene, EntityId entity_id,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

eastl::unique_ptr<IEditorCommand> makeSpawnEntityCommand(
    SceneInstance* scene, EntityId entity_id,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after);

}  // namespace Blunder
