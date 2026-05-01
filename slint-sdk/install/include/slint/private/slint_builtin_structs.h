#pragma once
// This file is auto-generated from api\cpp\cbindgen.rs
namespace slint::language {
/// The `KeyboardModifiers` struct provides booleans to indicate possible modifier keys on a keyboard, such as Shift, Control, etc.
/// It is provided as part of `KeyEvent`'s `modifiers` field.
///
/// Keyboard shortcuts on Apple platforms typically use the Command key (⌘), such as Command+C for "Copy". On other platforms
/// the same shortcut is typically represented using Control+C. To make it easier to develop cross-platform applications, on macOS,
/// Slint maps the Command key to the control modifier, and the Control key to the meta modifier.
///
/// On Windows, the Windows key is mapped to the meta modifier.
struct KeyboardModifiers {
    /// Indicates the Alt key on a keyboard.
    bool alt;
    /// Indicates the Control key on a keyboard, except on macOS, where it is the Command key (⌘).
    bool control;
    /// Indicates the Shift key on a keyboard.
    bool shift;
    /// Indicates the Control key on macos, and the Windows key on Windows.
    bool meta;
    /// \private
    friend bool operator==(const KeyboardModifiers&, const KeyboardModifiers&) = default;
    /// \private
    friend bool operator!=(const KeyboardModifiers&, const KeyboardModifiers&) = default;
};
/// This structure is generated and passed to the key press and release callbacks of the `FocusScope` element.
struct KeyEvent {
    /// The unicode representation of the key pressed.
    SharedString text;
    /// The keyboard modifiers active at the time of the key press event.
    KeyboardModifiers modifiers;
    /// This field is set to true for key press events that are repeated,
    /// i.e. the key is held down. It's always false for key release events.
    bool repeat;
    /// \private
    friend bool operator==(const KeyEvent&, const KeyEvent&) = default;
    /// \private
    friend bool operator!=(const KeyEvent&, const KeyEvent&) = default;
};
/// Represents an item in a StandardListView and a StandardTableView.
struct StandardListViewItem {
    /// The text content of the item
    SharedString text;
    /// \private
    friend bool operator==(const StandardListViewItem&, const StandardListViewItem&) = default;
    /// \private
    friend bool operator!=(const StandardListViewItem&, const StandardListViewItem&) = default;
};
}
namespace slint { using slint::language::StandardListViewItem; }
