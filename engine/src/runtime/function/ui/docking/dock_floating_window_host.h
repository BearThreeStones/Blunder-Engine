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
  bool is_last_sibling{false};
  int ancestor_cont_mask{0};
  eastl::vector<int> icon_kinds;
  eastl::vector<int> icon_indices;
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
  bool is_scene{false};
  bool selected{false};
  int type_kind{5};
  eastl::string type_label;
  eastl::string size_text;
  eastl::string date_text;
};

struct NativeFloatBrowserPathSegment {
  eastl::string path;
  eastl::string name;
};

struct NativeFloatBehaviourPropRow {
  eastl::string key;
  eastl::string kind;
  bool bool_value{false};
  float number_value{0.0f};
  eastl::string string_value;
  bool missing_type{false};
  bool clip_name_invalid{false};
};

struct NativeFloatBehaviourRow {
  int behaviour_id{0};
  eastl::string type_name;
  bool missing{false};
  eastl::vector<NativeFloatBehaviourPropRow> props;
};

struct NativeFloatAnimationClipRow {
  int entry_index{0};
  eastl::string clip_name;
  eastl::string clip_guid;
  eastl::string clip_display;
  bool clip_invalid{false};
};

struct NativeFloatSkeletonModifierRow {
  int modifier_index{0};
  eastl::string type_name;
  bool enabled{true};
  bool missing{false};
  eastl::string bone_name;
  float open_amount{0.0f};
  bool attach_driven{false};
  float target_x{0.0f};
  float target_y{0.0f};
  float target_z{1.0f};
  eastl::string child_entity_name;
};

struct NativeFloatHistoryRow {
  eastl::string label;
  int index{0};
  int stack{0};
  bool is_applied{false};
  bool is_current{false};
  bool is_group{false};
};

struct NativeFloatPanelSnapshot {
  DockPanelKind panel_kind{DockPanelKind::custom};
  eastl::vector<NativeFloatHierarchyRow> hierarchy_rows;
  int hierarchy_selected_entity_id{0};
  eastl::string hierarchy_scene_display_name;
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
  float inspector_quat_x{0.0f};
  float inspector_quat_y{0.0f};
  float inspector_quat_z{0.0f};
  float inspector_quat_w{1.0f};
  bool inspector_pos_x_mixed{false};
  bool inspector_pos_y_mixed{false};
  bool inspector_pos_z_mixed{false};
  bool inspector_rot_x_mixed{false};
  bool inspector_rot_y_mixed{false};
  bool inspector_rot_z_mixed{false};
  bool inspector_scale_x_mixed{false};
  bool inspector_scale_y_mixed{false};
  bool inspector_scale_z_mixed{false};
  bool inspector_transform_expanded{true};
  bool inspector_rotation_edit_mode_euler{true};
  bool inspector_scale_link_enabled{true};
  bool inspector_multi_edit_visible{false};
  bool inspector_multi_edit_absolute{true};
  eastl::vector<NativeFloatBehaviourRow> inspector_behaviours;
  eastl::vector<eastl::string> inspector_behaviour_type_choices;
  eastl::vector<eastl::string> inspector_behaviour_clip_name_choices;
  bool inspector_behaviours_expanded{true};
  eastl::vector<NativeFloatSkeletonModifierRow> inspector_skeleton_modifiers;
  eastl::vector<eastl::string> inspector_skeleton_modifier_type_choices;
  bool inspector_skeleton_modifiers_expanded{true};
  bool inspector_has_camera{false};
  float inspector_camera_fov{45.0f};
  float inspector_camera_near{0.1f};
  float inspector_camera_far{1000.0f};
  bool inspector_camera_is_main{false};
  bool inspector_camera_expanded{true};
  bool inspector_has_light{false};
  int inspector_light_type{0};
  float inspector_light_color_r{1.0f};
  float inspector_light_color_g{1.0f};
  float inspector_light_color_b{1.0f};
  float inspector_light_intensity{1.0f};
  bool inspector_light_enabled{true};
  int inspector_light_contribution{0};
  float inspector_light_range{10.0f};
  float inspector_light_inner_cone{0.0f};
  float inspector_light_outer_cone{45.0f};
  float inspector_light_width{1.0f};
  float inspector_light_height{1.0f};
  eastl::string inspector_light_linking_text;
  bool inspector_light_expanded{true};
  bool inspector_has_animation_player{false};
  eastl::vector<NativeFloatAnimationClipRow> inspector_animation_clips;
  bool inspector_animation_player_expanded{true};
  bool inspector_add_menu_enabled{false};
  bool inspector_has_skeleton{false};
  bool inspector_has_animation_tree{false};
  bool inspector_remove_skeleton_enabled{true};
  bool inspector_asset_mode{false};
  eastl::string inspector_asset_display_name;
  eastl::string inspector_asset_guid;
  eastl::string inspector_asset_type;
  eastl::string inspector_asset_intermediate_path;
  bool mesh_material_section_visible{false};
  bool mesh_material_unlit{false};
  float mesh_base_color_r{1.0f};
  float mesh_base_color_g{1.0f};
  float mesh_base_color_b{1.0f};
  float mesh_base_color_a{1.0f};
  float mesh_metallic{0.0f};
  float mesh_roughness{1.0f};
  float mesh_ambient_r{0.0f};
  float mesh_ambient_g{0.0f};
  float mesh_ambient_b{0.0f};
  float mesh_diffuse_r{1.0f};
  float mesh_diffuse_g{1.0f};
  float mesh_diffuse_b{1.0f};
  float mesh_specular_r{0.4f};
  float mesh_specular_g{0.4f};
  float mesh_specular_b{0.4f};
  float mesh_shininess{32.0f};
  eastl::string mesh_slot_base_color_name;
  bool mesh_slot_base_color_assigned{false};
  eastl::string mesh_slot_metallic_name;
  bool mesh_slot_metallic_assigned{false};
  eastl::string mesh_slot_normal_name;
  bool mesh_slot_normal_assigned{false};
  eastl::string mesh_slot_occlusion_name;
  bool mesh_slot_occlusion_assigned{false};
  eastl::vector<NativeFloatBrowserTreeRow> browser_tree_rows;
  eastl::vector<NativeFloatBrowserGridRow> browser_grid_rows;
  eastl::vector<NativeFloatBrowserPathSegment> browser_path_segments;
  bool browser_drag_active{false};
  eastl::string browser_drag_source_path;
  eastl::string browser_drop_highlight_path;
  bool browser_viewport_drop_active{false};
  eastl::string browser_status_text;
  eastl::string browser_selected_folder_path;
  float browser_thumb_size{64.0f};
  bool browser_details_view{false};
  int browser_sort_column{0};
  bool browser_sort_ascending{true};
  eastl::vector<NativeFloatHistoryRow> history_rows;
  bool history_filter_scene{true};
  bool history_filter_global{true};
  eastl::string browser_inline_rename_path;
  eastl::string browser_inline_rename_buffer;
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
    std::function<void(int entity_id)> on_hierarchy_entity_context_selected;
    std::function<void(int entity_id)> on_hierarchy_entity_toggle;
    std::function<void(int entity_id, const slint::SharedString& kind)> on_hierarchy_create_requested;
    std::function<void(int entity_id)> on_hierarchy_delete_requested;
    std::function<void(int entity_id, float mouse_x, float row_width)> on_hierarchy_icon_pressed;
    std::function<void()> on_inspector_activated;
    std::function<void(bool)> on_mesh_material_unlit_toggled;
    std::function<void(int, float, float, float, float)> on_mesh_material_scalar_committed;
    std::function<void()> on_mesh_material_reset_requested;
    std::function<void(int)> on_mesh_slot_pick_requested;
    std::function<void(int)> on_mesh_slot_clear_requested;
    std::function<void(int)> on_mesh_slot_drop_requested;
    std::function<void()> on_inspector_transform_edited;
    std::function<void(int, const slint::SharedString&)> on_inspector_field_text_committed;
    std::function<void(int, bool)> on_inspector_field_focus_changed;
    std::function<void(bool)> on_inspector_rotation_mode_changed;
    std::function<void(bool)> on_inspector_scale_link_toggled;
    std::function<void(bool)> on_inspector_multi_edit_mode_changed;
    std::function<void(const slint::SharedString&)> on_inspector_add_behaviour;
    std::function<void(int)> on_inspector_remove_behaviour;
    std::function<void(int, int)> on_inspector_reorder_behaviour;
    std::function<void(int, const slint::SharedString&, const slint::SharedString&, float, bool)>
        on_inspector_commit_behaviour_prop;
    std::function<void(const slint::SharedString&)> on_inspector_add_skeleton_modifier;
    std::function<void(int)> on_inspector_remove_skeleton_modifier;
    std::function<void(int, int)> on_inspector_reorder_skeleton_modifier;
    std::function<void(int, bool)> on_inspector_set_skeleton_modifier_enabled;
    std::function<void(int, const slint::SharedString&, const slint::SharedString&, float, bool)>
        on_inspector_commit_skeleton_modifier_field;
    std::function<void()> on_inspector_camera_edited;
    std::function<void()> on_inspector_light_edited;
    std::function<void(const slint::SharedString&)> on_inspector_add_unique_attachment;
    std::function<void(const slint::SharedString&)> on_inspector_remove_unique_attachment;
    std::function<void()> on_inspector_add_clip;
    std::function<void(int)> on_inspector_remove_clip;
    std::function<void(int, const slint::SharedString&, const slint::SharedString&)>
        on_inspector_commit_animation_clip;
    std::function<void(int)> on_inspector_open_clip_picker;
    std::function<void(const slint::SharedString&, const slint::SharedString&,
                       const slint::SharedString&)>
        on_inspector_pick_clip_choice;
    std::function<void(const slint::SharedString&)> on_browser_folder_selected;
    std::function<void(const slint::SharedString&)> on_browser_folder_toggle;
    std::function<void()> on_browser_refresh_requested;
    std::function<void()> on_browser_import_requested;
    std::function<void(const slint::SharedString&)> on_browser_new_scene_requested;
    std::function<void(const slint::SharedString&)> on_browser_new_folder_requested;
    std::function<void(const slint::SharedString&)> on_browser_rename_requested;
    std::function<void(const slint::SharedString&)> on_browser_delete_requested;
    std::function<void(const slint::SharedString&, bool, bool)> on_browser_grid_select;
    std::function<void(const slint::SharedString&, float, float)> on_browser_item_press;
    std::function<void(const slint::SharedString&, float, float)> on_browser_item_move;
    std::function<void(const slint::SharedString&, float, float)> on_browser_item_release;
    std::function<void(const slint::SharedString&)> on_browser_search_changed;
    std::function<void(const slint::SharedString&)> on_browser_path_segment_clicked;
    std::function<void(int)> on_browser_sort_clicked;
    std::function<void(bool, bool)> on_history_filter_changed;
    std::function<void(int, int)> on_history_entry_clicked;
    std::function<void(const slint::SharedString&)> on_browser_inline_rename_commit;
    std::function<void()> on_browser_inline_rename_cancel;
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
