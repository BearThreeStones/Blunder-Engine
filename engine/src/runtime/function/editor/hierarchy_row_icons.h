#pragma once

#include "runtime/function/editor/hierarchy_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;

void fillHierarchyRowIcons(const SceneInstance& scene, EntityId entity_id,
                           eastl::vector<HierarchyRowIconSlot>& out);

float hierarchyRowIconStripWidth(size_t icon_count);

/// Hit-test the right-aligned icon strip. Returns slot index or -1.
int hitTestHierarchyRowIconIndex(float mouse_x, float row_width, size_t icon_count);

bool hierarchyRowIconAttachmentPresent(const SceneInstance& scene, EntityId entity_id,
                                       HierarchyRowIconKind kind, int32_t index);

}  // namespace Blunder
