#pragma once

/* Generated with cbindgen:0.29.2 */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>
#include "private/slint_image_internal.h"
#include "private/slint_internal.h"

namespace slint::platform { struct Rgb565Pixel; }
namespace slint::cbindgen_private {
    struct WindowProperties; using slint::platform::Rgb565Pixel;
    using slint::cbindgen_private::types::TexturePixelFormat;
    struct DrawTextureArgs;
    struct DrawRectangleArgs;
    using types::PhysicalRegion;
}


namespace slint {
namespace cbindgen_private {

/// a Repr(C) variant of slint::platform::LayoutConstraints
struct LayoutConstraintsReprC {
    Size min;
    Size max;
    Size preferred;
    bool has_min;
    bool has_max;
};

using WindowAdapterUserData = void*;

struct RendererPtr {
    const void *_a;
    const void *_b;
};

using PlatformUserData = void*;

struct PlatformTaskOpaque {
    const void *_0;
    const void *_1;
};

using SoftwareRendererOpaque = const void*;

using CppTargetPixelBufferUserData = void*;

struct DrawRectangleArgs {
    float x;
    float y;
    float width;
    float height;
    float top_left_radius;
    float top_right_radius;
    float bottom_right_radius;
    float bottom_left_radius;
    float border_width;
    Brush background;
    Brush border;
    uint8_t alpha;
    /// 0, 90, 180, or 270
    int32_t rotation;
};

struct DrawTextureArgs {
    const uint8_t *image_data;
    TexturePixelFormat pixel_format;
    uintptr_t byte_stride;
    uint32_t width;
    uint32_t height;
    Color colorize;
    uint8_t alpha;
    intptr_t dst_x;
    intptr_t dst_y;
    uintptr_t dst_width;
    uintptr_t dst_height;
    /// 0, 90, 180, or 270
    int32_t rotation;
    bool has_tiling;
    int32_t tiling_offset_x;
    int32_t tiling_offset_y;
    float tiling_scale_x;
    float tiling_scale_y;
    uint32_t tiling_gap_x;
    uint32_t tiling_gap_y;
};

template<typename T>
struct CppTargetPixelBuffer {
    CppTargetPixelBufferUserData user_data;
    void (*line_slice)(CppTargetPixelBufferUserData, uintptr_t, T **slice_ptr, uintptr_t *slice_len);
    uintptr_t (*num_lines)(CppTargetPixelBufferUserData);
    bool (*fill_background)(CppTargetPixelBufferUserData, const Brush*, const PhysicalRegion*);
    bool (*draw_rectangle)(CppTargetPixelBufferUserData,
                           const DrawRectangleArgs*,
                           const PhysicalRegion*);
    bool (*draw_texture)(CppTargetPixelBufferUserData, const DrawTextureArgs*, const PhysicalRegion*);
};

using CppRawHandleOpaque = const void*;

using SkiaRendererOpaque = const void*;

using DropCallback = void(*)(const void *obj);

using TranslateCallback = void(*)(const void *obj,
                                  Slice<uint8_t> string,
                                  Slice<uint8_t> context,
                                  SharedString *out);

using NTranslateCallback = void(*)(const void *obj,
                                   uint64_t n,
                                   Slice<uint8_t> singular,
                                   Slice<uint8_t> plural,
                                   Slice<uint8_t> context,
                                   SharedString *out);

extern "C" {

void slint_windowrc_init(WindowAdapterRcOpaque *out);

void slint_ensure_backend();

/// Enters the main event loop.
void slint_run_event_loop(bool quit_on_last_window_closed);

/// Will execute the given functor in the main thread
void slint_post_event(void (*event)(void *user_data),
                      void *user_data,
                      void (*drop_user_data)(void*));

void slint_quit_event_loop();

void slint_register_font_from_path(const WindowAdapterRcOpaque *win,
                                   const SharedString *path,
                                   SharedString *error_str);

void slint_register_font_from_data(const WindowAdapterRcOpaque *win,
                                   Slice<uint8_t> data,
                                   SharedString *error_str);

void slint_register_bitmap_font(const WindowAdapterRcOpaque *win, const BitmapFont *font_data);

bool slint_string_to_float(const SharedString *string, float *value);

uintptr_t slint_string_character_count(const SharedString *string);

bool slint_string_to_usize(const SharedString *string, uintptr_t *value);

void slint_debug(const SharedString *string);

void slint_set_xdg_app_id(const SharedString *_app_id);

OperatingSystemType slint_detect_operating_system();

void slint_parse_markdown(const SharedString *format_string,
                          Slice<StyledText> args,
                          StyledText *out);

bool slint_open_url(const SharedString *url, const WindowAdapterRcOpaque *win);

void slint_string_to_styled_text(const SharedString *text, StyledText *out);

void slint_window_properties_get_title(const WindowProperties *wp, SharedString *out);

void slint_window_properties_get_background(const WindowProperties *wp, Brush *out);

bool slint_window_properties_get_fullscreen(const WindowProperties *wp);

bool slint_window_properties_get_minimized(const WindowProperties *wp);

bool slint_window_properties_get_maximized(const WindowProperties *wp);

LayoutConstraintsReprC slint_window_properties_get_layout_constraints(const WindowProperties *wp);

void slint_window_adapter_new(WindowAdapterUserData user_data,
                              void (*drop)(WindowAdapterUserData),
                              RendererPtr (*get_renderer_ref)(WindowAdapterUserData),
                              void (*set_visible)(WindowAdapterUserData, bool),
                              void (*request_redraw)(WindowAdapterUserData),
                              IntSize (*size)(WindowAdapterUserData),
                              void (*set_size)(WindowAdapterUserData, IntSize),
                              void (*update_window_properties)(WindowAdapterUserData,
                                                               const WindowProperties*),
                              bool (*position)(WindowAdapterUserData, Point2D<int32_t>*),
                              void (*set_position)(WindowAdapterUserData, Point2D<int32_t>),
                              WindowAdapterRcOpaque *target);

void slint_platform_register(PlatformUserData user_data,
                             void (*drop)(PlatformUserData),
                             void (*window_factory)(PlatformUserData, WindowAdapterRcOpaque*),
                             uint64_t (*duration_since_start)(PlatformUserData),
                             void (*set_clipboard_text)(PlatformUserData,
                                                        const SharedString*,
                                                        Clipboard),
                             bool (*clipboard_text)(PlatformUserData, SharedString*, Clipboard),
                             void (*run_event_loop)(PlatformUserData),
                             void (*quit_event_loop)(PlatformUserData),
                             void (*invoke_from_event_loop)(PlatformUserData, PlatformTaskOpaque));

bool slint_windowrc_has_active_animations(const WindowAdapterRcOpaque *handle);

void slint_platform_update_timers_and_animations();

/// Returns the duration in millisecond until the next timer or `u64::MAX` if there is no pending timers
uint64_t slint_platform_duration_until_next_timer_update();

void slint_platform_task_drop(PlatformTaskOpaque event);

void slint_platform_task_run(PlatformTaskOpaque event);

SoftwareRendererOpaque slint_software_renderer_new(uint32_t buffer_age);

void slint_software_renderer_drop(SoftwareRendererOpaque r);

PhysicalRegion slint_software_renderer_render_rgb8(SoftwareRendererOpaque r,
                                                   Rgb8Pixel *buffer,
                                                   uintptr_t buffer_len,
                                                   uintptr_t pixel_stride);

PhysicalRegion slint_software_renderer_render_accel_rgb8(SoftwareRendererOpaque r,
                                                         CppTargetPixelBuffer<Rgb8Pixel> *buffer);

PhysicalRegion slint_software_renderer_render_accel_rgb565(SoftwareRendererOpaque r,
                                                           CppTargetPixelBuffer<Rgb565Pixel> *buffer);

PhysicalRegion slint_software_renderer_render_rgb565(SoftwareRendererOpaque r,
                                                     uint16_t *buffer,
                                                     uintptr_t buffer_len,
                                                     uintptr_t pixel_stride);

PhysicalRegion slint_software_renderer_render_by_line_rgb565(SoftwareRendererOpaque r,
                                                             void (*process_line_fn)(void*,
                                                                                     uintptr_t,
                                                                                     uintptr_t,
                                                                                     uintptr_t,
                                                                                     void(*)(const void*,
                                                                                             Rgb565Pixel*,
                                                                                             uintptr_t),
                                                                                     const void*),
                                                             void *user_data);

PhysicalRegion slint_software_renderer_render_by_line_rgb8(SoftwareRendererOpaque r,
                                                           void (*process_line_fn)(void*,
                                                                                   uintptr_t,
                                                                                   uintptr_t,
                                                                                   uintptr_t,
                                                                                   void(*)(const void*,
                                                                                           Rgb8Pixel*,
                                                                                           uintptr_t),
                                                                                   const void*),
                                                           void *user_data);

void slint_software_renderer_set_rendering_rotation(SoftwareRendererOpaque r, int32_t rotation);

RendererPtr slint_software_renderer_handle(SoftwareRendererOpaque r);

void slint_software_renderer_region_to_rects(const PhysicalRegion *region,
                                             SharedVector<IntRect> *out);

CppRawHandleOpaque slint_new_raw_window_handle_win32(void *hwnd, void *_hinstance);

CppRawHandleOpaque slint_new_raw_window_handle_x11_xcb(uint32_t window,
                                                       uint32_t visual_id,
                                                       void *connection,
                                                       int screen);

CppRawHandleOpaque slint_new_raw_window_handle_x11_xlib(unsigned long window,
                                                        unsigned long visual_id,
                                                        void *display,
                                                        int screen);

CppRawHandleOpaque slint_new_raw_window_handle_wayland(void *surface, void *display);

CppRawHandleOpaque slint_new_raw_window_handle_appkit(void *ns_view, void *_ns_window);

void slint_raw_window_handle_drop(CppRawHandleOpaque handle);

SkiaRendererOpaque slint_skia_renderer_new(CppRawHandleOpaque handle_opaque, IntSize size);

void slint_skia_renderer_drop(SkiaRendererOpaque r);

void slint_skia_renderer_render(SkiaRendererOpaque r);

RendererPtr slint_skia_renderer_handle(SkiaRendererOpaque r);

bool slint_translate_set_translator(const void *obj,
                                    DropCallback drop,
                                    TranslateCallback translate,
                                    NTranslateCallback ntranslate);

}  // extern "C"

}  // namespace cbindgen_private
}  // namespace slint
