#pragma once

#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;

/// Align rule: all currently Active → off; otherwise on.
bool alignedObjectActiveAfter(const SceneInstance& scene,
                              const eastl::vector<EntityId>& ids);

/// Apply aligned Object Active to `ids` and seal one Document History Command.
/// No-op when `ids` is empty. Applies immediately then push.
void commitObjectActiveSelection(SceneInstance* scene,
                                 const eastl::vector<EntityId>& ids);

/// Hierarchy checkbox: selected row uses align on the whole selection;
/// unselected row becomes the single selection and toggles that Object.
void commitHierarchyActiveCheckbox(SceneInstance* scene, EntityId clicked_id);

/// Inspector checkbox uses the same align as A on the current selection.
void commitInspectorActiveCheckbox(SceneInstance* scene);

/// Hierarchy A: align current selection. No-op if empty.
void commitHierarchyActiveToggle(SceneInstance* scene);

/// Rebuild Hierarchy row Object Active flags and mark Inspector/viewport dirty.
void refreshObjectActiveEditorUi(SceneInstance* scene);

}  // namespace Blunder
