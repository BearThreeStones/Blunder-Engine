#include "runtime/function/ui/docking/dock_floating_window_host.h"

#include <glm/vec2.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/function/ui/docking/dock_auto_hide.h"
#include "runtime/function/ui/docking/dock_floating.h"
#include "runtime/function/ui/docking/dock_node.h"
#include "runtime/function/ui/docking/dock_widget.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

namespace {

slint::SharedString toSharedString(const eastl::string& value) {
  return slint::SharedString(value.c_str());
}

eastl::hash_map<DockId, std::optional<slint::ComponentHandle<FloatingPanelWindow>>>
    g_float_components;

std::optional<slint::ComponentHandle<FloatingPanelWindow>>* componentFor(
    DockId node_id) {
  return &g_float_components[node_id];
}

}  // namespace

void DockFloatingWindowHost::initialize(WindowSystem* window_system,
                                        SlintSystem* slint_system) {
  m_window_system = window_system;
  m_slint_system = slint_system;
  m_child_windows = slint_system != nullptr ? &slint_system->childWindowRegistry() : nullptr;
}

void DockFloatingWindowHost::shutdown() {
  for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
    destroyEntry(it->second);
  }
  m_entries.clear();
  g_float_components.clear();
  if (m_child_windows) {
    m_child_windows->destroyAll();
  }
  m_window_system = nullptr;
  m_slint_system = nullptr;
}

DockFloatingWindowHost::FloatEntry* DockFloatingWindowHost::findEntry(DockId node_id) {
  const auto it = m_entries.find(node_id);
  return it != m_entries.end() ? &it->second : nullptr;
}

const DockFloatingWindowHost::FloatEntry* DockFloatingWindowHost::findEntry(
    DockId node_id) const {
  const auto it = m_entries.find(node_id);
  return it != m_entries.end() ? &it->second : nullptr;
}

DockFloatingWindowHost::FloatEntry* DockFloatingWindowHost::findEntryByWindowId(
    SDL_WindowID window_id) {
  for (auto& pair : m_entries) {
    if (pair.second.sdl_window != nullptr &&
        m_child_windows->windowId(pair.second.sdl_window) == window_id) {
      return &pair.second;
    }
  }
  return nullptr;
}

void DockFloatingWindowHost::destroyEntry(FloatEntry& entry) {
  SDL_Window* window = entry.sdl_window;
  if (m_slint_system != nullptr && window != nullptr) {
    m_slint_system->unregisterSlintChildAdapter(window);
  }
  if (auto* component = componentFor(entry.node_id); component && *component) {
    (*component)->operator->()->hide();
    component->reset();
    g_float_components.erase(entry.node_id);
  }
  if (window != nullptr && m_child_windows != nullptr) {
    m_child_windows->destroy(window);
    entry.sdl_window = nullptr;
  }
}

glm::vec2 DockFloatingWindowHost::toDockLocal(SDL_Window* float_window, float local_x,
                                              float local_y) const {
  return m_child_windows->floatLocalToDockLocal(float_window, local_x, local_y,
                                               m_docking_origin_x, m_docking_origin_y);
}

void DockFloatingWindowHost::syncTabs(DockManager& manager, FloatEntry& entry) {
  auto* component = componentFor(entry.node_id);
  if (!component || !*component) {
    return;
  }

  const std::shared_ptr<DockNode> float_node = manager.findNode(entry.node_id);
  if (!float_node || !float_node->isFloating()) {
    return;
  }
  const std::shared_ptr<DockNode> container = float_node->floatingContent();
  if (!container || !container->isContainer()) {
    return;
  }

  const DockLayoutMetrics& metrics = manager.metrics();
  const DockRect& frame = float_node->floatingRect();
  const float tab_h = metrics.chrome_bar_height;
  const auto& widgets = container->widgets();
  if (widgets.empty()) {
    (*component)->operator->()->set_tabs(std::make_shared<slint::VectorModel<DockTab>>());
    return;
  }

  float tab_w = metrics.tab_width;
  const float max_w =
      frame.width > 0.0f ? frame.width / static_cast<float>(widgets.size()) : tab_w;
  if (tab_w > max_w) {
    tab_w = max_w;
  }

  auto tab_model = std::make_shared<slint::VectorModel<DockTab>>();
  float tab_x = 0.0f;
  const DockAutoHideFlag auto_hide_config = manager.autoHideConfig();
  const DockId container_id = container->id();

  for (size_t i = 0; i < widgets.size(); ++i) {
    const auto& widget = widgets[i];
    if (!widget) {
      continue;
    }
    DockTab row{};
    row.widget_id = static_cast<int>(widget->id());
    row.container_id = static_cast<int>(container_id);
    row.title = toSharedString(widget->title());
    row.panel_kind = static_cast<int>(widget->panelKind());
    row.active = static_cast<int>(i) == container->activeIndex();
    row.show_pin = testAutoHideFlag(auto_hide_config,
                                    DockAutoHideFlag::dock_area_has_pin_button) &&
                   hasDockWidgetFeature(widget->features(), DockWidgetFeature::pinnable);
    row.x = tab_x;
    row.y = 0.0f;
    row.width = tab_w;
    row.height = tab_h;
    tab_x += tab_w;
    tab_model->push_back(row);
  }
  (*component)->operator->()->set_tabs(tab_model);
}

void DockFloatingWindowHost::applySnapshotToEntry(FloatEntry& entry,
                                                  const NativeFloatPanelSnapshot& snapshot) {
  auto* component = componentFor(entry.node_id);
  if (!component || !*component) {
    return;
  }
  auto& ui = *(*component)->operator->();
  switch (snapshot.panel_kind) {
    case DockPanelKind::hierarchy: {
      auto rows = std::make_shared<slint::VectorModel<HierarchyTreeRow>>();
      for (const NativeFloatHierarchyRow& row : snapshot.hierarchy_rows) {
        HierarchyTreeRow slint_row{};
        slint_row.entity_id = row.entity_id;
        slint_row.name = toSharedString(row.name);
        slint_row.depth = row.depth;
        slint_row.expanded = row.expanded;
        slint_row.has_children = row.has_children;
        slint_row.selected = row.selected;
        rows->push_back(slint_row);
      }
      ui.set_hierarchy_tree_rows(rows);
      ui.set_hierarchy_selected_entity_id(snapshot.hierarchy_selected_entity_id);
      break;
    }
    case DockPanelKind::inspector:
      ui.set_inspector_has_selection(snapshot.inspector_has_selection);
      ui.set_inspector_entity_name(toSharedString(snapshot.inspector_entity_name));
      ui.set_inspector_pos_x(snapshot.inspector_pos_x);
      ui.set_inspector_pos_y(snapshot.inspector_pos_y);
      ui.set_inspector_pos_z(snapshot.inspector_pos_z);
      ui.set_inspector_rot_x(snapshot.inspector_rot_x);
      ui.set_inspector_rot_y(snapshot.inspector_rot_y);
      ui.set_inspector_rot_z(snapshot.inspector_rot_z);
      ui.set_inspector_scale_x(snapshot.inspector_scale_x);
      ui.set_inspector_scale_y(snapshot.inspector_scale_y);
      ui.set_inspector_scale_z(snapshot.inspector_scale_z);
      ui.set_inspector_quat_x(snapshot.inspector_quat_x);
      ui.set_inspector_quat_y(snapshot.inspector_quat_y);
      ui.set_inspector_quat_z(snapshot.inspector_quat_z);
      ui.set_inspector_quat_w(snapshot.inspector_quat_w);
      ui.set_inspector_pos_x_mixed(snapshot.inspector_pos_x_mixed);
      ui.set_inspector_pos_y_mixed(snapshot.inspector_pos_y_mixed);
      ui.set_inspector_pos_z_mixed(snapshot.inspector_pos_z_mixed);
      ui.set_inspector_rot_x_mixed(snapshot.inspector_rot_x_mixed);
      ui.set_inspector_rot_y_mixed(snapshot.inspector_rot_y_mixed);
      ui.set_inspector_rot_z_mixed(snapshot.inspector_rot_z_mixed);
      ui.set_inspector_scale_x_mixed(snapshot.inspector_scale_x_mixed);
      ui.set_inspector_scale_y_mixed(snapshot.inspector_scale_y_mixed);
      ui.set_inspector_scale_z_mixed(snapshot.inspector_scale_z_mixed);
      ui.set_inspector_transform_expanded(snapshot.inspector_transform_expanded);
      ui.set_inspector_rotation_edit_mode_euler(
          snapshot.inspector_rotation_edit_mode_euler);
      ui.set_inspector_scale_link_enabled(snapshot.inspector_scale_link_enabled);
      ui.set_inspector_multi_edit_visible(snapshot.inspector_multi_edit_visible);
      ui.set_inspector_multi_edit_absolute(snapshot.inspector_multi_edit_absolute);
      ui.set_light_dir_x(snapshot.light_dir_x);
      ui.set_light_dir_y(snapshot.light_dir_y);
      ui.set_light_dir_z(snapshot.light_dir_z);
      ui.set_light_color_r(snapshot.light_color_r);
      ui.set_light_color_g(snapshot.light_color_g);
      ui.set_light_color_b(snapshot.light_color_b);
      ui.set_ambient_r(snapshot.ambient_r);
      ui.set_ambient_g(snapshot.ambient_g);
      ui.set_ambient_b(snapshot.ambient_b);
      ui.set_diffuse_r(snapshot.diffuse_r);
      ui.set_diffuse_g(snapshot.diffuse_g);
      ui.set_diffuse_b(snapshot.diffuse_b);
      ui.set_specular_r(snapshot.specular_r);
      ui.set_specular_g(snapshot.specular_g);
      ui.set_specular_b(snapshot.specular_b);
      ui.set_shininess(snapshot.shininess);
      ui.set_shading_unlit(snapshot.shading_unlit);
      ui.set_ssao_enabled(snapshot.ssao_enabled);
      ui.set_ssao_radius(snapshot.ssao_radius);
      ui.set_ssao_bias(snapshot.ssao_bias);
      ui.set_ssao_strength(snapshot.ssao_strength);
      break;

    case DockPanelKind::content_browser: {
      auto tree_model = std::make_shared<slint::VectorModel<BrowserTreeRow>>();
      for (const NativeFloatBrowserTreeRow& row : snapshot.browser_tree_rows) {
        BrowserTreeRow slint_row{};
        slint_row.path = toSharedString(row.path);
        slint_row.name = toSharedString(row.name);
        slint_row.depth = row.depth;
        slint_row.is_dir = row.is_dir;
        slint_row.expanded = row.expanded;
        slint_row.has_children = row.has_children;
        tree_model->push_back(slint_row);
      }
      ui.set_browser_tree_rows(tree_model);

      auto grid_model = std::make_shared<slint::VectorModel<BrowserGridRow>>();
      for (const NativeFloatBrowserGridRow& row : snapshot.browser_grid_rows) {
        BrowserGridRow slint_row{};
        slint_row.path = toSharedString(row.path);
        slint_row.name = toSharedString(row.name);
        slint_row.thumb = row.thumb;
        slint_row.is_dir = row.is_dir;
        grid_model->push_back(slint_row);
      }
      ui.set_browser_grid_rows(grid_model);

      auto path_model = std::make_shared<slint::VectorModel<BrowserPathSegment>>();
      for (const NativeFloatBrowserPathSegment& seg : snapshot.browser_path_segments) {
        BrowserPathSegment slint_seg{};
        slint_seg.path = toSharedString(seg.path);
        slint_seg.name = toSharedString(seg.name);
        path_model->push_back(slint_seg);
      }
      ui.set_browser_path_segments(path_model);

      ui.set_browser_drag_active(snapshot.browser_drag_active);
      ui.set_browser_drag_source_path(toSharedString(snapshot.browser_drag_source_path));
      ui.set_browser_drop_highlight_path(
          toSharedString(snapshot.browser_drop_highlight_path));
      ui.set_browser_viewport_drop_active(snapshot.browser_viewport_drop_active);
      ui.set_browser_status_text(toSharedString(snapshot.browser_status_text));
      ui.set_browser_selected_folder_path(
          toSharedString(snapshot.browser_selected_folder_path));
      break;
    }
    default:
      break;
  }
}

void DockFloatingWindowHost::applyPanelSnapshot(DockId node_id,
                                                const NativeFloatPanelSnapshot& snapshot) {
  FloatEntry* entry = findEntry(node_id);
  if (!entry) {
    return;
  }
  applySnapshotToEntry(*entry, snapshot);
}

void DockFloatingWindowHost::setFloatVisible(DockId node_id, bool visible) {
  FloatEntry* entry = findEntry(node_id);
  if (!entry || !entry->sdl_window) {
    return;
  }
  if (auto* component = componentFor(node_id); component && *component) {
    if (visible) {
      (*component)->operator->()->show();
    } else {
      (*component)->operator->()->hide();
    }
  }
  if (visible) {
    SDL_ShowWindow(entry->sdl_window);
  } else {
    SDL_HideWindow(entry->sdl_window);
  }
}

void DockFloatingWindowHost::createEntry(const std::shared_ptr<DockNode>& node,
                                         const std::shared_ptr<DockWidget>& widget,
                                         const DockRect& frame, float docking_origin_x,
                                         float docking_origin_y) {
  if (!m_slint_system || !widget || !node) {
    return;
  }
  if (!m_child_windows) {
    return;
  }
  const glm::ivec2 screen =
      m_child_windows->clientToScreen(docking_origin_x + frame.x, docking_origin_y + frame.y);
  SDL_Window* window = m_child_windows->create(
      widget->title().c_str(), screen.x, screen.y, static_cast<int>(frame.width),
      static_cast<int>(frame.height));
  if (!window) {
    return;
  }

  m_slint_system->setPendingSlintChildWindow(window);
  try {
    auto component = FloatingPanelWindow::create();
    const DockId node_id = node->id();

    component->set_node_id(static_cast<int>(node_id));
    component->set_panel_kind(static_cast<int>(widget->panelKind()));
    component->set_title_text(toSharedString(widget->title()));

    const DockId widget_id = widget->id();
    const DockId container_id =
        node->floatingContent() ? node->floatingContent()->id() : k_invalid_dock_id;

    component->on_close_pressed([this, widget_id]() {
      if (m_callbacks.on_close_widget) {
        m_callbacks.on_close_widget(widget_id);
      }
    });
    component->on_title_move_pressed([this, node_id, window](float x, float y) {
      if (m_callbacks.on_floating_pressed) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_floating_pressed(node_id, dock.x, dock.y, false, DockResizeEdge::none);
      }
      if (window) {
        m_child_windows->raise(window);
      }
    });
    component->on_title_move_moved([this, window](float x, float y) {
      if (m_callbacks.on_floating_moved) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_floating_moved(dock.x, dock.y);
      }
    });
    component->on_title_move_released([this]() {
      if (m_callbacks.on_floating_released) {
        m_callbacks.on_floating_released();
      }
    });
    component->on_grip_resize_pressed([this, node_id, window](int edge, float x, float y) {
      if (m_callbacks.on_floating_pressed) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_floating_pressed(node_id, dock.x, dock.y, true,
                                        static_cast<DockResizeEdge>(edge));
      }
    });
    component->on_grip_resize_moved([this, window](float x, float y) {
      if (m_callbacks.on_floating_moved) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_floating_moved(dock.x, dock.y);
      }
    });
    component->on_grip_resize_released([this]() {
      if (m_callbacks.on_floating_released) {
        m_callbacks.on_floating_released();
      }
    });
    component->on_tab_pressed([this, window](int widget_id, float x, float y) {
      if (m_callbacks.on_tab_pressed) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_tab_pressed(static_cast<DockId>(widget_id), dock.x, dock.y);
      }
    });
    component->on_tab_moved([this, window](float x, float y) {
      if (m_callbacks.on_tab_moved) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_tab_moved(dock.x, dock.y);
      }
    });
    component->on_tab_released([this, window](float x, float y) {
      if (m_callbacks.on_tab_released) {
        const glm::vec2 dock = toDockLocal(window, x, y);
        m_callbacks.on_tab_released(dock.x, dock.y);
      }
    });
    component->on_widget_closed([this](int widget_id) {
      if (m_callbacks.on_close_widget) {
        m_callbacks.on_close_widget(static_cast<DockId>(widget_id));
      }
    });
    component->on_hierarchy_entity_selected([this](int entity_id) {
      if (m_callbacks.on_hierarchy_entity_selected) {
        m_callbacks.on_hierarchy_entity_selected(entity_id);
      }
    });
    component->on_hierarchy_entity_toggle([this](int entity_id) {
      if (m_callbacks.on_hierarchy_entity_toggle) {
        m_callbacks.on_hierarchy_entity_toggle(entity_id);
      }
    });
    component->on_inspector_transform_edited([this]() {
      if (m_callbacks.on_inspector_transform_edited) {
        m_callbacks.on_inspector_transform_edited();
      }
    });
    component->on_inspector_field_text_committed(
        [this](int field_id, const slint::SharedString& text) {
          if (m_callbacks.on_inspector_field_text_committed) {
            m_callbacks.on_inspector_field_text_committed(field_id, text);
          }
        });
    component->on_inspector_field_focus_changed([this](int field_id, bool focused) {
      if (m_callbacks.on_inspector_field_focus_changed) {
        m_callbacks.on_inspector_field_focus_changed(field_id, focused);
      }
    });
    component->on_inspector_rotation_mode_changed([this](bool euler) {
      if (m_callbacks.on_inspector_rotation_mode_changed) {
        m_callbacks.on_inspector_rotation_mode_changed(euler);
      }
    });
    component->on_inspector_scale_link_toggled([this](bool linked) {
      if (m_callbacks.on_inspector_scale_link_toggled) {
        m_callbacks.on_inspector_scale_link_toggled(linked);
      }
    });
    component->on_inspector_multi_edit_mode_changed([this](bool absolute) {
      if (m_callbacks.on_inspector_multi_edit_mode_changed) {
        m_callbacks.on_inspector_multi_edit_mode_changed(absolute);
      }
    });
    component->on_browser_folder_selected([this](const slint::SharedString& path) {
      if (m_callbacks.on_browser_folder_selected) {
        m_callbacks.on_browser_folder_selected(path);
      }
    });
    component->on_browser_folder_toggle([this](const slint::SharedString& path) {
      if (m_callbacks.on_browser_folder_toggle) {
        m_callbacks.on_browser_folder_toggle(path);
      }
    });
    component->on_browser_refresh_requested([this]() {
      if (m_callbacks.on_browser_refresh_requested) {
        m_callbacks.on_browser_refresh_requested();
      }
    });
    component->on_browser_import_requested([this]() {
      if (m_callbacks.on_browser_import_requested) {
        m_callbacks.on_browser_import_requested();
      }
    });
    component->on_browser_item_press(
        [this](const slint::SharedString& path, float x, float y) {
          if (m_callbacks.on_browser_item_press) {
            m_callbacks.on_browser_item_press(path, x, y);
          }
        });
    component->on_browser_item_move(
        [this](const slint::SharedString& path, float x, float y) {
          if (m_callbacks.on_browser_item_move) {
            m_callbacks.on_browser_item_move(path, x, y);
          }
        });
    component->on_browser_item_release(
        [this](const slint::SharedString& path, float x, float y) {
          if (m_callbacks.on_browser_item_release) {
            m_callbacks.on_browser_item_release(path, x, y);
          }
        });
    component->on_browser_search_changed([this](const slint::SharedString& text) {
      if (m_callbacks.on_browser_search_changed) {
        m_callbacks.on_browser_search_changed(text);
      }
    });
    component->on_browser_path_segment_clicked([this](const slint::SharedString& path) {
      if (m_callbacks.on_browser_path_segment_clicked) {
        m_callbacks.on_browser_path_segment_clicked(path);
      }
    });

    component->show();

    FloatEntry entry{};
    entry.node_id = node_id;
    entry.widget_id = widget_id;
    entry.container_id = container_id;
    entry.panel_kind = widget->panelKind();
    entry.sdl_window = window;
    m_entries[node_id] = eastl::move(entry);
    *componentFor(node_id) = component;
  } catch (const std::exception& e) {
    LOG_ERROR("[DockFloatingWindowHost] failed to create Slint float window: {}", e.what());
    if (m_slint_system != nullptr) {
      m_slint_system->unregisterSlintChildAdapter(window);
    }
    m_child_windows->destroy(window);
  } catch (...) {
    LOG_ERROR("[DockFloatingWindowHost] failed to create Slint float window");
    if (m_slint_system != nullptr) {
      m_slint_system->unregisterSlintChildAdapter(window);
    }
    m_child_windows->destroy(window);
  }
}

void DockFloatingWindowHost::sync(DockManager& manager, const DockLayoutModel& model,
                                  float docking_origin_x, float docking_origin_y) {
  m_docking_origin_x = docking_origin_x;
  m_docking_origin_y = docking_origin_y;
  if (!testFloatingFlag(manager.floatingConfig(), DockFloatingFlag::native_os_window)) {
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
      destroyEntry(it->second);
    }
    m_entries.clear();
    return;
  }

  eastl::vector<DockId> active_ids;
  for (const std::shared_ptr<DockNode>& node : manager.floatingNodes()) {
    if (!node || !manager.isNativeFloating(node->id())) {
      continue;
    }
    active_ids.push_back(node->id());
    const std::shared_ptr<DockWidget> widget =
        node->floatingContent() ? node->floatingContent()->activeWidget() : nullptr;
    if (!widget) {
      continue;
    }
    FloatEntry* entry = findEntry(node->id());
    const DockRect frame = node->floatingRect();
    if (!entry) {
      createEntry(node, widget, frame, docking_origin_x, docking_origin_y);
      entry = findEntry(node->id());
    }
    if (!entry || !entry->sdl_window) {
      continue;
    }
    const glm::ivec2 screen = m_child_windows->clientToScreen(docking_origin_x + frame.x,
                                                             docking_origin_y + frame.y);
    m_child_windows->move(entry->sdl_window, screen.x, screen.y);
    m_child_windows->resize(entry->sdl_window, static_cast<int>(frame.width),
                         static_cast<int>(frame.height));
    syncTabs(manager, *entry);
  }

  eastl::vector<DockId> stale_ids;
  for (const auto& pair : m_entries) {
    if (eastl::find(active_ids.begin(), active_ids.end(), pair.first) == active_ids.end()) {
      stale_ids.push_back(pair.first);
    }
  }
  for (DockId stale_id : stale_ids) {
    destroyEntry(m_entries[stale_id]);
    m_entries.erase(stale_id);
  }
}

void DockFloatingWindowHost::renderFrames() {
  if (!m_slint_system || !m_child_windows) {
    return;
  }
  for (const auto& pair : m_entries) {
    SDL_Window* window = pair.second.sdl_window;
    if (!window) {
      continue;
    }
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) != 0) {
      continue;
    }
    const SDL_WindowID window_id = m_child_windows->windowId(window);
    if (window_id == 0) {
      continue;
    }
    if (SlintSystem::SlintWindowAdapter* adapter =
            m_slint_system->slintAdapterForWindow(window_id)) {
      m_slint_system->renderSlintAdapter(adapter);
    }
  }
}

bool DockFloatingWindowHost::processEvent(const SDL_Event& event) {
  if (!m_slint_system) {
    return false;
  }
  SDL_WindowID event_window_id = 0;
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      event_window_id = event.motion.windowID;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      event_window_id = event.button.windowID;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      event_window_id = event.wheel.windowID;
      break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      event_window_id = event.key.windowID;
      break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_EXPOSED:
      event_window_id = event.window.windowID;
      break;
    default:
      return false;
  }
  if (event_window_id == 0) {
    return false;
  }
  FloatEntry* entry = findEntryByWindowId(event_window_id);
  if (!entry) {
    return false;
  }
  if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
      event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    m_child_windows->raise(entry->sdl_window);
  }
  if (SlintSystem::SlintWindowAdapter* adapter =
          m_slint_system->slintAdapterForWindow(event_window_id)) {
    return m_slint_system->processSlintAdapterEvent(adapter, event);
  }
  return false;
}

}  // namespace Blunder
