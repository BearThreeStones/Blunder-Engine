#pragma once

/* Generated with cbindgen:0.29.2 */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>
#include "private/slint_config.h"
#include "private/vtable.h"
#include "private/slint_string.h"
#include "private/slint_sharedvector.h"
#include "private/slint_keys.h"
#include "private/slint_properties.h"
#include "private/slint_callbacks.h"
#include "private/slint_color.h"
#include "private/slint_image.h"
#include "private/slint_pathdata.h"
#include "private/slint_brush.h"
#include "private/slint_generated_public.h"
#include "private/slint_enums_internal.h"
#include "private/slint_point.h"
#include "private/slint_timer.h"
#include "private/slint_builtin_structs_internal.h"
#include "private/slint_events_internal.h"

namespace slint {
    namespace private_api { class WindowAdapterRc; }
    namespace cbindgen_private {
        using slint::private_api::WindowAdapterRc;
        using namespace vtable;
        using private_api::Property;
        using private_api::PathData;
        using private_api::Point;
        struct ItemTreeVTable;
        struct ItemVTable;
        using types::IntRect;
        using types::Size;
        using types::MouseEvent;
    }
    template<typename ModelData> class Model;
}

namespace slint {
namespace cbindgen_private {

/// The maximum number of rectangles that can be stored in a DirtyRegion
constexpr static const uintptr_t DirtyRegion_MAX_COUNT = 3;

enum class TraversalOrder : uint8_t {
    BackToFront,
    FrontToBack,
};

/// This value is returned by the `input_event` function of an Item
/// to notify the run-time about how the event was handled and
/// what the next steps are.
/// See [`crate::items::ItemVTable::input_event`].
enum class InputEventResult : uint8_t {
    /// The event was accepted. This may result in additional events, for example
    /// accepting a mouse move will result in a MouseExit event later.
    EventAccepted,
    /// The event was ignored.
    EventIgnored,
    /// All further mouse events need to be sent to this item or component
    GrabMouse,
    /// Will start a drag operation. Can only be returned from a [`crate::items::DragArea`] item.
    StartDrag,
};

/// Represents how an item's focus_event handler dealt with a focus event.
/// An accepted event results in no further event propagation.
enum class FocusEventResult : uint8_t {
    /// The event was handled.
    FocusAccepted,
    /// The event was not handled and should be sent to other items.
    FocusIgnored,
};

/// Represents how an item's key_event handler dealt with a key event.
/// An accepted event results in no further event propagation.
enum class KeyEventResult : uint8_t {
    /// The event was handled.
    EventAccepted,
    /// The event was not handled and should be sent to other items.
    EventIgnored,
};

/// Returned by the `render()` function on items to indicate whether the rendering of
/// children should be handled by the caller, of if the item took care of that (for example
/// through layer indirection)
enum class RenderingResult {
    ContinueRenderingChildren,
    ContinueRenderingWithoutChildren,
};

/// The property names of the accessible-properties
enum class AccessibleStringProperty : uint32_t {
    Checkable,
    Checked,
    DelegateFocus,
    Description,
    Enabled,
    Expandable,
    Expanded,
    Id,
    ItemCount,
    ItemIndex,
    ItemSelectable,
    ItemSelected,
    Label,
    PlaceholderText,
    ReadOnly,
    Value,
    ValueMaximum,
    ValueMinimum,
    ValueStep,
};

enum class UndoItemKind {
    TextInsert,
    TextRemove,
};

/// This enum defines the different kinds of key events that can happen.
enum class KeyEventType : uint8_t {
    /// A key on a keyboard was pressed.
    KeyPressed = 0,
    /// A key on a keyboard was released.
    KeyReleased = 1,
    /// The input method updates the currently composed text. The KeyEvent's text field is the pre-edit text and
    /// composition_selection specifies the placement of the cursor within the pre-edit text.
    UpdateComposition = 2,
    /// The input method replaces the currently composed text with the final result of the composition.
    CommitComposition = 3,
};

/// The clip board, used in [`Platform::clipboard_text`] and [Platform::set_clipboard_text`]
enum class Clipboard : uint8_t {
    /// This is the default clipboard used for text action for Ctrl+V,  Ctrl+C.
    /// Corresponds to the secondary clipboard on X11.
    DefaultClipboard = 0,
    /// This is the clipboard that is used when text is selected
    /// Corresponds to the primary clipboard on X11.
    /// The Platform implementation should do nothing if copy on select is not supported on that platform.
    SelectionClipboard = 1,
};

/// A change tracker is used to run a callback when a property value changes.
///
/// The Change Tracker must be initialized with the [`Self::init`] method.
///
/// When the property changes, the ChangeTracker is added to a thread local list, and the notify
/// callback is called when the [`Self::run_change_handlers()`] method is called
struct ChangeTracker;

struct FlickableData;

/// This struct is used to pass key events to the runtime.
struct InternalKeyEvent;

/// Alias for `&mut dyn ItemRenderer`. Required so cbindgen generates the ItemVTable
/// despite the presence of trait object
struct ItemRendererRef;

struct KeyBindingList;

template<typename T = void>
struct Option;

/// The return value of the ItemTree::visit_children_item function
///
/// Represents something like `enum { Continue, Aborted{aborted_at_item: isize} }`.
/// But this is just wrapping a int because it is easier to use ffi with isize than
/// complex enum.
///
/// -1 means the visitor will continue
/// otherwise this is the index of the item that aborted the visit.
using VisitChildrenResult = uint64_t;
/// The result used for a visitor that want to continue the visit
constexpr static const VisitChildrenResult VisitChildrenResult_CONTINUE = UINT64_MAX;

/// A ItemRc is holding a reference to a ItemTree containing the item, and the index of this item
struct ItemRc {
    VRc<ItemTreeVTable> item_tree;
    uint32_t index;
};

/// The constraint that applies to a layout
struct LayoutInfo {
    /// The maximum size for the item.
    float max;
    /// The maximum size in percentage of the parent (value between 0 and 100).
    float max_percent;
    /// The minimum size for this item.
    float min;
    /// The minimum size in percentage of the parent (value between 0 and 100).
    float min_percent;
    /// the preferred size
    float preferred;
    /// the  stretch factor
    float stretch;
    inline LayoutInfo merge(const LayoutInfo &other) const;
    friend inline LayoutInfo operator+(const LayoutInfo &a, const LayoutInfo &b) { return a.merge(b); }
    friend bool operator==(const LayoutInfo&, const LayoutInfo&) = default;
};

/// This value is returned by the `input_event_filter_before_children` function, which
/// can specify how to further process the event.
/// See [`crate::items::ItemVTable::input_event_filter_before_children`].
struct InputEventFilterResult {
    enum class Tag {
        /// The event is going to be forwarded to children, then the [`crate::items::ItemVTable::input_event`]
        /// function is called
        ForwardEvent,
        /// The event will be forwarded to the children, but the [`crate::items::ItemVTable::input_event`] is not
        /// going to be called for this item
        ForwardAndIgnore,
        /// Just like `ForwardEvent`, but even in the case that children grabs the mouse, this function
        /// will still be called for further events
        ForwardAndInterceptGrab,
        /// The event will not be forwarded to children, if a child already had the grab, the
        /// grab will be cancelled with a [`MouseEvent::Exit`] event
        Intercept,
        /// The event will be forwarded to the children with a delay (in milliseconds), unless it is
        /// being intercepted.
        /// This is what happens when the flickable wants to delay the event.
        /// This should only be used for Press event, and the event will be sent after the delay, or
        /// if a release event is seen before that delay
        /// If any other component is handling the event it will be not handled by the component returned this result
        DelayForwarding,
    };

    struct DelayForwarding_Body {
        uint64_t _0;
    };

    Tag tag;
    union {
        DelayForwarding_Body delay_forwarding;
    };
};

/// This event is sent to a component and items when they receive or lose
/// the keyboard focus.
union FocusEvent {
    enum class Tag : uint8_t {
        /// This event is sent when an item receives the focus.
        FocusIn,
        /// This event is sent when an item loses the focus.
        FocusOut,
    };

    struct FocusIn_Body {
        Tag tag;
        FocusReason _0;
    };

    struct FocusOut_Body {
        Tag tag;
        FocusReason _0;
    };

    struct {
        Tag tag;
    };
    FocusIn_Body focus_in;
    FocusOut_Body focus_out;
};

/// A size represented in the coordinate space of logical pixels. That is the space before applying
/// a display device specific scale factor.
struct LogicalSize {
    /// The width in logical pixels.
    float width;
    /// The height in logical.
    float height;
};

/// Items are the nodes in the render tree.
struct ItemVTable {
    /// This function is called by the run-time after the memory for the item
    /// has been allocated and initialized. It will be called before any user specified
    /// bindings are set.
    void (*init)(Pin<VRef<ItemVTable>>, const ItemRc *my_item);
    void (*deinit)(Pin<VRef<ItemVTable>>, const WindowAdapterRc *window_adapter);
    /// offset in bytes from the *const ItemImpl.
    /// usize::MAX  means None
    uintptr_t cached_rendering_data_offset;
    /// We would need max/min/preferred size, and all layout info.
    /// `cross_axis_constraint` is the available width when querying vertical
    /// layout info, enabling height-for-width (Text word-wrap, Image aspect
    /// ratio). Negative values mean unconstrained.
    LayoutInfo (*layout_info)(Pin<VRef<ItemVTable>>,
                              Orientation orientation,
                              float cross_axis_constraint,
                              const WindowAdapterRc *window_adapter,
                              const ItemRc *self_rc);
    /// Event handler for mouse and touch event. This function is called before being called on children.
    /// Then, depending on the return value, it is called for the children, and their children, then
    /// [`Self::input_event`] is called on the children, and finally [`Self::input_event`] is called
    /// on this item again.
    ///
    /// The `cursor` argument needs to be changed by either this function ot the `input_event` function
    /// if this item wants to change the cursor.
    /// The value of `cursor` is always reset to `MouseCursor::Default` before dispatching the event,
    /// so any call to this function need to set the cursor
    InputEventFilterResult (*input_event_filter_before_children)(Pin<VRef<ItemVTable>>,
                                                                 const MouseEvent*,
                                                                 const WindowAdapterRc *window_adapter,
                                                                 const ItemRc *self_rc,
                                                                 MouseCursor *cursor);
    /// Handle input event for mouse and touch event
    InputEventResult (*input_event)(Pin<VRef<ItemVTable>>,
                                    const MouseEvent*,
                                    const WindowAdapterRc *window_adapter,
                                    const ItemRc *self_rc,
                                    MouseCursor *cursor);
    FocusEventResult (*focus_event)(Pin<VRef<ItemVTable>>,
                                    const FocusEvent*,
                                    const WindowAdapterRc *window_adapter,
                                    const ItemRc *self_rc);
    /// Called on the parents of the focused item, allowing for global shortcuts and similar
    /// overrides of the default actions.
    KeyEventResult (*capture_key_event)(Pin<VRef<ItemVTable>>,
                                        const InternalKeyEvent*,
                                        const WindowAdapterRc *window_adapter,
                                        const ItemRc *self_rc);
    KeyEventResult (*key_event)(Pin<VRef<ItemVTable>>,
                                const InternalKeyEvent*,
                                const WindowAdapterRc *window_adapter,
                                const ItemRc *self_rc);
    RenderingResult (*render)(Pin<VRef<ItemVTable>>,
                              ItemRendererRef *backend,
                              const ItemRc *self_rc,
                              LogicalSize size);
    /// Returns the rendering bounding rect for that particular item in the parent's item coordinate
    /// (same coordinate system as the geometry)
    LogicalRect (*bounding_rect)(Pin<VRef<ItemVTable>>,
                                 const WindowAdapterRc *window_adapter,
                                 const ItemRc *self_rc,
                                 LogicalRect geometry);
    bool (*clips_children)(Pin<VRef<ItemVTable>>);
};

/// Object to be passed in visit_item_children method of the ItemTree.
struct ItemVisitorVTable {
    /// Called for each child of the visited item
    ///
    /// The `item_tree` parameter is the ItemTree in which the item live which might not be the same
    /// as the parent's ItemTree.
    /// `index` is to be used again in the visit_item_children function of the ItemTree (the one passed as parameter)
    /// and `item` is a reference to the item itself
    VisitChildrenResult (*visit_item)(VRefMut<ItemVisitorVTable>,
                                      const VRc<ItemTreeVTable, Dyn> *item_tree,
                                      uint32_t index,
                                      Pin<VRef<ItemVTable>> item);
    /// Destructor
    void (*drop)(VRefMut<ItemVisitorVTable>);
};

/// A range of indices
struct IndexRange {
    /// Start index
    uintptr_t start;
    /// Index one past the last index
    uintptr_t end;
};

/// The item tree is an array of ItemTreeNode representing a static tree of items
/// within a ItemTree.
union ItemTreeNode {
    enum class Tag : uint8_t {
        /// Static item
        Item,
        /// A placeholder for many instance of item in their own ItemTree which
        /// are instantiated according to a model like repeaters
        DynamicTree,
    };

    struct Item_Body {
        Tag tag;
        /// True when the item has accessibility properties attached
        bool is_accessible;
        /// number of children
        uint32_t children_count;
        /// index of the first children within the item tree
        uint32_t children_index;
        /// The index of the parent item (not valid for the root)
        uint32_t parent_index;
        /// The index in the extra item_array
        uint32_t item_array_index;
    };

    struct DynamicTree_Body {
        Tag tag;
        /// the index which is passed in the visit_dynamic callback.
        uint32_t index;
        /// The index of the parent item (not valid for the root)
        uint32_t parent_index;
    };

    struct {
        Tag tag;
    };
    Item_Body item;
    DynamicTree_Body dynamic_tree;
    constexpr ItemTreeNode(Item_Body x) : item {x} {}
    constexpr ItemTreeNode(DynamicTree_Body x) : dynamic_tree{x} {}
};

/// Type alias to the commonly used VWeak<ItemTreeVTable, Dyn>>
using ItemTreeWeak = VWeak<ItemTreeVTable, Dyn>;

/// A Weak reference to an item that can be constructed from an ItemRc.
struct ItemWeak {
    ItemTreeWeak item_tree;
    uint32_t index;
};

/// The argument of an accessible action.
union AccessibilityAction {
    enum class Tag : uint32_t {
        Default,
        Decrement,
        Increment,
        Expand,
        /// This is currently unused
        ReplaceSelectedText,
        SetValue,
    };

    struct ReplaceSelectedText_Body {
        Tag tag;
        SharedString _0;
    };

    struct SetValue_Body {
        Tag tag;
        SharedString _0;
    };

    struct {
        Tag tag;
    };
    ReplaceSelectedText_Body replace_selected_text;
    SetValue_Body set_value;
};

/// Define a accessibility actions that supported by an item.
using SupportedAccessibilityAction = uint32_t;
constexpr static const SupportedAccessibilityAction SupportedAccessibilityAction_Default = (uint32_t)1;
constexpr static const SupportedAccessibilityAction SupportedAccessibilityAction_Decrement = (uint32_t)(1 << 1);
constexpr static const SupportedAccessibilityAction SupportedAccessibilityAction_Increment = (uint32_t)(1 << 2);
constexpr static const SupportedAccessibilityAction SupportedAccessibilityAction_Expand = (uint32_t)(1 << 3);
constexpr static const SupportedAccessibilityAction SupportedAccessibilityAction_ReplaceSelectedText = (uint32_t)(1 << 4);
constexpr static const SupportedAccessibilityAction SupportedAccessibilityAction_SetValue = (uint32_t)(1 << 5);

/// A ItemTree is representing an unit that is allocated together
struct ItemTreeVTable {
    /// Visit the children of the item at index `index`.
    /// Note that the root item is at index 0, so passing 0 would visit the item under root (the children of root).
    /// If you want to visit the root item, you need to pass -1 as an index.
    VisitChildrenResult (*visit_children_item)(Pin<VRef<ItemTreeVTable>>,
                                               intptr_t index,
                                               TraversalOrder order,
                                               VRefMut<ItemVisitorVTable> visitor);
    /// Return a reference to an item using the given index
    Pin<VRef<ItemVTable>> (*get_item_ref)(Pin<VRef<ItemTreeVTable>>, uint32_t index);
    /// Return the range of indices below the dynamic `ItemTreeNode` at `index`
    IndexRange (*get_subtree_range)(Pin<VRef<ItemTreeVTable>>, uint32_t index);
    /// Return the `ItemTreeRc` at `subindex` below the dynamic `ItemTreeNode` at `index`
    void (*get_subtree)(Pin<VRef<ItemTreeVTable>>,
                        uint32_t index,
                        uintptr_t subindex,
                        VWeak<ItemTreeVTable, Dyn> *result);
    /// Return the item tree that is defined by this `ItemTree`.
    Slice<ItemTreeNode> (*get_item_tree)(Pin<VRef<ItemTreeVTable>>);
    /// Return the node this ItemTree is a part of in the parent ItemTree.
    ///
    /// The return value is an item weak because it can be null if there is no parent.
    /// And the return value is passed by &mut because ItemWeak has a destructor
    /// Note that the returned value will typically point to a repeater node, which is
    /// strictly speaking not an Item at all!
    ///
    void (*parent_node)(Pin<VRef<ItemTreeVTable>>, ItemWeak *result);
    /// This embeds this ItemTree into the item tree of another ItemTree
    ///
    /// Returns `true` if this ItemTree was embedded into the `parent`
    /// at `parent_item_tree_index`.
    bool (*embed_component)(Pin<VRef<ItemTreeVTable>>,
                            const VWeak<ItemTreeVTable> *parent,
                            uint32_t parent_item_tree_index);
    /// Return the index of the current subtree or usize::MAX if this is not a subtree
    uintptr_t (*subtree_index)(Pin<VRef<ItemTreeVTable>>);
    /// Returns the layout info for the root of the ItemTree
    LayoutInfo (*layout_info)(Pin<VRef<ItemTreeVTable>>, Orientation);
    /// Returns the item's geometry (relative to its parent item)
    LogicalRect (*item_geometry)(Pin<VRef<ItemTreeVTable>>, uint32_t item_index);
    /// Returns the accessible role for a given item
    AccessibleRole (*accessible_role)(Pin<VRef<ItemTreeVTable>>, uint32_t item_index);
    /// Returns the accessible property via the `result`. Returns true if such a property exists.
    bool (*accessible_string_property)(Pin<VRef<ItemTreeVTable>>,
                                       uint32_t item_index,
                                       AccessibleStringProperty what,
                                       SharedString *result);
    /// Executes an accessibility action.
    void (*accessibility_action)(Pin<VRef<ItemTreeVTable>>,
                                 uint32_t item_index,
                                 const AccessibilityAction *action);
    /// Returns the supported accessibility actions.
    SupportedAccessibilityAction (*supported_accessibility_actions)(Pin<VRef<ItemTreeVTable>>,
                                                                    uint32_t item_index);
    /// Add the `ElementName::id` entries of the given item
    bool (*item_element_infos)(Pin<VRef<ItemTreeVTable>>, uint32_t item_index, SharedString *result);
    /// Returns a Window, creating a fresh one if `do_create` is true.
    void (*window_adapter)(Pin<VRef<ItemTreeVTable>>,
                           bool do_create,
                           Option<WindowAdapterRc> *result);
    /// in-place destructor (for VRc)
    Layout (*drop_in_place)(VRefMut<ItemTreeVTable>);
    /// dealloc function (for VRc)
    void (*dealloc)(const ItemTreeVTable*, uint8_t *ptr, Layout layout);
};

/// Type alias to the commonly used VRc<ItemTreeVTable, Dyn>>
using ItemTreeRc = VRc<ItemTreeVTable, Dyn>;

/// Same layout as WindowAdapterRc
struct WindowAdapterRcOpaque {
    const void *_0;
    const void *_1;
};

/// Alias for `vtable::VRef<ItemTreeVTable>` which represent a pointer to a `dyn ItemTree` with
/// the associated vtable
using ItemTreeRef = VRef<ItemTreeVTable>;

/// Type alias to the commonly used `Pin<VRef<ItemTreeVTable>>>`
using ItemTreeRefPin = Pin<ItemTreeRef>;

/// Four distances from the edges of a rectangle represented in the coordinate space of logical pixels.
struct LogicalEdges {
    /// The top distance in logical pixels.
    float top;
    /// The bottom distance in logical pixels.
    float bottom;
    /// The left distance in logical pixels.
    float left;
    /// The right distance in logical pixels.
    float right;
};

/// This structure must be present in items that are Rendered and contains information.
/// Used by the backend.
struct CachedRenderingData {
    /// Used and modified by the backend, should be initialized to 0 by the user code
    uintptr_t cache_index;
    /// Used and modified by the backend, should be initialized to 0 by the user code.
    /// The backend compares this generation against the one of the cache to verify
    /// the validity of the cache_index field.
    uintptr_t cache_generation;
    constexpr CachedRenderingData() : cache_index{}, cache_generation{} {}
};

/// The implementation of the `Window` element
struct WindowItem {
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<LogicalEdges> safe_area_insets;
    Property<LogicalPoint> virtual_keyboard_position;
    Property<LogicalSize> virtual_keyboard_size;
    Property<Brush> background;
    Property<SharedString> title;
    Property<bool> no_frame;
    Property<LogicalLength> resize_border_width;
    Property<bool> always_on_top;
    Property<bool> full_screen;
    Property<Image> icon;
    Property<SharedString> default_font_family;
    Property<LogicalLength> default_font_size;
    Property<int32_t> default_font_weight;
    CachedRenderingData cached_rendering_data;
};

/// The implementation used for `ContextMenuArea` and `ContextMenuInternal` elements
struct ContextMenu {
    private_api::CallbackHelper<MenuEntry, std::shared_ptr<slint::Model<MenuEntry>>> sub_menu;
    private_api::CallbackHelper<MenuEntry> activated;
    private_api::CallbackHelper<slint::LogicalPosition> show;
    CachedRenderingData cached_rendering_data;
    uint32_t popup_id;
    Property<bool> enabled;
#if defined(__ANDROID__)
    Option<Timer> long_press_timer
#endif
    ;
};

/// Wraps the internal data structure for the Flickable
struct FlickableDataBox {
struct FlickableData;
    FlickableData *_0;
};

/// Similar as `Option<core::ops::Range<i32>>` but `repr(C)`
///
/// This is the selection within a preedit
struct PreEditSelection {
    bool valid;
    int32_t start;
    int32_t end;
};

struct UndoItem {
    uintptr_t pos;
    SharedString text;
    uintptr_t cursor;
    uintptr_t anchor;
    UndoItemKind kind;
};

/// The implementation of the `TextInput` element
struct TextInput {
    Property<SharedString> text;
    Property<SharedString> font_family;
    Property<LogicalLength> font_size;
    Property<int32_t> font_weight;
    Property<bool> font_italic;
    Property<Brush> color;
    Property<Color> selection_foreground_color;
    Property<Color> selection_background_color;
    Property<TextHorizontalAlignment> horizontal_alignment;
    Property<TextVerticalAlignment> vertical_alignment;
    Property<TextWrap> wrap;
    Property<InputType> input_type;
    Property<LogicalLength> letter_spacing;
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<int32_t> cursor_position_byte_offset;
    Property<int32_t> anchor_position_byte_offset;
    Property<LogicalLength> text_cursor_width;
    Property<LogicalLength> page_height;
    Property<bool> cursor_visible;
    Property<bool> has_focus;
    Property<bool> enabled;
    private_api::CallbackHelper<void> accepted;
    private_api::CallbackHelper<slint::LogicalPosition> cursor_position_changed;
    private_api::CallbackHelper<void> edited;
    private_api::CallbackHelper<KeyEvent, EventResult> key_pressed;
    private_api::CallbackHelper<KeyEvent, EventResult> key_released;
    Property<bool> single_line;
    Property<bool> read_only;
    Property<SharedString> preedit_text;
    /// A selection within the preedit (cursor and anchor)
    Property<PreEditSelection> preedit_selection;
    CachedRenderingData cached_rendering_data;
    float preferred_x_pos;
    /// 0 = not pressed, 1 = single press, 2 = double clicked+press , ...
    uint8_t pressed;
    SharedVector<UndoItem> undo_items;
    SharedVector<UndoItem> redo_items;
};

struct SwipeGestureHandler {
    Property<bool> enabled;
    Property<bool> handle_swipe_left;
    Property<bool> handle_swipe_right;
    Property<bool> handle_swipe_up;
    Property<bool> handle_swipe_down;
    private_api::CallbackHelper<void> moved;
    private_api::CallbackHelper<void> swiped;
    private_api::CallbackHelper<void> cancelled;
    Property<LogicalPosition> pressed_position;
    Property<LogicalPosition> current_position;
    Property<bool> swiping;
    bool pressed;
    /// FIXME: remove this
    CachedRenderingData cached_rendering_data;
};

/// An optimized ShortcutList that is only initialized when it is
/// first accessed.
struct MaybeKeyBindingList {
struct KeyBindingList;
    const KeyBindingList *_0;
};

/// The input data for a cell of a GridLayout, before row/col determination and before H/V split
/// Used as input to organize_grid_layout()
struct GridLayoutInputData {
    /// whether this cell is the first one in a Row element
    bool new_row;
    /// col and row number.
    /// Only ROW_COL_AUTO and the u16 range are valid, values outside of
    /// that will be clamped with a warning at runtime
    float col;
    float row;
    /// colspan and rowspan
    /// Only the u16 range is valid, values outside of that will be clamped with a warning at runtime
    float colspan;
    float rowspan;
};

/// The organized layout data for a GridLayout, after row/col determination:
/// For each cell, stores col, colspan, row, rowspan
using GridLayoutOrganizedData = SharedVector<uint16_t>;

struct Padding {
    float begin;
    float end;
};

/// The horizontal or vertical data for all cells of a GridLayout, used as input to solve_grid_layout()
struct GridLayoutData {
    float size;
    float spacing;
    Padding padding;
    GridLayoutOrganizedData organized_data;
};

/// The information about a single item in a box or grid layout
struct LayoutItemInfo {
    LayoutInfo constraint;
};

/// The BoxLayoutData is used to represent both a Horizontal and Vertical layout.
/// The width/height x/y correspond to that of a horizontal layout.
/// For vertical layout, they are inverted
struct BoxLayoutData {
    float size;
    float spacing;
    Padding padding;
    LayoutAlignment alignment;
    Slice<LayoutItemInfo> cells;
};

/// The information about a single item in a flexbox layout
struct FlexboxLayoutItemInfo {
    LayoutInfo constraint;
    /// Flex grow factor (0 = don't grow, default)
    float flex_grow;
    /// Flex shrink factor (0 = don't shrink, default)
    float flex_shrink;
    /// Flex basis in logical pixels (-1 = auto, meaning use preferred size; default)
    float flex_basis;
    /// Per-item cross-axis alignment override (Auto = use container's align-items)
    FlexboxLayoutAlignSelf flex_align_self;
    /// Visual ordering of flex items (lower values appear first, default 0)
    int32_t flex_order;
};

/// The FlexboxLayoutData is used for a flex layout.
struct FlexboxLayoutData {
    float width;
    float height;
    float spacing_h;
    float spacing_v;
    Padding padding_h;
    Padding padding_v;
    LayoutAlignment alignment;
    FlexboxLayoutDirection direction;
    FlexboxLayoutAlignContent align_content;
    FlexboxLayoutAlignItems align_items;
    FlexboxLayoutWrap flex_wrap;
    /// Horizontal constraints (width) for each cell
    Slice<FlexboxLayoutItemInfo> cells_h;
    /// Vertical constraints (height) for each cell
    Slice<FlexboxLayoutItemInfo> cells_v;
};

/// Interface for native menu and menubar
struct MenuVTable {
    /// Return the list of items for the sub menu (or the main menu of parent is None)
    void (*sub_menu)(VRef<MenuVTable>, const MenuEntry*, SharedVector<MenuEntry>*);
    /// Handler when the menu entry is activated
    void (*activate)(VRef<MenuVTable>, const MenuEntry*);
    /// drop_in_place handler
    Layout (*drop_in_place)(VRefMut<MenuVTable>);
    /// dealloc handler
    void (*dealloc)(const MenuVTable*, uint8_t *ptr, Layout layout);
};

/// C++ callback table for [`RepeaterInstanceOps`], including the opaque
/// user_data pointer that is passed to each callback.
struct RepeaterInstanceOpsVTable {
    void *user_data;
    uintptr_t (*len)(void *user_data);
    void (*splice)(void *user_data, uintptr_t position, uintptr_t remove, uintptr_t add);
    bool (*ensure_updated)(void *user_data, uintptr_t instance_idx, uintptr_t row);
    /// Height of instance, or NaN if not yet created.
    float (*height)(void *user_data, uintptr_t instance_idx);
    float (*listview_layout)(void *user_data, uintptr_t instance_idx, float *y);
    void (*init)(void *user_data, uintptr_t instance_idx);
};

/// Persistent layout state for a ListView repeater.
struct RepeaterLayoutState {
    /// The model row index of the first instance in the collection.
    uintptr_t offset;
    /// The average visible item height (cached between frames).
    float cached_item_height;
    /// The viewport_y value from the previous layout pass.
    /// It is used to detect if we are scrolling up or down
    float previous_viewport_y;
    /// The y position of the item at `offset`.
    float anchor_y;
};

/// The representation of an easing curve, for animations
struct EasingCurve {
    enum class Tag : uint32_t {
        /// The linear curve
        Linear,
        /// A Cubic bezier curve, with its 4 parameters
        CubicBezier,
        /// Easing curve as defined at: <https://easings.net/#easeInElastic>
        EaseInElastic,
        /// Easing curve as defined at: <https://easings.net/#easeOutElastic>
        EaseOutElastic,
        /// Easing curve as defined at: <https://easings.net/#easeInOutElastic>
        EaseInOutElastic,
        /// Easing curve as defined at: <https://easings.net/#easeInBounce>
        EaseInBounce,
        /// Easing curve as defined at: <https://easings.net/#easeOutBounce>
        EaseOutBounce,
        /// Easing curve as defined at: <https://easings.net/#easeInOutBounce>
        EaseInOutBounce,
    };

    struct CubicBezier_Body {
        float _0[4];
    };

    Tag tag;
    union {
        CubicBezier_Body cubic_bezier;
    };
    constexpr EasingCurve(EasingCurve::Tag tag = Tag::Linear, float a = 0, float b = 0, float c = 1, float d = 1) : tag(tag), cubic_bezier{{a,b,c,d}} {}
};

/// The implementation of the `PropertyAnimation` element
/// This animation has the time as animation limit
struct PropertyAnimation {
    int32_t delay;
    /// duration in millisecond
    int32_t duration;
    float iteration_count;
    AnimationDirection direction;
    EasingCurve easing;
};

/// 2D Size in integer coordinates
using IntSize = Size2D<uint32_t>;

/// A event that describes user input or windowing system events.
///
/// Slint backends typically receive events from the windowing system, translate them to this
/// enum and deliver them to the scene of items via [`slint::Window::try_dispatch_event()`](`crate::api::Window::try_dispatch_event()`).
///
/// The pointer variants describe events originating from an input device such as a mouse
/// or a contact point on a touch-enabled surface.
///
/// All position fields are in logical window coordinates.
union WindowEvent {
    enum class Tag : uint32_t {
        /// A pointer was pressed.
        PointerPressed,
        /// A pointer was released.
        PointerReleased,
        /// The position of the pointer has changed.
        PointerMoved,
        /// The wheel button of a mouse was rotated to initiate scrolling.
        PointerScrolled,
        /// The pointer exited the window.
        PointerExited,
        /// A key was pressed.
        KeyPressed,
        /// A key press was auto-repeated.
        KeyPressRepeated,
        /// A key was released.
        KeyReleased,
        /// The window's scale factor has changed. This can happen for example when the display's resolution
        /// changes, the user selects a new scale factor in the system settings, or the window is moved to a
        /// different screen.
        /// Platform implementations should dispatch this event also right after the initial window creation,
        /// to set the initial scale factor the windowing system provided for the window.
        ScaleFactorChanged,
        /// The window was resized.
        ///
        /// The backend must send this event to ensure that the `width` and `height` property of the root Window
        /// element are properly set.
        Resized,
        /// The user requested to close the window.
        ///
        /// The backend should send this event when the user tries to close the window,for example by pressing the close button.
        ///
        /// This will have the effect of invoking the callback set in [`Window::on_close_requested()`](`crate::api::Window::on_close_requested()`)
        /// and then hiding the window depending on the return value of the callback.
        CloseRequested,
        /// The Window was activated or de-activated.
        ///
        /// The backend should dispatch this event with true when the window gains focus
        /// and false when the window loses focus.
        WindowActiveChanged,
    };

    struct PointerPressed_Body {
        Tag tag;
        LogicalPosition position;
        /// The button that was pressed.
        PointerEventButton button;
    };

    struct PointerReleased_Body {
        Tag tag;
        LogicalPosition position;
        /// The button that was released.
        PointerEventButton button;
    };

    struct PointerMoved_Body {
        Tag tag;
        LogicalPosition position;
    };

    struct PointerScrolled_Body {
        Tag tag;
        LogicalPosition position;
        /// The amount of logical pixels to scroll in the horizontal direction.
        float delta_x;
        /// The amount of logical pixels to scroll in the vertical direction.
        float delta_y;
    };

    struct KeyPressed_Body {
        Tag tag;
        /// The unicode representation of the key pressed.
        ///
        /// # Example
        /// A specific key can be mapped to a unicode by using the [`Key`] enum
        /// ```rust
        /// let _ = slint::platform::WindowEvent::KeyPressed { text: slint::platform::Key::Shift.into() };
        /// ```
        SharedString text;
    };

    struct KeyPressRepeated_Body {
        Tag tag;
        /// The unicode representation of the key pressed.
        ///
        /// # Example
        /// A specific key can be mapped to a unicode by using the [`Key`] enum
        /// ```rust
        /// let _ = slint::platform::WindowEvent::KeyPressRepeated { text: slint::platform::Key::Shift.into() };
        /// ```
        SharedString text;
    };

    struct KeyReleased_Body {
        Tag tag;
        /// The unicode representation of the key released.
        ///
        /// # Example
        /// A specific key can be mapped to a unicode by using the [`Key`] enum
        /// ```rust
        /// let _ = slint::platform::WindowEvent::KeyReleased { text: slint::platform::Key::Shift.into() };
        /// ```
        SharedString text;
    };

    struct ScaleFactorChanged_Body {
        Tag tag;
        /// The window system provided scale factor to map logical pixels to physical pixels.
        float scale_factor;
    };

    struct Resized_Body {
        Tag tag;
        /// The new logical size of the window
        LogicalSize size;
    };

    struct WindowActiveChanged_Body {
        Tag tag;
        bool _0;
    };

    struct {
        Tag tag;
    };
    PointerPressed_Body pointer_pressed;
    PointerReleased_Body pointer_released;
    PointerMoved_Body pointer_moved;
    PointerScrolled_Body pointer_scrolled;
    KeyPressed_Body key_pressed;
    KeyPressRepeated_Body key_press_repeated;
    KeyReleased_Body key_released;
    ScaleFactorChanged_Body scale_factor_changed;
    Resized_Body resized;
    WindowActiveChanged_Body window_active_changed;
/* Some members of the WindowEvent enum have destructors (with SharedString), but thankfully we don't use these so we can have an empty constructor */
    ~WindowEvent() {}
};

/// Expand Rect so that cbindgen can see it. ( is in fact euclid::default::Rect<f32>)
struct Rect {
    float x;
    float y;
    float width;
    float height;
};

/// An entry in the character map of a [`BitmapFont`].
struct CharacterMapEntry {
    /// The unicode code point for a given glyph
    uint32_t code_point;
    /// The corresponding index in the `glyph_data` of [`BitmapGlyphs`]
    uint16_t glyph_index;
};

/// A pre-rendered glyph with the alpha map and associated metrics
struct BitmapGlyph {
    /// The starting x-coordinate for the glyph, relative to the base line
    /// This is a fixed point number that is shifted by 6 bits
    int16_t x;
    /// The starting y-coordinate for the glyph, relative to the base line
    /// This is a fixed point number that is shifted by 6 bits
    int16_t y;
    /// The width of the glyph in pixels
    int16_t width;
    /// The height of the glyph in pixels
    int16_t height;
    /// The horizontal distance to the next glyph
    /// This is a fixed point number that is shifted by 6 bits
    int16_t x_advance;
    /// The 8-bit alpha map that's to be blended with the current text color
    /// or 8-bit signed distance field depending on `BitmapFont::sdf`
    Slice<uint8_t> data;
};

/// A set of pre-rendered bitmap glyphs at a fixed pixel size
struct BitmapGlyphs {
    /// The font size in pixels at which the glyphs were pre-rendered. The boundaries of glyphs may exceed this
    /// size, if the font designer has chosen so. This is only used for matching.
    int16_t pixel_size;
    /// The data of the pre-rendered glyphs
    Slice<BitmapGlyph> glyph_data;
};

/// A subset of an originally scalable font that's rendered ahead of time.
struct BitmapFont {
    /// The family name of the font
    Slice<uint8_t> family_name;
    /// A vector of code points and their corresponding glyph index, sorted by code point.
    Slice<CharacterMapEntry> character_map;
    /// The font supplied size of the em square.
    float units_per_em;
    /// The font ascent in design metrics (typically positive)
    float ascent;
    /// The font descent in design metrics (typically negative)
    float descent;
    /// The font's x-height.
    float x_height;
    /// The font's cap-height.
    float cap_height;
    /// A vector of pre-rendered glyph sets. Each glyph set must have the same number of glyphs,
    /// which must be at least as big as the largest glyph index in the character map.
    Slice<BitmapGlyphs> glyphs;
    /// The weight of the font in CSS units (400 is normal).
    uint16_t weight;
    /// Whether the type-face is rendered italic.
    bool italic;
    /// Whether the format of the font is a signed distance field
    bool sdf;
};

/// The implementation of an empty items that does nothing
struct Empty {
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Rectangle` element
struct Rectangle {
    Property<Brush> background;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `BasicBorderRectangle` element
struct BasicBorderRectangle {
    Property<Brush> background;
    Property<LogicalLength> border_width;
    Property<LogicalLength> border_radius;
    Property<Brush> border_color;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `BorderRectangle` element
struct BorderRectangle {
    Property<Brush> background;
    Property<LogicalLength> border_width;
    Property<LogicalLength> border_radius;
    Property<LogicalLength> border_top_left_radius;
    Property<LogicalLength> border_top_right_radius;
    Property<LogicalLength> border_bottom_left_radius;
    Property<LogicalLength> border_bottom_right_radius;
    Property<Brush> border_color;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `DragArea` element
struct DragArea {
    Property<bool> enabled;
    Property<SharedString> mime_type;
    Property<SharedString> data;
    bool pressed;
    LogicalPoint pressed_position;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `DropArea` element
struct DropArea {
    Property<bool> enabled;
    Property<bool> contains_drag;
    private_api::CallbackHelper<DropEvent, bool> can_drop;
    private_api::CallbackHelper<DropEvent> dropped;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Image` element
struct ImageItem {
    Property<Image> source;
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<ImageFit> image_fit;
    Property<ImageRendering> image_rendering;
    Property<Brush> colorize;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `ClippedImage` element
struct ClippedImage {
    Property<Image> source;
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<ImageFit> image_fit;
    Property<ImageRendering> image_rendering;
    Property<Brush> colorize;
    Property<int32_t> source_clip_x;
    Property<int32_t> source_clip_y;
    Property<int32_t> source_clip_width;
    Property<int32_t> source_clip_height;
    Property<ImageHorizontalAlignment> horizontal_alignment;
    Property<ImageVerticalAlignment> vertical_alignment;
    Property<ImageTiling> horizontal_tiling;
    Property<ImageTiling> vertical_tiling;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `TouchArea` element
struct TouchArea {
    Property<bool> enabled;
    /// FIXME: We should annotate this as an "output" property.
    Property<bool> pressed;
    Property<bool> has_hover;
    /// FIXME: there should be just one property for the point instead of two.
    /// Could even be merged with pressed in a `Property<Option<Point>>` (of course, in the
    /// implementation item only, for the compiler it would stay separate properties)
    Property<LogicalLength> pressed_x;
    Property<LogicalLength> pressed_y;
    /// FIXME: should maybe be as parameter to the mouse event instead. Or at least just one property
    Property<LogicalLength> mouse_x;
    Property<LogicalLength> mouse_y;
    Property<MouseCursor> mouse_cursor;
    private_api::CallbackHelper<void> clicked;
    private_api::CallbackHelper<void> double_clicked;
    private_api::CallbackHelper<void> moved;
    private_api::CallbackHelper<PointerEvent> pointer_event;
    private_api::CallbackHelper<PointerScrollEvent, EventResult> scroll_event;
    /// FIXME: remove this
    CachedRenderingData cached_rendering_data;
    /// true when we are currently grabbing the mouse
    bool grabbed;
};

/// A runtime item that exposes key events
struct FocusScope {
    Property<bool> enabled;
    Property<bool> has_focus;
    Property<bool> focus_on_click;
    Property<bool> focus_on_tab_navigation;
    private_api::CallbackHelper<KeyEvent, EventResult> key_pressed;
    private_api::CallbackHelper<KeyEvent, EventResult> key_released;
    private_api::CallbackHelper<KeyEvent, EventResult> capture_key_pressed;
    private_api::CallbackHelper<KeyEvent, EventResult> capture_key_released;
    private_api::CallbackHelper<FocusReason> focus_changed_event;
    private_api::CallbackHelper<FocusReason> focus_gained;
    private_api::CallbackHelper<FocusReason> focus_lost;
    MaybeKeyBindingList key_bindings;
    /// FIXME: remove this
    CachedRenderingData cached_rendering_data;
    inline FocusScope(); inline ~FocusScope();
};

struct KeyBinding {
    Property<Keys> keys;
    Property<bool> enabled;
    private_api::CallbackHelper<void> activated;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `ScaleRotateGestureHandler` element.
///
/// Provides an API surface for platform-recognized pinch gesture events.
/// Receives `MouseEvent::PinchGesture` events via the normal mouse event
/// tree-walk and exposes cumulative scale, center position, and lifecycle callbacks.
struct ScaleRotateGestureHandler {
    Property<bool> enabled;
    Property<bool> active;
    /// Cumulative scale factor relative to gesture start. Always 1.0 when the
    /// gesture starts, then updated as the gesture progresses (e.g., 2.0 means
    /// doubled, 0.5 means halved).
    Property<float> scale;
    /// Cumulative rotation in degrees relative to gesture start. Always 0.0 when
    /// the gesture starts.
    Property<float> rotation;
    Property<LogicalPosition> center;
    private_api::CallbackHelper<void> started;
    private_api::CallbackHelper<void> updated;
    private_api::CallbackHelper<void> ended;
    private_api::CallbackHelper<void> cancelled;
    /// FIXME: remove this
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Flickable` element
struct Flickable {
    Property<LogicalLength> viewport_x;
    Property<LogicalLength> viewport_y;
    Property<LogicalLength> viewport_width;
    Property<LogicalLength> viewport_height;
    Property<bool> interactive;
    private_api::CallbackHelper<void> flicked;
    FlickableDataBox data;
    /// FIXME: remove this
    CachedRenderingData cached_rendering_data;
    inline Flickable(); inline ~Flickable();
};

/// The implementation of the `Text` element
struct SimpleText {
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<SharedString> text;
    Property<LogicalLength> font_size;
    Property<int32_t> font_weight;
    Property<Brush> color;
    Property<TextHorizontalAlignment> horizontal_alignment;
    Property<TextVerticalAlignment> vertical_alignment;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Text` element
struct StyledTextItem {
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<StyledText> text;
    Property<Brush> default_color;
    Property<LogicalLength> default_font_size;
    Property<SharedString> default_font_family;
    Property<TextHorizontalAlignment> horizontal_alignment;
    Property<TextVerticalAlignment> vertical_alignment;
    private_api::CallbackHelper<slint::SharedString> link_clicked;
    Property<Color> link_color;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Text` element
struct ComplexText {
    Property<LogicalLength> width;
    Property<LogicalLength> height;
    Property<SharedString> text;
    Property<LogicalLength> font_size;
    Property<int32_t> font_weight;
    Property<Brush> color;
    Property<TextHorizontalAlignment> horizontal_alignment;
    Property<TextVerticalAlignment> vertical_alignment;
    Property<SharedString> font_family;
    Property<bool> font_italic;
    Property<TextWrap> wrap;
    Property<TextOverflow> overflow;
    Property<LogicalLength> letter_spacing;
    Property<Brush> stroke;
    Property<LogicalLength> stroke_width;
    Property<TextStrokeStyle> stroke_style;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Path` element
struct Path {
    Property<PathData> elements;
    Property<Brush> fill;
    Property<FillRule> fill_rule;
    Property<Brush> stroke;
    Property<LogicalLength> stroke_width;
    Property<LineCap> stroke_line_cap;
    Property<LineJoin> stroke_line_join;
    Property<float> stroke_miter_limit;
    Property<float> viewbox_x;
    Property<float> viewbox_y;
    Property<float> viewbox_width;
    Property<float> viewbox_height;
    Property<ImageFit> fit;
    Property<bool> clip;
    Property<bool> anti_alias;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Clip` element
struct Clip {
    Property<LogicalLength> border_top_left_radius;
    Property<LogicalLength> border_top_right_radius;
    Property<LogicalLength> border_bottom_left_radius;
    Property<LogicalLength> border_bottom_right_radius;
    Property<LogicalLength> border_width;
    CachedRenderingData cached_rendering_data;
    Property<bool> clip;
};

/// The implementation of the `BoxShadow` element
struct BoxShadow {
    Property<LogicalLength> border_radius;
    Property<LogicalLength> offset_x;
    Property<LogicalLength> offset_y;
    Property<Color> color;
    Property<LogicalLength> blur;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of the `Transform` item.
/// This item is generated by the compiler  as soon as any transform property is used on any element.
struct Transform {
    Property<float> transform_rotation;
    Property<float> transform_scale_x;
    Property<float> transform_scale_y;
    Property<LogicalPosition> transform_origin;
    CachedRenderingData cached_rendering_data;
};

/// The Opacity Item is not meant to be used directly by the .slint code, instead, the `opacity: xxx` or `visible: false` should be used
struct Opacity {
    Property<float> opacity;
    CachedRenderingData cached_rendering_data;
};

/// The Layer Item is not meant to be used directly by the .slint code, instead, the `layer: xxx` property should be used
struct Layer {
    Property<bool> cache_rendering_hint;
    CachedRenderingData cached_rendering_data;
};

/// The implementation of an MenuItem items that does nothing
struct MenuItem {
    CachedRenderingData cached_rendering_data;
    Property<SharedString> title;
    private_api::CallbackHelper<void> activated;
    Property<bool> enabled;
    Property<bool> checkable;
    Property<bool> checked;
    Property<Image> icon;
    Property<Keys> shortcut;
};

extern "C" {

/// Initialize the callback.
/// slint_callback_drop must be called.
void slint_callback_init(CallbackOpaque *out);

/// Emit the callback
void slint_callback_call(const CallbackOpaque *sig, const void *arg, void *ret);

/// Set callback handler.
///
/// The binding has signature fn(user_data)
void slint_callback_set_handler(const CallbackOpaque *sig,
                                void (*binding)(void *user_data, const void *arg, void *ret),
                                void *user_data,
                                void (*drop_user_data)(void*));

/// Destroy callback
void slint_callback_drop(CallbackOpaque *handle);

bool slint_date_time_use_24_hour_format();

int32_t slint_date_time_month_day_count(uint32_t month, int32_t year);

int32_t slint_date_time_month_offset(uint32_t month, int32_t year);

void slint_date_time_format_date(const SharedString *format,
                                 uint32_t day,
                                 uint32_t month,
                                 int32_t year,
                                 SharedString *out);

void slint_date_time_date_now(int32_t *d, int32_t *m, int32_t *y);

bool slint_date_time_parse_date(const SharedString *date,
                                const SharedString *format,
                                int32_t *d,
                                int32_t *m,
                                int32_t *y);

void slint_keys_debug_string(const Keys *shortcut, SharedString *out);

/// Call init() on the ItemVTable of each item in the item array.
void slint_register_item_tree(const ItemTreeRc *item_tree_rc,
                              const WindowAdapterRcOpaque *window_handle);

/// Free the backend graphics resources allocated in the item array.
void slint_unregister_item_tree(ItemTreeRefPin component,
                                Slice<VOffset<uint8_t, ItemVTable, AllowPin>> item_array,
                                const WindowAdapterRcOpaque *window_handle);

/// Expose `crate::item_tree::visit_item_tree` to C++
///
/// Safety: Assume a correct implementation of the item_tree array
VisitChildrenResult slint_visit_item_tree(const ItemTreeRc *item_tree,
                                          Slice<ItemTreeNode> item_tree_array,
                                          intptr_t index,
                                          TraversalOrder order,
                                          VRefMut<ItemVisitorVTable> visitor,
                                          VisitChildrenResult (*visit_dynamic)(const void *base,
                                                                               TraversalOrder order,
                                                                               VRefMut<ItemVisitorVTable> visitor,
                                                                               uint32_t dyn_index));

void slint_windowitem_hide(Pin<const WindowItem*> window_item,
                           const WindowAdapterRcOpaque *window_adapter,
                           const VRc<ItemTreeVTable> *_self_component,
                           uint32_t _self_index);

void slint_contextmenu_close(Pin<const ContextMenu*> s,
                             const WindowAdapterRcOpaque *window_adapter,
                             const VRc<ItemTreeVTable> *self_component,
                             uint32_t self_index);

bool slint_contextmenu_is_open(Pin<const ContextMenu*> s,
                               const WindowAdapterRcOpaque *window_adapter,
                               const VRc<ItemTreeVTable> *self_component,
                               uint32_t self_index);

LogicalPoint slint_item_absolute_position(const VRc<ItemTreeVTable> *self_component,
                                          uint32_t self_index);

/// # Safety
/// This must be called using a non-null pointer pointing to a chunk of memory big enough to
/// hold a FlickableDataBox
void slint_flickable_data_init(FlickableDataBox *data);

/// # Safety
/// This must be called using a non-null pointer pointing to an initialized FlickableDataBox
void slint_flickable_data_free(FlickableDataBox *data);

void slint_textinput_set_selection_offsets(Pin<const TextInput*> text_input,
                                           const WindowAdapterRcOpaque *window_adapter,
                                           const VRc<ItemTreeVTable> *self_component,
                                           uint32_t self_index,
                                           int32_t start,
                                           int32_t end);

void slint_textinput_select_all(Pin<const TextInput*> text_input,
                                const WindowAdapterRcOpaque *window_adapter,
                                const VRc<ItemTreeVTable> *self_component,
                                uint32_t self_index);

void slint_textinput_clear_selection(Pin<const TextInput*> text_input,
                                     const WindowAdapterRcOpaque *window_adapter,
                                     const VRc<ItemTreeVTable> *self_component,
                                     uint32_t self_index);

void slint_textinput_cut(Pin<const TextInput*> text_input,
                         const WindowAdapterRcOpaque *window_adapter,
                         const VRc<ItemTreeVTable> *self_component,
                         uint32_t self_index);

void slint_textinput_copy(Pin<const TextInput*> text_input,
                          const WindowAdapterRcOpaque *window_adapter,
                          const VRc<ItemTreeVTable> *self_component,
                          uint32_t self_index);

void slint_textinput_paste(Pin<const TextInput*> text_input,
                           const WindowAdapterRcOpaque *window_adapter,
                           const VRc<ItemTreeVTable> *self_component,
                           uint32_t self_index);

FontMetrics slint_cpp_text_item_fontmetrics(const WindowAdapterRcOpaque *window_adapter,
                                            const VRc<ItemTreeVTable> *self_component,
                                            uint32_t self_index);

void slint_swipegesturehandler_cancel(Pin<const SwipeGestureHandler*> s,
                                      const WindowAdapterRcOpaque *window_adapter,
                                      const VRc<ItemTreeVTable> *self_component,
                                      uint32_t self_index);

/// # Safety
/// This must be called using a non-null pointer pointing to a chunk of memory big enough to
/// hold a MaybeKeybindingList
void slint_maybe_key_binding_list_init(MaybeKeyBindingList *list);

/// # Safety
/// This must be called using a non-null pointer pointing to an initialized MaybeKeybindingList
void slint_maybe_key_binding_list_free(MaybeKeyBindingList *list);

void slint_organize_grid_layout(Slice<GridLayoutInputData> input_data,
                                Slice<uint32_t> repeater_indices,
                                Slice<uint32_t> repeater_steps,
                                GridLayoutOrganizedData *result);

void slint_organize_dialog_button_layout(Slice<GridLayoutInputData> input_data,
                                         Slice<DialogButtonRole> dialog_button_roles,
                                         GridLayoutOrganizedData *result);

void slint_solve_grid_layout(const GridLayoutData *data,
                             Slice<LayoutItemInfo> constraints,
                             Orientation orientation,
                             Slice<uint32_t> repeater_indices,
                             Slice<uint32_t> repeater_steps,
                             SharedVector<float> *result);

LayoutInfo slint_grid_layout_info(const GridLayoutOrganizedData *organized_data,
                                  Slice<LayoutItemInfo> constraints,
                                  Slice<uint32_t> repeater_indices,
                                  Slice<uint32_t> repeater_steps,
                                  float spacing,
                                  const Padding *padding,
                                  Orientation orientation);

void slint_solve_box_layout(const BoxLayoutData *data,
                            Slice<uint32_t> repeater_indices,
                            SharedVector<float> *result);

/// Return the LayoutInfo for a BoxLayout with the given cells.
LayoutInfo slint_box_layout_info(Slice<LayoutItemInfo> cells,
                                 float spacing,
                                 const Padding *padding,
                                 LayoutAlignment alignment);

/// Return the LayoutInfo for a BoxLayout with the given cells.
LayoutInfo slint_box_layout_info_ortho(Slice<LayoutItemInfo> cells, const Padding *padding);

void slint_solve_flexbox_layout(const FlexboxLayoutData *data,
                                Slice<uint32_t> repeater_indices,
                                SharedVector<float> *result,
                                const void *measure_fn,
                                void *measure_user_data);

/// Return main-axis LayoutInfo for a FlexboxLayout (single-axis, no cross-axis dependency).
LayoutInfo slint_flexbox_layout_info_main_axis(Slice<FlexboxLayoutItemInfo> cells,
                                               float spacing,
                                               const Padding *padding,
                                               FlexboxLayoutWrap flex_wrap);

/// Return cross-axis LayoutInfo for a FlexboxLayout.
LayoutInfo slint_flexbox_layout_info_cross_axis(Slice<FlexboxLayoutItemInfo> cells_h,
                                                Slice<FlexboxLayoutItemInfo> cells_v,
                                                float spacing_h,
                                                float spacing_v,
                                                const Padding *padding_h,
                                                const Padding *padding_v,
                                                FlexboxLayoutDirection direction,
                                                FlexboxLayoutWrap flex_wrap,
                                                float constraint_size);

/// Create a `VRc::<MenuVTable>`` that wraps the [`ItemTreeRc`]
///
/// Put the created VRc into the result pointer with std::ptr::write
void slint_menus_create_wrapper(const ItemTreeRc *menu_tree,
                                VRc<MenuVTable> *result,
                                bool (*condition)(const ItemTreeRc *menu_tree));

void slint_repeater_ensure_updated(RepeaterInstanceOpsVTable *ops,
                                   uintptr_t offset,
                                   uintptr_t count);

void slint_repeater_ensure_updated_listview(RepeaterInstanceOpsVTable *ops,
                                            RepeaterLayoutState *state,
                                            uintptr_t row_count,
                                            Pin<const Property<LogicalLength>*> viewport_width,
                                            Pin<const Property<LogicalLength>*> viewport_height,
                                            Pin<const Property<LogicalLength>*> viewport_y,
                                            LogicalLength listview_width,
                                            LogicalLength listview_height);

/// Initialize the first pointer of the Property. Does not initialize the content.
/// `out` is assumed to be uninitialized
void slint_property_init(PropertyHandleOpaque *out);

/// To be called before accessing the value
void slint_property_update(const PropertyHandleOpaque *handle, void *val);

/// Mark the fact that the property was changed and that its binding need to be removed, and
/// the dependencies marked dirty.
/// To be called after the `value` has been changed
void slint_property_set_changed(const PropertyHandleOpaque *handle, const void *value);

/// Set a binding
///
/// The current implementation will do usually two memory allocation:
///  1. the allocation from the calling code to allocate user_data
///  2. the box allocation within this binding
///
/// It might be possible to reduce that by passing something with a
/// vtable, so there is the need for less memory allocation.
void slint_property_set_binding(const PropertyHandleOpaque *handle,
                                void (*binding)(void *user_data, void *pointer_to_value),
                                void *user_data,
                                void (*drop_user_data)(void*),
                                bool (*intercept_set)(void *user_data, const void *pointer_to_value),
                                bool (*intercept_set_binding)(void *user_data, void *new_binding));

/// Set a binding using an already allocated building holder
///
/// (take ownership of the binding)
void slint_property_set_binding_internal(const PropertyHandleOpaque *handle, void *binding);

/// Delete a binding. The pointer must be a pointer to a binding (so a BindingHolder)
void slint_property_delete_binding(void *binding);

/// Evaluate a raw binding
void slint_property_evaluate_binding(void *binding, void *value);

/// Returns whether the property behind this handle is marked as dirty
bool slint_property_is_dirty(const PropertyHandleOpaque *handle);

/// Marks the property as dirty and notifies dependencies.
void slint_property_mark_dirty(const PropertyHandleOpaque *handle);

/// Marks the property as dirty and notifies dependencies.
void slint_property_set_constant(const PropertyHandleOpaque *handle);

/// Destroy handle
void slint_property_drop(PropertyHandleOpaque *handle);

/// Internal function to set up a property animation to the specified target value for an integer property.
void slint_property_set_animated_value_int(const PropertyHandleOpaque *handle,
                                           int32_t from,
                                           int32_t to,
                                           const PropertyAnimation *animation_data);

/// Internal function to set up a property animation to the specified target value for a float property.
void slint_property_set_animated_value_float(const PropertyHandleOpaque *handle,
                                             float from,
                                             float to,
                                             const PropertyAnimation *animation_data);

/// Internal function to set up a property animation to the specified target value for a color property.
void slint_property_set_animated_value_color(const PropertyHandleOpaque *handle,
                                             Color from,
                                             Color to,
                                             const PropertyAnimation *animation_data);

/// Internal function to set up a property animation to the specified target value for a brush property.
void slint_property_set_animated_value_brush(const PropertyHandleOpaque *handle,
                                             const Brush *from,
                                             const Brush *to,
                                             const PropertyAnimation *animation_data);

/// Internal function to set up a property animation between values produced by the specified binding for an integer property.
void slint_property_set_animated_binding_int(const PropertyHandleOpaque *handle,
                                             void (*binding)(void*, int*),
                                             void *user_data,
                                             void (*drop_user_data)(void*),
                                             PropertyAnimation (*transition_data)(void *user_data,
                                                                                  uint64_t **start_instant));

/// Internal function to set up a property animation between values produced by the specified binding for a float property.
void slint_property_set_animated_binding_float(const PropertyHandleOpaque *handle,
                                               void (*binding)(void*, float*),
                                               void *user_data,
                                               void (*drop_user_data)(void*),
                                               PropertyAnimation (*transition_data)(void *user_data,
                                                                                    uint64_t **start_instant));

/// Internal function to set up a property animation between values produced by the specified binding for a color property.
void slint_property_set_animated_binding_color(const PropertyHandleOpaque *handle,
                                               void (*binding)(void*, Color*),
                                               void *user_data,
                                               void (*drop_user_data)(void*),
                                               PropertyAnimation (*transition_data)(void *user_data,
                                                                                    uint64_t **start_instant));

/// Internal function to set up a property animation between values produced by the specified binding for a brush property.
void slint_property_set_animated_binding_brush(const PropertyHandleOpaque *handle,
                                               void (*binding)(void*, Brush*),
                                               void *user_data,
                                               void (*drop_user_data)(void*),
                                               PropertyAnimation (*transition_data)(void *user_data,
                                                                                    uint64_t **start_instant));

/// Internal function to set up a state binding on a Property<StateInfo>.
void slint_property_set_state_binding(const PropertyHandleOpaque *handle,
                                      int32_t (*binding)(void*),
                                      void *user_data,
                                      void (*drop_user_data)(void*));

/// Initialize the first pointer of the PropertyTracker.
/// `out` is assumed to be uninitialized
/// slint_property_tracker_drop need to be called after that
void slint_property_tracker_init(PropertyTrackerOpaque *out);

/// Call the callback with the user data. Any properties access within the callback will be registered.
/// Any currently evaluated bindings or property trackers will be notified if accessed properties are changed.
void slint_property_tracker_evaluate(const PropertyTrackerOpaque *handle,
                                     void (*callback)(void *user_data),
                                     void *user_data);

/// Call the callback with the user data. Any properties access within the callback will be registered.
/// Any currently evaluated bindings or property trackers will be not notified if accessed properties are changed.
void slint_property_tracker_evaluate_as_dependency_root(const PropertyTrackerOpaque *handle,
                                                        void (*callback)(void *user_data),
                                                        void *user_data);

/// Query if the property tracker is dirty
bool slint_property_tracker_is_dirty(const PropertyTrackerOpaque *handle);

/// Destroy handle
void slint_property_tracker_drop(PropertyTrackerOpaque *handle);

/// Construct a ChangeTracker
void slint_change_tracker_construct(ChangeTracker *ct);

/// Drop a ChangeTracker
void slint_change_tracker_drop(ChangeTracker *ct);

/// initialize the change tracker
void slint_change_tracker_init(const ChangeTracker *ct,
                               void *user_data,
                               void (*drop_user_data)(void *user_data),
                               bool (*eval_fn)(void *user_data),
                               void (*notify_fn)(void *user_data));

/// return the current animation tick for the `animation-tick` function
uint64_t slint_animation_tick();

/// This function is used for the low-level C++ interface to allocate the backing vector of a SharedVector.
uint8_t *slint_shared_vector_allocate(uintptr_t size,
                                      uintptr_t align);

/// This function is used for the low-level C++ interface to deallocate the backing vector of a SharedVector
void slint_shared_vector_free(uint8_t *ptr,
                              uintptr_t size,
                              uintptr_t align);

/// This function is used for the low-level C++ interface to initialize the empty SharedVector.
const uint8_t *slint_shared_vector_empty();

/// Returns a nul-terminated pointer for this string.
/// The returned value is owned by the string, and should not be used after any
/// mutable function have been called on the string, and must not be freed.
const char *slint_shared_string_bytes(const SharedString *ss);

/// Destroy the shared string
void slint_shared_string_drop(const SharedString *ss);

/// Increment the reference count of the string.
/// The resulting structure must be passed to slint_shared_string_drop
void slint_shared_string_clone(SharedString *out, const SharedString *ss);

/// Safety: bytes must be a valid utf-8 string of size len without null inside.
/// The resulting structure must be passed to slint_shared_string_drop
void slint_shared_string_from_bytes(SharedString *out, const char *bytes, uintptr_t len);

/// Create a string from a number.
/// The resulting structure must be passed to slint_shared_string_drop
void slint_shared_string_from_number(SharedString *out, double n);

void slint_shared_string_from_number_fixed(SharedString *out, double n, uintptr_t digits);

void slint_shared_string_from_number_precision(SharedString *out, double n, uintptr_t precision);

/// Append some bytes to an existing shared string
///
/// bytes must be a valid utf8 array of size `len`, without null bytes inside
void slint_shared_string_append(SharedString *self_, const char *bytes, uintptr_t len);

void slint_shared_string_to_lowercase(SharedString *out, const SharedString *ss);

void slint_shared_string_to_uppercase(SharedString *out, const SharedString *ss);

/// Create a new default styled text
void slint_styled_text_new(StyledText *out);

/// Destroy the shared string
void slint_styled_text_drop(const StyledText *text);

/// Returns true if \a a is equal to \a b; otherwise returns false.
bool slint_styled_text_eq(const StyledText *a, const StyledText *b);

/// Clone the styled text
void slint_styled_text_clone(StyledText *out, const StyledText *ss);

/// Slint animations do not use real time, but use a mocked time.
/// Normally, the event loop update the time of the animation using
/// real time, but in tests, it is more convenient to use the fake time.
/// This function will add some milliseconds to the fake time
void slint_mock_elapsed_time(uint64_t time_in_ms);

/// Return the current mocked time.
uint64_t slint_get_mocked_time();

/// Simulate a click on a position within the component and releasing after some time.
/// The time until the release is hardcoded to 50ms
void slint_send_mouse_click(float x, float y, const WindowAdapterRc *window_adapter);

/// Simulate a single key event with the given text (pressed or released).
///
/// Unlike [`slint_send_keyboard_char`], this dispatches a single [`WindowEvent`]
/// with the complete text. This is important for multi-codepoint grapheme clusters
/// (e.g. NFD-encoded `é` = `e` + `\u{0301}`).
void slint_send_keyboard_key_text(const SharedString *text,
                                  bool pressed,
                                  const WindowAdapterRc *window_adapter);

/// Simulate a character input event (pressed or released).
///
/// Each character in the string is dispatched as a separate [`WindowEvent`].
/// This is useful for modifier keys where each special character code
/// represents an independent key press.
void slint_send_keyboard_char(const SharedString *string,
                              bool pressed,
                              const WindowAdapterRc *window_adapter);

/// Simulate a character input event.
void send_keyboard_string_sequence(const SharedString *sequence,
                                   const WindowAdapterRc *window_adapter);

/// Returns the interval in milliseconds. 0 when the timer was never started.
uint64_t slint_timer_interval(uintptr_t id);

/// Perform the translation and formatting.
void slint_translate(SharedString *to_translate,
                     const SharedString *context,
                     const SharedString *domain,
                     Slice<SharedString> arguments,
                     int32_t n,
                     const SharedString *plural);

/// Mark all translated string as dirty to perform re-translation in case the language change
void slint_translations_mark_dirty();

/// Safety: The slice must contain valid null-terminated utf-8 strings
void slint_translate_from_bundle(Slice<const char*> strs,
                                 Slice<SharedString> arguments,
                                 SharedString *output);

/// strs is all the strings variant of all languages.
/// indices is the array of indices such that for each language, the corresponding indice is one past the last index of the string for that language.
/// So to get the string array for that language, one would do `strs[indices[lang-1]..indices[lang]]`
/// (where indices[-1] is 0)
///
/// Safety; the strs must be pointer to valid null-terminated utf-8 strings
void slint_translate_from_bundle_with_plural(Slice<const char*> strs,
                                             Slice<uint32_t> indices,
                                             Slice<uintptr_t(*)(int32_t)> plural_rules,
                                             Slice<SharedString> arguments,
                                             int32_t n,
                                             SharedString *output);

void slint_translate_set_bundled_languages(Slice<Slice<uint8_t>> languages);

bool slint_translate_select_bundled_translation(Slice<uint8_t> language);

/// Releases the reference to the windowrc held by handle.
void slint_windowrc_drop(WindowAdapterRcOpaque *handle);

/// Releases the reference to the component window held by handle.
void slint_windowrc_clone(const WindowAdapterRcOpaque *source, WindowAdapterRcOpaque *target);

/// Spins an event loop and renders the items of the provided component in this window.
void slint_windowrc_show(const WindowAdapterRcOpaque *handle);

/// Spins an event loop and renders the items of the provided component in this window.
void slint_windowrc_hide(const WindowAdapterRcOpaque *handle);

/// Returns the visibility state of the window. This function can return false even if you previously called show()
/// on it, for example if the user minimized the window.
bool slint_windowrc_is_visible(const WindowAdapterRcOpaque *handle);

/// Returns the window scale factor.
float slint_windowrc_get_scale_factor(const WindowAdapterRcOpaque *handle);

/// Sets the window scale factor, merely for testing purposes.
void slint_windowrc_set_const_scale_factor(const WindowAdapterRcOpaque *handle, float value);

/// Returns the text-input-focused property value.
bool slint_windowrc_get_text_input_focused(const WindowAdapterRcOpaque *handle);

/// Set the text-input-focused property.
void slint_windowrc_set_text_input_focused(const WindowAdapterRcOpaque *handle, bool value);

/// Sets the focus item.
void slint_windowrc_set_focus_item(const WindowAdapterRcOpaque *handle,
                                   const ItemRc *focus_item,
                                   bool set_focus,
                                   FocusReason reason);

/// Associates the window with the given component.
void slint_windowrc_set_component(const WindowAdapterRcOpaque *handle, const ItemTreeRc *component);

/// Show a popup and return its ID. The returned ID will always be non-zero.
uint32_t slint_windowrc_show_popup(const WindowAdapterRcOpaque *handle,
                                   const ItemTreeRc *popup,
                                   LogicalPosition position,
                                   PopupClosePolicy close_policy,
                                   const ItemRc *parent_item,
                                   bool is_menu);

/// Close the popup by the given ID.
void slint_windowrc_close_popup(const WindowAdapterRcOpaque *handle, uint32_t popup_id);

/// C binding to the set_rendering_notifier() API of Window
bool slint_windowrc_set_rendering_notifier(const WindowAdapterRcOpaque *handle,
                                           void (*callback)(RenderingState rendering_state,
                                                            GraphicsAPI graphics_api,
                                                            void *user_data),
                                           void (*drop_user_data)(void *user_data),
                                           void *user_data,
                                           SetRenderingNotifierError *error);

/// C binding to the on_close_requested() API of Window
void slint_windowrc_on_close_requested(const WindowAdapterRcOpaque *handle,
                                       CloseRequestResponse (*callback)(void *user_data),
                                       void (*drop_user_data)(void *user_data),
                                       void *user_data);

/// This function issues a request to the windowing system to redraw the contents of the window.
void slint_windowrc_request_redraw(const WindowAdapterRcOpaque *handle);

/// Returns the position of the window on the screen, in physical screen coordinates and including
/// a window frame (if present).
void slint_windowrc_position(const WindowAdapterRcOpaque *handle, Point2D<int32_t> *pos);

/// Sets the position of the window on the screen, in physical screen coordinates and including
/// a window frame (if present).
/// Note that on some windowing systems, such as Wayland, this functionality is not available.
void slint_windowrc_set_physical_position(const WindowAdapterRcOpaque *handle,
                                          const Point2D<int32_t> *pos);

/// Sets the position of the window on the screen, in physical screen coordinates and including
/// a window frame (if present).
/// Note that on some windowing systems, such as Wayland, this functionality is not available.
void slint_windowrc_set_logical_position(const WindowAdapterRcOpaque *handle,
                                         const Point2D<float> *pos);

/// Returns the size of the window on the screen, in physical screen coordinates and excluding
/// a window frame (if present).
IntSize slint_windowrc_size(const WindowAdapterRcOpaque *handle);

/// Resizes the window to the specified size on the screen, in physical pixels and excluding
/// a window frame (if present).
void slint_windowrc_set_physical_size(const WindowAdapterRcOpaque *handle, const IntSize *size);

/// Resizes the window to the specified size on the screen, in physical pixels and excluding
/// a window frame (if present).
void slint_windowrc_set_logical_size(const WindowAdapterRcOpaque *handle, const Size *size);

/// Return whether the style is using a dark theme
ColorScheme slint_windowrc_color_scheme(const WindowAdapterRcOpaque *handle);

/// Return the system accent color, or transparent if not available
void slint_windowrc_accent_color(const WindowAdapterRcOpaque *handle, Color *out);

/// Return whether the platform supports native menu bars
bool slint_windowrc_supports_native_menu_bar(const WindowAdapterRcOpaque *handle);

/// Setup the native menu bar
void slint_windowrc_setup_native_menu_bar(const WindowAdapterRcOpaque *handle,
                                          const VRc<MenuVTable> *menu_instance);

void slint_windowrc_setup_menu_bar_shortcuts(const WindowAdapterRcOpaque *handle,
                                             const VRc<MenuVTable> *menu_instance);

/// Show a native context menu
bool slint_windowrc_show_native_popup_menu(const WindowAdapterRcOpaque *handle,
                                           const VRc<MenuVTable> *context_menu,
                                           LogicalPosition position,
                                           const ItemRc *parent_item);

/// Return the default-font-size property of the WindowItem
float slint_windowrc_resolved_default_font_size(const ItemTreeRc *item_tree);

/// Dispatch a key pressed or release event
void slint_windowrc_dispatch_key_event(const WindowAdapterRcOpaque *handle,
                                       KeyEventType event_type,
                                       const SharedString *text,
                                       bool repeat);

/// Dispatch a mouse event
void slint_windowrc_dispatch_pointer_event(const WindowAdapterRcOpaque *handle,
                                           const MouseEvent *event);

/// Dispatch a window event
void slint_windowrc_dispatch_event(const WindowAdapterRcOpaque *handle, const WindowEvent *event);

bool slint_windowrc_is_fullscreen(const WindowAdapterRcOpaque *handle);

bool slint_windowrc_is_minimized(const WindowAdapterRcOpaque *handle);

bool slint_windowrc_is_maximized(const WindowAdapterRcOpaque *handle);

void slint_windowrc_set_fullscreen(const WindowAdapterRcOpaque *handle, bool value);

void slint_windowrc_set_minimized(const WindowAdapterRcOpaque *handle, bool value);

void slint_windowrc_set_maximized(const WindowAdapterRcOpaque *handle, bool value);

/// Takes a snapshot of the window contents and returns it as RGBA8 encoded pixel buffer.
bool slint_windowrc_take_snapshot(const WindowAdapterRcOpaque *handle,
                                  SharedVector<Rgba8Pixel> *data,
                                  uint32_t *width,
                                  uint32_t *height);

/// Returns the `HWND` associated with this window, or null if it doesn't exist or isn't created yet.
void *slint_windowrc_hwnd_win32(const WindowAdapterRcOpaque *handle);

/// Returns the `HINSTANCE` associated with this window, or null if it doesn't exist or isn't created yet.
void *slint_windowrc_hinstance_win32(const WindowAdapterRcOpaque *handle);

/// Returns the `wl_surface` associated with this window, or null if it doesn't exist or isn't created yet.
void *slint_windowrc_wlsurface_wayland(const WindowAdapterRcOpaque *handle);

/// Returns the `wl_display` associated with this window, or null if it doesn't exist or isn't created yet.
void *slint_windowrc_wldisplay_wayland(const WindowAdapterRcOpaque *handle);

/// Returns the `NSView` associated with this window, or null if it doesn't exist or isn't created yet.
void *slint_windowrc_nsview_appkit(const WindowAdapterRcOpaque *handle);

}  // extern "C"

}  // namespace cbindgen_private
}  // namespace slint


namespace slint::private_api {
#define SLINT_DECL_ITEM(ItemName) \
    extern const cbindgen_private::ItemVTable ItemName##VTable; \
    extern SLINT_DLL_IMPORT const cbindgen_private::ItemVTable* slint_get_##ItemName##VTable();

extern "C" {
SLINT_DECL_ITEM(Empty);
SLINT_DECL_ITEM(Rectangle);
SLINT_DECL_ITEM(BasicBorderRectangle);
SLINT_DECL_ITEM(BorderRectangle);
SLINT_DECL_ITEM(DragArea);
SLINT_DECL_ITEM(DropArea);
SLINT_DECL_ITEM(ImageItem);
SLINT_DECL_ITEM(ClippedImage);
SLINT_DECL_ITEM(TouchArea);
SLINT_DECL_ITEM(FocusScope);
SLINT_DECL_ITEM(KeyBinding);
SLINT_DECL_ITEM(SwipeGestureHandler);
SLINT_DECL_ITEM(ScaleRotateGestureHandler);
SLINT_DECL_ITEM(Flickable);
SLINT_DECL_ITEM(SimpleText);
SLINT_DECL_ITEM(StyledTextItem);
SLINT_DECL_ITEM(ComplexText);
SLINT_DECL_ITEM(Path);
SLINT_DECL_ITEM(WindowItem);
SLINT_DECL_ITEM(TextInput);
SLINT_DECL_ITEM(Clip);
SLINT_DECL_ITEM(BoxShadow);
SLINT_DECL_ITEM(Transform);
SLINT_DECL_ITEM(Opacity);
SLINT_DECL_ITEM(Layer);
SLINT_DECL_ITEM(ContextMenu);
SLINT_DECL_ITEM(MenuItem);
}

#undef SLINT_DECL_ITEM
}
