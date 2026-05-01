#pragma once

/* Generated with cbindgen:0.29.2 */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

namespace slint {
namespace cbindgen_private {

/// Describes the kind of layout an element represents.
enum class LayoutKind : uint8_t {
    /// A `HorizontalLayout`.
    HorizontalLayout,
    /// A `VerticalLayout`.
    VerticalLayout,
    /// A `GridLayout`.
    GridLayout,
    /// A flex box layout.
    FlexboxLayout,
};

/// `ElementHandle` wraps an existing element in a Slint UI. An ElementHandle does not keep
/// the corresponding element in the UI alive. Use [`Self::is_valid()`] to verify that
/// it is still alive.
///
/// Obtain instances of `ElementHandle` by querying your application through
/// [`Self::find_by_accessible_label()`].
struct ElementHandle {
    ItemWeak item;
    uintptr_t element_index;
};

extern "C" {

void slint_testing_init_backend();

void slint_testing_configure_test_fonts();

bool slint_testing_element_visit_elements(const ItemTreeRc *root,
                                          void *user_data,
                                          bool (*visitor)(void*, const ElementHandle*));

void slint_testing_element_find_by_accessible_label(const ItemTreeRc *root,
                                                    const Slice<uint8_t> *label,
                                                    SharedVector<ElementHandle> *out);

void slint_testing_element_find_by_element_id(const ItemTreeRc *root,
                                              const Slice<uint8_t> *element_id,
                                              SharedVector<ElementHandle> *out);

void slint_testing_element_find_by_element_type_name(const ItemTreeRc *root,
                                                     const Slice<uint8_t> *type_name,
                                                     SharedVector<ElementHandle> *out);

bool slint_testing_element_id(const ElementHandle *element, SharedString *out);

bool slint_testing_element_type_name(const ElementHandle *element, SharedString *out);

bool slint_testing_element_bases(const ElementHandle *element, SharedVector<SharedString> *out);

bool slint_testing_element_layout_kind(const ElementHandle *element, LayoutKind *out);

}  // extern "C"

}  // namespace cbindgen_private
}  // namespace slint
