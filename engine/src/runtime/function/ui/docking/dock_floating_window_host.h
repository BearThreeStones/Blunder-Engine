#pragma once

#include <SDL3/SDL.h>

#include <functional>

#include <slint.h>

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_layout_model.h"
#include "runtime/function/ui/docking/dock_manager.h"
#include "runtime/function/ui/docking/dock_types.h"
#include "runtime/platform/window/child_window_registry.h"

namespace Blunder {

class SlintSystem;
class WindowSystem;

struct NativeFloatHierarchyRow {
  int entity_id{0};
  eastl::string name;
  int depth{0};
  bool expanded{false};
  bool has_children{false};
  bool selected{false};
};

struct NativeFloatBrowserTreeRow {
  eastl::string path;
  eastl::string name;
  int depth{0};
  bool is_dir{false};
  bool expanded{false};
  bool has_children{false};
};

struct NativeFloatBrowserGridRow {
  eastl::string path;
  eastl::string name;
  slint::Image thumb;
  bool is_dir{false};
};

struct NativeFloatBrowserPathSegment {
  eastl::string path;
  eastl::string name;
};

struct NativeFloatPanelSnapshot {
  DockPanelKind panel_kind{DockPanelKind::custom};
  eastl::vector<NativeFloatHierarchyRow> hierarchy_rows;
  int hierarchy_selected_entity_id{0};
  bool inspector_has_selection{false};
  eastl::string inspector_entity_name;
  float inspector_pos_x{0.0f};
  float inspector_pos_y{0.0f};
  float inspector_pos_z{0.0f};
  float inspector_rot_x{0.0f};
  float inspector_rot_y{0.0f};
  float inspector_rot_z{0.0f};
  float inspector_scale_x{1.0f};
  float inspector_scale_y{1.0f};
  float inspector_scale_z{1.0f};
  float light_dir_x{0.45f};
  float light_dir_y{0.7f};
  float light_dir_z{0.55f};
  float light_color_r{1.0f};
  float light_color_g{1.0f};
  float light_color_b{1.0f};
  float ambient_r{0.15f};
  float ambient_g{0.15f};
  float ambient_b{0.15f};
  float diffuse_r{1.0f};
  float diffuse_g{1.0f};
  float diffuse_b{1.0f};
  float specular_r{0.4f};
  float specular_g{0.4f};
  float specular_b{0.4f};
  float shininess{32.0f};
  bool shading_unlit{false};
  bool ssao_enabled{true};
  float ssao_radius{0.45f};
  float ssao_bias{0.025f};
  float ssao_strength{0.85f};
  eastl::vector<NativeFloatBrowserTreeRow> browser_tree_rows;
  eastl::vector<NativeFloatBrowserGridRow> browser_grid_rows;
  eastl::vector<NativeFloatBrowserPathSegment> browser_path_segments;
  bool browser_drag_active{false};
  eastl::string browser_drag_source_path;
  eastl::string browser_drop_highlight_path;
  bool browser_viewport_drop_active{false};
  eastl::string browser_status_text;
  eastl::string browser_selected_folder_path;
};

class DockFloatingWindowHost final {
 public:
  struct FloatEntry {
    DockId node_id{k_invalid_dock_id};
    DockId widget_id{k_invalid_dock_id};
    DockId container_id{k_invalid_dock_id};
    DockPanelKind panel_kind{DockPanelKind::custom};
    SDL_Window* sdl_window{nullptr};
  };

  struct Callbacks {
    std::function<void()> on_docking_dirty;
    std::function<void(DockId widget_id)> on_close_widget;
    std::function<void(DockId node_id, float x, float y, bool resize, DockResizeEdge edge)>
        on_floating_pressed;
    std::function<void(float x, float y)> on_floating_moved;
    std::function<void()> on_floating_released;
    std::function<void(DockId widget_id, float x, float y)> on_tab_pressed;
    std::function<void(float x, float y)> on_tab_moved;
    std::function<void(float x, float y)> on_tab_released;
    std::function<void(int entity_id)> on_hierarchy_entity_selected;
    std::function<void(int entity_id)> on_hierarchy_entity_toggle;
    std::function<void()> on_inspector_transform_edited;
    std::function<void(const slint::SharedString&)> on_browser_folder_selected;
    std::function<void(const slint::SharedString&)> on_browser_folder_toggle;
    std::function<void()> on_browser_refresh_requested;
    std::function<void()> on_browser_import_requested;
    std::function<void(const slint::SharedString&, float, float)> on_browser_item_press;
    std::function<void(const slint::SharedString&, float, float)> on_browser_item_move;
    std::function<void(const slint::SharedString&, float, float)> on_browser_item_release;
    std::function<void(const slint::SharedString&)> on_browser_search_changed;
    std::function<void(const slint::SharedString&)> on_browser_path_segment_clicked;
  };

  void setCallbacks(Callbacks callbacks) { m_callbacks = eastl::move(callbacks); }

  void initialize(WindowSystem* window_system, SlintSystem* slint_system);
  void shutdown();

  void sync(DockManager& manager, const DockLayoutModel& model, float docking_origin_x,
            float docking_origin_y);
  void setFloatVisible(DockId node_id, bool visible);
  void applyPanelSnapshot(DockId node_id, const NativeFloatPanelSnapshot& snapshot);
  void renderFrames();
  bool processEvent(const SDL_Event& event);

 private:
  FloatEntry* findEntry(DockId node_id);
  const FloatEntry* findEntry(DockId node_id) const;
  FloatEntry* findEntryByWindowId(SDL_WindowID window_id);
  void destroyEntry(FloatEntry& entry);
  void createEntry(const std::shared_ptr<DockNode>& node,
                   const std::shared_ptr<DockWidget>& widget, const DockRect& frame,
                   float docking_origin_x, float docking_origin_y);
  void syncTabs(DockManager& manager, FloatEntry& entry);
  void applySnapshotToEntry(FloatEntry& entry, const NativeFloatPanelSnapshot& snapshot);
  glm::vec2 toDockLocal(SDL_Window* float_window, float local_x, float local_y) const;

  WindowSystem* m_window_system{nullptr};
  SlintSystem* m_slint_system{nullptr};
  ChildWindowRegistry* m_child_windows{nullptr};
  float m_docking_origin_x{0.0f};
  float m_docking_origin_y{0.0f};
  Callbacks m_callbacks;
  eastl::hash_map<DockId, FloatEntry> m_entries;
};

}  // namespace Blunder
