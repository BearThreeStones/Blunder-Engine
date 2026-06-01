#include "runtime/function/ui/docking/dock_slint_bridge.h"

#include <glm/vec2.hpp>

#include "runtime/function/ui/docking/dock_manager.h"

namespace Blunder {

namespace {

slint::SharedString toSharedString(const eastl::string& value) {
  return slint::SharedString(value.c_str());
}

}

std::shared_ptr<slint::VectorModel<DockTile>> DockSlintBridge::buildTileModel(
    const DockLayoutModel& model) {
  auto rows = std::make_shared<slint::VectorModel<DockTile>>();
  for (const DockTileView& view : model.tiles) {
    DockTile row{};
    row.node_id = static_cast<int>(view.node_id);
    row.tab_x = view.tab_bar_rect.x;
    row.tab_y = view.tab_bar_rect.y;
    row.tab_width = view.tab_bar_rect.width;
    row.tab_height = view.tab_bar_rect.height;
    row.content_x = view.content_rect.x;
    row.content_y = view.content_rect.y;
    row.content_width = view.content_rect.width;
    row.content_height = view.content_rect.height;
    row.active_widget_id = static_cast<int>(view.active_widget_id);
    row.active_panel_kind = static_cast<int>(view.active_panel_kind);
    row.floating = view.floating;
    rows->push_back(row);
  }
  return rows;
}

std::shared_ptr<slint::VectorModel<DockTab>> DockSlintBridge::buildTabModel(
    const DockLayoutModel& model) {
  auto rows = std::make_shared<slint::VectorModel<DockTab>>();
  for (const DockTabView& view : model.tabs) {
    DockTab row{};
    row.widget_id = static_cast<int>(view.widget_id);
    row.container_id = static_cast<int>(view.container_id);
    row.title = toSharedString(view.title);
    row.panel_kind = static_cast<int>(view.panel_kind);
    row.active = view.active;
    row.x = view.rect.x;
    row.y = view.rect.y;
    row.width = view.rect.width;
    row.height = view.rect.height;
    rows->push_back(row);
  }
  return rows;
}

std::shared_ptr<slint::VectorModel<DockSplitterHandle>> DockSlintBridge::buildSplitterModel(
    const DockLayoutModel& model) {
  auto rows = std::make_shared<slint::VectorModel<DockSplitterHandle>>();
  for (const DockSplitterView& view : model.splitters) {
    DockSplitterHandle row{};
    row.node_id = static_cast<int>(view.node_id);
    row.x = view.rect.x;
    row.y = view.rect.y;
    row.width = view.rect.width;
    row.height = view.rect.height;
    row.vertical = view.vertical_handle;
    rows->push_back(row);
  }
  return rows;
}

std::shared_ptr<slint::VectorModel<DockGuide>> DockSlintBridge::buildGuideModel(
    const DockLayoutModel& model) {
  auto rows = std::make_shared<slint::VectorModel<DockGuide>>();
  for (const DockGuideView& view : model.guides) {
    DockGuide row{};
    row.slot = static_cast<int>(view.slot);
    row.x = view.rect.x;
    row.y = view.rect.y;
    row.width = view.rect.width;
    row.height = view.rect.height;
    row.highlighted = view.highlighted;
    rows->push_back(row);
  }
  return rows;
}

DockPreview DockSlintBridge::buildPreview(const DockLayoutModel& model) {
  DockPreview preview{};
  preview.visible = model.preview.visible;
  preview.x = model.preview.rect.x;
  preview.y = model.preview.rect.y;
  preview.width = model.preview.rect.width;
  preview.height = model.preview.rect.height;
  return preview;
}

void DockSlintBridge::applyLayout(const slint::ComponentHandle<DockingHost>& host,
                                  const DockLayoutModel& model) {
  host->set_tiles(buildTileModel(model));
  host->set_tabs(buildTabModel(model));
  host->set_splitters(buildSplitterModel(model));
  host->set_guides(buildGuideModel(model));
  host->set_preview(buildPreview(model));
  host->set_drag_active(model.preview.visible);
}

void DockSlintBridge::wireCallbacks(const slint::ComponentHandle<DockingHost>& host,
                                    DockManager& manager, std::function<void()> on_changed) {
  DockManager* manager_ptr = &manager;

  host->on_tab_pressed([manager_ptr, on_changed](int widget_id, float x, float y) {
    manager_ptr->beginDrag(static_cast<DockId>(widget_id), glm::vec2{x, y});
    if (on_changed) {
      on_changed();
    }
  });

  host->on_tab_moved([manager_ptr, on_changed](float x, float y) {
    manager_ptr->handleDragMove(glm::vec2{x, y});
    if (on_changed) {
      on_changed();
    }
  });

  host->on_tab_released([manager_ptr, on_changed](float x, float y) {
    manager_ptr->handleDragMove(glm::vec2{x, y});
    manager_ptr->endDrag();
    if (on_changed) {
      on_changed();
    }
  });

  host->on_tab_activated([manager_ptr, on_changed](int widget_id) {
    manager_ptr->activateWidget(static_cast<DockId>(widget_id));
    if (on_changed) {
      on_changed();
    }
  });

  host->on_widget_closed([manager_ptr, on_changed](int widget_id) {
    manager_ptr->closeWidget(static_cast<DockId>(widget_id));
    if (on_changed) {
      on_changed();
    }
  });

  host->on_splitter_pressed([](int, float, float) {});

  host->on_splitter_moved([manager_ptr, on_changed](int node_id, float x, float y) {
    manager_ptr->resizeSplitter(static_cast<DockId>(node_id), glm::vec2{x, y});
    if (on_changed) {
      on_changed();
    }
  });

  host->on_splitter_released([on_changed]() {
    if (on_changed) {
      on_changed();
    }
  });
}

}
