#pragma once

/* Generated with cbindgen:0.29.2 */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>
#include "private/slint_enums_internal.h"
#include "private/slint_builtin_structs.h"
namespace slint::cbindgen_private::types {
using KeyboardModifiers = ::slint::language::KeyboardModifiers;
}

namespace slint {
namespace cbindgen_private {
namespace types {

/// Internal representation of the `Keys` type.
/// This is semver exempt and is only used to set up the native menu in the backends.
struct KeysInner {
    /// The `key` used to trigger the shortcut
    ///
    /// Note: This is currently converted to lowercase when the shortcut is created!
    SharedString key;
    /// `KeyboardModifier`s that need to be pressed for the shortcut to fire
    KeyboardModifiers modifiers;
    /// Whether to ignore shift state when matching the shortcut
    bool ignore_shift;
    /// Whether to ignore alt state when matching the shortcut
    bool ignore_alt;

    bool operator==(const KeysInner& other) const {
        return key == other.key &&
               modifiers == other.modifiers &&
               ignore_shift == other.ignore_shift &&
               ignore_alt == other.ignore_alt;
    }
    bool operator!=(const KeysInner& other) const {
        return key != other.key ||
               modifiers != other.modifiers ||
               ignore_shift != other.ignore_shift ||
               ignore_alt != other.ignore_alt;
    }
};

/// The `Keys` type is the Rust representation of Slint's `keys` primitive type.
///
/// It can be created with the `@keys` macro in Slint and defines which key event(s) activate a KeyBinding.
///
/// See also the Slint documentation on [Key Bindings](slint:KeyBindingOverview).
struct Keys {
    KeysInner inner;

    bool operator==(const Keys& other) const {
        return inner == other.inner;
    }
    bool operator!=(const Keys& other) const {
        return inner != other.inner;
    }
};

extern "C" {

void slint_keys(const SharedString *key,
                bool alt,
                bool control,
                bool shift,
                bool meta,
                bool ignore_shift,
                bool ignore_alt,
                Keys *out);

void slint_keys_to_string(const Keys *shortcut, SharedString *out);

}  // extern "C"

}  // namespace types
}  // namespace cbindgen_private
}  // namespace slint
