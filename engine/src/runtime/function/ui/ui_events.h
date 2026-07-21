#pragma once

#include <cstdint>

#include "EASTL/string.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

enum class UiSelectionMode : uint8_t {
  replace = 0,
  add,
  toggle,
};

enum class UiEventKind : uint8_t {
  none = 0,
  selectEntity,
  toggleHierarchyNode,
  inspectorTransformEdited,
  saveScene,
  browserRefresh,
  browserFolderSelected,
  browserFolderToggle,
  browserSearchChanged,
  browserPathSegmentClicked,
  openSceneAsset,
  syncShadingFromAsset,
  undo,
  redo,
  play,
  playPause,
  playStop,
};

struct UiEvent {
  UiEventKind kind{UiEventKind::none};
  EntityId entity_id{k_invalid_entity_id};
  UiSelectionMode selection_mode{UiSelectionMode::replace};
  eastl::string path;

  static UiEvent selectEntity(EntityId id,
                              UiSelectionMode mode = UiSelectionMode::replace) {
    UiEvent e{};
    e.kind = UiEventKind::selectEntity;
    e.entity_id = id;
    e.selection_mode = mode;
    return e;
  }

  static UiEvent toggleHierarchyNode(EntityId id) {
    UiEvent e{};
    e.kind = UiEventKind::toggleHierarchyNode;
    e.entity_id = id;
    return e;
  }

  static UiEvent withPath(UiEventKind kind, eastl::string path) {
    UiEvent e{};
    e.kind = kind;
    e.path = eastl::move(path);
    return e;
  }

  static UiEvent simple(UiEventKind kind) {
    UiEvent e{};
    e.kind = kind;
    return e;
  }
};

}  // namespace Blunder
