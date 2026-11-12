#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "image.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"

#define APP_CLASS L"FustimageWindow"
#define APP_TITLE L"Fustimage"
#define APP_VERSION L"0.1"
#define TIMER_ANIM 1
#define TIMER_TOOLTIP 2
#define WM_APP_LOAD_DONE (WM_APP + 1)
#define MAX_UNDO 64
#define MAX_CONTROLS 32
#define PI_F 3.14159265358979323846f
#define LUMA_RELEASE(p)                                                        \
    do {                                                                       \
        if ((p) != NULL) {                                                     \
            IUnknown *luma_rel_ = (IUnknown *)(p);                             \
            luma_rel_->lpVtbl->Release(luma_rel_);                             \
            (p) = NULL;                                                        \
        }                                                                      \
    } while (0)

typedef D2D1_COLOR_F Color;
typedef D2D1_RECT_F Rect;
typedef D2D1_POINT_2F Point;

enum {
    CTRL_NONE = -1,
    CTRL_OPEN = 0,
    CTRL_UNDO,
    CTRL_REDO,
    CTRL_EXPORT,
    CTRL_COPY,
    CTRL_ROT_L90,
    CTRL_ROT_R90,
    CTRL_ROT_L180,
    CTRL_ROT_R180,
    CTRL_FLIP_H,
    CTRL_FLIP_V,
    CTRL_RATIO_16_9,
    CTRL_RATIO_1_1,
    CTRL_RATIO_9_16,
    CTRL_RATIO_4_3,
    CTRL_RATIO_3_4,
    CTRL_RATIO_3_2,
    CTRL_RATIO_2_3,
    CTRL_RATIO_FREE,
    CTRL_APPLY_CROP,
    CTRL_CANCEL_CROP,
    CTRL_THEME,
    CTRL_COUNT
};

typedef enum {
    STYLE_PRIMARY,
    STYLE_TONAL,
    STYLE_CARD,
    STYLE_RATIO,
    STYLE_THEME,
    STYLE_ACTIONS
} ControlStyle;

typedef enum {
    ICON_NONE,
    ICON_OPEN,
    ICON_UNDO,
    ICON_REDO,
    ICON_EXPORT,
    ICON_COPY,
    ICON_ROTATE_L90,
    ICON_ROTATE_R90,
    ICON_ROTATE_L180,
    ICON_ROTATE_R180,
    ICON_FLIP_H,
    ICON_FLIP_V,
    ICON_CHECK,
    ICON_CLOSE,
    ICON_SUN,
    ICON_MOON,
    ICON_IMAGE,
    ICON_CROP,
    ICON_INFO,
    ICON_ZOOM,
    ICON_SPARKLE
} IconKind;

typedef struct {
    Color surface;
    Color surface_dim;
    Color surface_bright;
    Color container_lowest;
    Color container_low;
    Color container;
    Color container_high;
    Color container_highest;
    Color on_surface;
    Color on_surface_variant;
    Color outline;
    Color outline_variant;
    Color primary;
    Color on_primary;
    Color primary_container;
    Color on_primary_container;
    Color secondary;
    Color on_secondary;
    Color secondary_container;
    Color on_secondary_container;
    Color tertiary;
    Color on_tertiary;
    Color tertiary_container;
    Color on_tertiary_container;
    Color error;
    Color on_error;
    Color error_container;
    Color on_error_container;
    Color inverse_surface;
    Color inverse_on_surface;
    Color canvas;
    Color shadow;
} LumaTheme;

typedef struct {
    int rotation;
    LumaCrop crop;
    bool mirror_h;
    bool mirror_v;
} LumaEdit;

typedef struct {
    int id;
    Rect rect;
    const wchar_t *label;
    const wchar_t *tooltip;
    IconKind icon;
    ControlStyle style;
    bool enabled;
    bool selected;
    bool scrolled;
    bool icon_only;
    int ratio_index;
    float aspect;
} LumaControl;

typedef struct {
    HWND hwnd;
    HINSTANCE instance;
    ID2D1Factory *d2d;
    IDWriteFactory *dwrite;
    ID2D1HwndRenderTarget *hwnd_rt;
    ID2D1RenderTarget *rt;
    ID2D1SolidColorBrush *brush;
    ID2D1StrokeStyle *round_style;
    ID2D1StrokeStyle *dash_style;
    ID2D1Bitmap *bitmap;

    IDWriteTextFormat *f_brand;
    IDWriteTextFormat *f_h1;
    IDWriteTextFormat *f_h2;
    IDWriteTextFormat *f_title;
    IDWriteTextFormat *f_body;
    IDWriteTextFormat *f_body_strong;
    IDWriteTextFormat *f_label;
    IDWriteTextFormat *f_button;
    IDWriteTextFormat *f_caption;
    IDWriteTextFormat *f_tiny;

    LumaTheme theme;
    bool dark;
    float dpi;
    float width;
    float height;
    float sidebar_w;
    float margin;
    bool compact;
    bool wide;

    Rect sidebar;
    Rect appbar;
    Rect canvas;
    float footer_h;
    float footer_top;
    float tools_top;
    float tools_bottom;
    float scroll_y;
    float scroll_max;
    float content_height;
    float actions_y;
    bool actions_pinned;
    float rotation_header_y;
    float rotation_cards_y;
    float crop_header_y;
    float crop_divider_y;
    float crop_cards_y;
    float crop_hint_y;
    float crop_panel_top;
    float crop_panel_height;
    float theme_button_y;
    bool crop_panel_visible;

    LumaImage image;
    bool has_image;
    wchar_t path[MAX_PATH];
    wchar_t display_name[160];
    wchar_t format_name[16];
    wchar_t size_text[32];
    int rotation;
    LumaCrop crop;
    bool mirror_h;
    bool mirror_v;
    int initial_rotation;
    LumaCrop initial_crop;
    bool initial_mirror_h;
    bool initial_mirror_v;
    bool edited;
    bool exported;
    LumaEdit undo[MAX_UNDO];
    int undo_count;
    LumaEdit redo[MAX_UNDO];
    int redo_count;

    bool fit;
    float zoom;
    Point pan;
    Rect image_rect;
    bool image_visible;
    float fit_scale;
    float display_scale;
    float view_pad;

    bool crop_mode;
    float crop_aspect;
    int crop_preset;
    int crop_drag;
    Point drag_start;
    LumaCrop drag_start_crop;
    Rect drag_image_rect;
    LumaEdit crop_before;
    bool crop_before_valid;

    bool mouse_down;
    float mouse_x;
    float mouse_y;
    int hover_id;
    int pressed_id;
    int focus_id;
    bool focus_visible;
    bool panning;
    Point pan_start;
    Point pan_origin;
    int cursor_kind;

    LumaControl controls[MAX_CONTROLS];
    int control_count;

    double now;
    ULONGLONG last_tick;
    float reveal;
    bool anim_timer;
    bool tooltip_timer;
    ULONGLONG hover_since;

    wchar_t toast[512];
    bool toast_error;
    bool toast_active;
    double toast_start;
    double toast_until;

    bool loading;
    wchar_t loading_name[160];
    LONG load_id;
    HANDLE load_thread;

    bool closing;
    wchar_t startup_path[MAX_PATH];
    bool startup_dark;
    bool startup_light;
    wchar_t startup_crop[16];
} LumaApp;

static LumaApp g_app;

typedef struct {
    const wchar_t *label;
    float aspect;
    const wchar_t *tooltip;
} RatioInfo;

static const RatioInfo RATIOS[] = {
    {L"16:9", 16.0f / 9.0f, L"裁剪为 16:9 宽屏"},
    {L"1:1", 1.0f, L"裁剪为 1:1 方形"},
    {L"9:16", 9.0f / 16.0f, L"裁剪为 9:16 竖屏"},
    {L"4:3", 4.0f / 3.0f, L"裁剪为 4:3 经典比例"},
    {L"3:4", 3.0f / 4.0f, L"裁剪为 3:4 竖幅"},
    {L"3:2", 3.0f / 2.0f, L"裁剪为 3:2 摄影比例"},
    {L"2:3", 2.0f / 3.0f, L"裁剪为 2:3 竖幅摄影"},
    {L"∞", 0.0f, L"自由裁剪，拖动边角调整"},
};

static Color rgba(unsigned rgb, float alpha) {
    Color c;
    c.r = (float)((rgb >> 16) & 0xffu) / 255.0f;
    c.g = (float)((rgb >> 8) & 0xffu) / 255.0f;
    c.b = (float)(rgb & 0xffu) / 255.0f;
    c.a = alpha;
    return c;
}

static Color color_alpha(Color c, float alpha) {
    c.a *= alpha;
    return c;
}

static Color color_mix(Color a, Color b, float t) {
    Color c;
    c.r = a.r + (b.r - a.r) * t;
    c.g = a.g + (b.g - a.g) * t;
    c.b = a.b + (b.b - a.b) * t;
    c.a = a.a + (b.a - a.a) * t;
    return c;
}

static Rect rect_make(float left, float top, float right, float bottom) {
    Rect r;
    r.left = left;
    r.top = top;
    r.right = right;
    r.bottom = bottom;
    return r;
}

static Rect rect_inset(Rect r, float dx, float dy) {
    return rect_make(r.left + dx, r.top + dy, r.right - dx, r.bottom - dy);
}

static Rect rect_offset(Rect r, float dx, float dy) {
    return rect_make(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy);
}

static float rect_w(Rect r) { return r.right - r.left; }
static float rect_h(Rect r) { return r.bottom - r.top; }
static float rect_cx(Rect r) { return (r.left + r.right) * 0.5f; }
static float rect_cy(Rect r) { return (r.top + r.bottom) * 0.5f; }
static bool rect_contains(Rect r, float x, float y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

static Point point_make(float x, float y) {
    Point p;
    p.x = x;
    p.y = y;
    return p;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void copy_utf16(wchar_t *dst, size_t cap, const wchar_t *src) {
    if (cap == 0) return;
    if (src == NULL) {
        dst[0] = L'\0';
        return;
    }
    wcsncpy(dst, src, cap - 1);
    dst[cap - 1] = L'\0';
}

static void theme_light(LumaTheme *t) {
    memset(t, 0, sizeof(*t));
    t->surface = rgba(0xFCF8FF, 1.0f);
    t->surface_dim = rgba(0xDCD6E8, 1.0f);
    t->surface_bright = rgba(0xFFFBFF, 1.0f);
    t->container_lowest = rgba(0xFFFFFF, 1.0f);
    t->container_low = rgba(0xF7F2FC, 1.0f);
    t->container = rgba(0xF1EBF7, 1.0f);
    t->container_high = rgba(0xEBE4F2, 1.0f);
    t->container_highest = rgba(0xE4DCEC, 1.0f);
    t->on_surface = rgba(0x1C1B20, 1.0f);
    t->on_surface_variant = rgba(0x48454E, 1.0f);
    t->outline = rgba(0x79747E, 1.0f);
    t->outline_variant = rgba(0xCAC4D0, 1.0f);
    t->primary = rgba(0x5B57D1, 1.0f);
    t->on_primary = rgba(0xFFFFFF, 1.0f);
    t->primary_container = rgba(0xE4DEFF, 1.0f);
    t->on_primary_container = rgba(0x181146, 1.0f);
    t->secondary = rgba(0x006A67, 1.0f);
    t->on_secondary = rgba(0xFFFFFF, 1.0f);
    t->secondary_container = rgba(0xA6F0EC, 1.0f);
    t->on_secondary_container = rgba(0x00201F, 1.0f);
    t->tertiary = rgba(0x8B4B5F, 1.0f);
    t->on_tertiary = rgba(0xFFFFFF, 1.0f);
    t->tertiary_container = rgba(0xFFD9E2, 1.0f);
    t->on_tertiary_container = rgba(0x3A071A, 1.0f);
    t->error = rgba(0xBA1A1A, 1.0f);
    t->on_error = rgba(0xFFFFFF, 1.0f);
    t->error_container = rgba(0xFFDAD6, 1.0f);
    t->on_error_container = rgba(0x410002, 1.0f);
    t->inverse_surface = rgba(0x313035, 1.0f);
    t->inverse_on_surface = rgba(0xF4EFF4, 1.0f);
    t->canvas = rgba(0xF2EDF8, 1.0f);
    t->shadow = rgba(0x1C1630, 0.22f);
}

static void theme_dark(LumaTheme *t) {
    memset(t, 0, sizeof(*t));
    t->surface = rgba(0x141218, 1.0f);
    t->surface_dim = rgba(0x141218, 1.0f);
    t->surface_bright = rgba(0x3B3743, 1.0f);
    t->container_lowest = rgba(0x0F0D13, 1.0f);
    t->container_low = rgba(0x1A181F, 1.0f);
    t->container = rgba(0x201D25, 1.0f);
    t->container_high = rgba(0x2A2730, 1.0f);
    t->container_highest = rgba(0x35313B, 1.0f);
    t->on_surface = rgba(0xE7E0E9, 1.0f);
    t->on_surface_variant = rgba(0xCAC4D0, 1.0f);
    t->outline = rgba(0x948F99, 1.0f);
    t->outline_variant = rgba(0x4A454F, 1.0f);
    t->primary = rgba(0xC9C1FF, 1.0f);
    t->on_primary = rgba(0x2C276C, 1.0f);
    t->primary_container = rgba(0x44408C, 1.0f);
    t->on_primary_container = rgba(0xE4DEFF, 1.0f);
    t->secondary = rgba(0x80D5D1, 1.0f);
    t->on_secondary = rgba(0x003735, 1.0f);
    t->secondary_container = rgba(0x00504D, 1.0f);
    t->on_secondary_container = rgba(0x9CF1EC, 1.0f);
    t->tertiary = rgba(0xFFB1C5, 1.0f);
    t->on_tertiary = rgba(0x551D31, 1.0f);
    t->tertiary_container = rgba(0x6C3347, 1.0f);
    t->on_tertiary_container = rgba(0xFFD9E2, 1.0f);
    t->error = rgba(0xFFB4AB, 1.0f);
    t->on_error = rgba(0x690005, 1.0f);
    t->error_container = rgba(0x93000A, 1.0f);
    t->on_error_container = rgba(0xFFDAD6, 1.0f);
    t->inverse_surface = rgba(0xE7E0E9, 1.0f);
    t->inverse_on_surface = rgba(0x313035, 1.0f);
    t->canvas = rgba(0x100E14, 1.0f);
    t->shadow = rgba(0x000000, 0.45f);
}

static void apply_theme_to_window(void) {
    BOOL use_dark = g_app.dark ? TRUE : FALSE;
    COLORREF caption;
    COLORREF text;
    COLORREF border;
    int corner = 2;

    if (g_app.hwnd == NULL) return;
    if (g_app.dark) theme_dark(&g_app.theme);
    else theme_light(&g_app.theme);

    caption = g_app.dark ? RGB(20, 18, 24) : RGB(252, 248, 255);
    text = g_app.dark ? RGB(231, 224, 233) : RGB(28, 27, 32);
    border = g_app.dark ? RGB(45, 41, 52) : RGB(220, 214, 232);
    DwmSetWindowAttribute(g_app.hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &use_dark, sizeof(use_dark));
    DwmSetWindowAttribute(g_app.hwnd, DWMWA_CAPTION_COLOR, &caption,
                          sizeof(caption));
    DwmSetWindowAttribute(g_app.hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
    DwmSetWindowAttribute(g_app.hwnd, DWMWA_BORDER_COLOR, &border,
                          sizeof(border));
    DwmSetWindowAttribute(g_app.hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner,
                          sizeof(corner));
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void show_toast(const wchar_t *text, bool error) {
    copy_utf16(g_app.toast, 512, text);
    g_app.toast_error = error;
    g_app.toast_active = true;
    g_app.toast_start = g_app.now;
    g_app.toast_until = g_app.now + (error ? 5.0 : 3.6);
    SetTimer(g_app.hwnd, TIMER_ANIM, 16, NULL);
    g_app.anim_timer = true;
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static float text_width(const wchar_t *text, IDWriteTextFormat *format) {
    IDWriteTextLayout *layout = NULL;
    DWRITE_TEXT_METRICS metrics;
    float width = 0.0f;
    if (text == NULL || format == NULL || g_app.dwrite == NULL) return 0.0f;
    if (SUCCEEDED(g_app.dwrite->lpVtbl->CreateTextLayout(
            g_app.dwrite, text, (UINT32)wcslen(text), format, 4096.0f,
            200.0f, &layout)) &&
        layout != NULL) {
        if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics))) {
            width = metrics.widthIncludingTrailingWhitespace;
        }
        layout->lpVtbl->Release(layout);
    }
    return width;
}

static IDWriteTextFormat *make_format(const wchar_t *family,
                                      DWRITE_FONT_WEIGHT weight, float size) {
    IDWriteTextFormat *format = NULL;
    HRESULT hr;
    if (g_app.dwrite == NULL) return NULL;
    hr = g_app.dwrite->lpVtbl->CreateTextFormat(
        g_app.dwrite, family, NULL, weight, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"zh-CN", &format);
    if (FAILED(hr) || format == NULL) {
        format = NULL;
        hr = g_app.dwrite->lpVtbl->CreateTextFormat(
            g_app.dwrite, L"Microsoft YaHei UI", NULL, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
            L"zh-CN", &format);
    }
    if (FAILED(hr) || format == NULL) {
        format = NULL;
        hr = g_app.dwrite->lpVtbl->CreateTextFormat(
            g_app.dwrite, L"Segoe UI", NULL, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
            L"zh-CN", &format);
    }
    if (FAILED(hr) || format == NULL) return NULL;
    format->lpVtbl->SetWordWrapping(format, DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

static bool create_text_formats(void) {
    g_app.f_brand = make_format(L"Segoe UI Variable Display",
                                DWRITE_FONT_WEIGHT_BOLD, 21.0f);
    g_app.f_h1 = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_SEMI_BOLD,
                             25.0f);
    g_app.f_h2 = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_SEMI_BOLD,
                             18.0f);
    g_app.f_title = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                15.0f);
    g_app.f_body = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_NORMAL,
                               12.5f);
    g_app.f_body_strong =
        make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_MEDIUM, 12.5f);
    g_app.f_label = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_MEDIUM,
                                11.5f);
    g_app.f_button = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                 12.5f);
    g_app.f_caption = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_NORMAL,
                                  10.5f);
    g_app.f_tiny = make_format(L"Noto Sans SC", DWRITE_FONT_WEIGHT_MEDIUM,
                               9.0f);
    return g_app.f_brand && g_app.f_h1 && g_app.f_h2 && g_app.f_title &&
           g_app.f_body && g_app.f_body_strong && g_app.f_label &&
           g_app.f_button && g_app.f_caption && g_app.f_tiny;
}

static void set_brush(Color color) {
    if (g_app.brush != NULL) {
        g_app.brush->lpVtbl->SetColor(g_app.brush, &color);
    }
}

#define UI_BRUSH ((ID2D1Brush *)g_app.brush)

static void fill_rect(Rect r, Color color) {
    set_brush(color);
    g_app.rt->lpVtbl->FillRectangle(g_app.rt, &r, UI_BRUSH);
}

static void fill_rounded(Rect r, float radius, Color color) {
    D2D1_ROUNDED_RECT rr;
    float max_radius = rect_w(r) < rect_h(r) ? rect_w(r) / 2.0f : rect_h(r) / 2.0f;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0.0f) radius = 0.0f;
    rr.rect = r;
    rr.radiusX = radius;
    rr.radiusY = radius;
    set_brush(color);
    g_app.rt->lpVtbl->FillRoundedRectangle(g_app.rt, &rr, UI_BRUSH);
}

static void stroke_rounded(Rect r, float radius, Color color, float width,
                           ID2D1StrokeStyle *style) {
    D2D1_ROUNDED_RECT rr;
    float max_radius = rect_w(r) < rect_h(r) ? rect_w(r) / 2.0f : rect_h(r) / 2.0f;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0.0f) radius = 0.0f;
    rr.rect = r;
    rr.radiusX = radius;
    rr.radiusY = radius;
    set_brush(color);
    g_app.rt->lpVtbl->DrawRoundedRectangle(g_app.rt, &rr, UI_BRUSH, width,
                                           style);
}

static void stroke_rect(Rect r, Color color, float width) {
    set_brush(color);
    g_app.rt->lpVtbl->DrawRectangle(g_app.rt, &r, UI_BRUSH, width, NULL);
}

static void fill_ellipse(float cx, float cy, float rx, float ry, Color color) {
    D2D1_ELLIPSE e;
    e.point.x = cx;
    e.point.y = cy;
    e.radiusX = rx;
    e.radiusY = ry;
    set_brush(color);
    g_app.rt->lpVtbl->FillEllipse(g_app.rt, &e, UI_BRUSH);
}

static void stroke_ellipse(float cx, float cy, float rx, float ry, Color color,
                           float width) {
    D2D1_ELLIPSE e;
    e.point.x = cx;
    e.point.y = cy;
    e.radiusX = rx;
    e.radiusY = ry;
    set_brush(color);
    g_app.rt->lpVtbl->DrawEllipse(g_app.rt, &e, UI_BRUSH, width,
                                  g_app.round_style);
}

static void line(float x1, float y1, float x2, float y2, Color color,
                 float width) {
    Point a = point_make(x1, y1);
    Point b = point_make(x2, y2);
    set_brush(color);
    g_app.rt->lpVtbl->DrawLine(g_app.rt, a, b, UI_BRUSH, width,
                               g_app.round_style);
}

static void draw_text_raw(const wchar_t *text, IDWriteTextFormat *format,
                          Rect r, Color color, int align, int valign,
                          bool wrap, bool clip) {
    int length;
    if (text == NULL || format == NULL) return;
    length = (int)wcslen(text);
    if (length <= 0) return;
    format->lpVtbl->SetTextAlignment(
        format, align == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER
                           : (align == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING
                                         : DWRITE_TEXT_ALIGNMENT_LEADING));
    format->lpVtbl->SetParagraphAlignment(
        format, valign == 1 ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                            : (valign == 2 ? DWRITE_PARAGRAPH_ALIGNMENT_FAR
                                           : DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    format->lpVtbl->SetWordWrapping(
        format, wrap ? DWRITE_WORD_WRAPPING_WRAP
                     : DWRITE_WORD_WRAPPING_NO_WRAP);
    set_brush(color);
    if (clip) {
        Rect clip_rect = r;
        g_app.rt->lpVtbl->PushAxisAlignedClip(g_app.rt, &clip_rect,
                                              D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
    g_app.rt->lpVtbl->DrawText(g_app.rt, text, (UINT32)length, format, &r,
                               UI_BRUSH, D2D1_DRAW_TEXT_OPTIONS_NONE,
                               DWRITE_MEASURING_MODE_NATURAL);
    if (clip) {
        g_app.rt->lpVtbl->PopAxisAlignedClip(g_app.rt);
    }
}

static void draw_text_trimmed(const wchar_t *text, IDWriteTextFormat *format,
                              Rect r, Color color, bool middle) {
    IDWriteTextLayout *layout = NULL;
    IDWriteInlineObject *ellipsis = NULL;
    DWRITE_TRIMMING trimming;
    DWRITE_TEXT_METRICS metrics;
    D2D1_POINT_2F origin;
    if (text == NULL || format == NULL || text[0] == L'\0') return;
    if (FAILED(g_app.dwrite->lpVtbl->CreateTextLayout(
            g_app.dwrite, text, (UINT32)wcslen(text), format, rect_w(r),
            rect_h(r), &layout)) ||
        layout == NULL) {
        return;
    }
    layout->lpVtbl->SetWordWrapping(layout, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (SUCCEEDED(g_app.dwrite->lpVtbl->CreateEllipsisTrimmingSign(
            g_app.dwrite, format, &ellipsis))) {
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        trimming.delimiter = 0;
        trimming.delimiterCount = 0;
        layout->lpVtbl->SetTrimming(layout, &trimming, ellipsis);
    }
    layout->lpVtbl->GetMetrics(layout, &metrics);
    origin.x = r.left;
    origin.y = r.top;
    if (middle) origin.y += (rect_h(r) - metrics.height) * 0.5f;
    set_brush(color);
    g_app.rt->lpVtbl->PushAxisAlignedClip(g_app.rt, &r,
                                          D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_app.rt->lpVtbl->DrawTextLayout(g_app.rt, origin, layout, UI_BRUSH,
                                     D2D1_DRAW_TEXT_OPTIONS_CLIP);
    g_app.rt->lpVtbl->PopAxisAlignedClip(g_app.rt);
    if (ellipsis != NULL) ellipsis->lpVtbl->Release(ellipsis);
    layout->lpVtbl->Release(layout);
}

typedef struct {
    ID2D1Layer *layer;
    ID2D1RoundedRectangleGeometry *geometry;
} RoundClip;

static RoundClip push_round_clip(Rect r, float radius) {
    RoundClip clip;
    D2D1_ROUNDED_RECT rounded;
    D2D1_LAYER_PARAMETERS params;
    clip.layer = NULL;
    clip.geometry = NULL;
    rounded.rect = r;
    rounded.radiusX = radius;
    rounded.radiusY = radius;
    if (g_app.d2d == NULL || g_app.rt == NULL) return clip;
    if (FAILED(g_app.d2d->lpVtbl->CreateRoundedRectangleGeometry(
            g_app.d2d, &rounded, &clip.geometry)) ||
        clip.geometry == NULL) {
        return clip;
    }
    if (FAILED(g_app.rt->lpVtbl->CreateLayer(g_app.rt, NULL, &clip.layer)) ||
        clip.layer == NULL) {
        LUMA_RELEASE(clip.geometry);
        return clip;
    }
    memset(&params, 0, sizeof(params));
    params.contentBounds = r;
    params.geometricMask = (ID2D1Geometry *)clip.geometry;
    params.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    params.maskTransform._11 = 1.0f;
    params.maskTransform._22 = 1.0f;
    params.opacity = 1.0f;
    g_app.rt->lpVtbl->PushLayer(g_app.rt, &params, clip.layer);
    return clip;
}

static void pop_round_clip(RoundClip *clip) {
    if (clip->layer != NULL) {
        g_app.rt->lpVtbl->PopLayer(g_app.rt);
        LUMA_RELEASE(clip->layer);
    }
    LUMA_RELEASE(clip->geometry);
}

static void draw_soft_shadow(Rect r, float radius, float strength) {
    int i;
    for (i = 4; i >= 1; --i) {
        Rect s = rect_offset(rect_inset(r, -(float)i * 2.2f, -(float)i * 1.4f),
                             0.0f, (float)i * 1.8f);
        fill_rounded(s, radius + (float)i * 1.6f,
                     color_alpha(g_app.theme.shadow,
                                 strength * (0.10f / (float)i)));
    }
}

static void sink_add_bezier(ID2D1GeometrySink *sink,
                            const D2D1_BEZIER_SEGMENT *segment) {
    ID2D1GeometrySink_AddBezier(sink, segment);
}

static void fill_triangle(Point a, Point b, Point c, Color color) {
    ID2D1PathGeometry *path = NULL;
    ID2D1GeometrySink *sink = NULL;
    if (FAILED(g_app.d2d->lpVtbl->CreatePathGeometry(g_app.d2d, &path)) ||
        path == NULL) {
        return;
    }
    if (FAILED(path->lpVtbl->Open(path, &sink)) || sink == NULL) {
        LUMA_RELEASE(path);
        return;
    }
    ID2D1GeometrySink_BeginFigure(sink, a, D2D1_FIGURE_BEGIN_FILLED);
    ID2D1GeometrySink_AddLine(sink, b);
    ID2D1GeometrySink_AddLine(sink, c);
    ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
    ID2D1GeometrySink_Close(sink);
    LUMA_RELEASE(sink);
    set_brush(color);
    g_app.rt->lpVtbl->FillGeometry(g_app.rt, (ID2D1Geometry *)path,
                                   UI_BRUSH, NULL);
    LUMA_RELEASE(path);
}

static void draw_arc(float cx, float cy, float radius, float start_deg,
                     float sweep_deg, Color color, float width) {
    ID2D1PathGeometry *path = NULL;
    ID2D1GeometrySink *sink = NULL;
    D2D1_ARC_SEGMENT arc;
    Point start;
    Point end;
    float start_rad = start_deg * PI_F / 180.0f;
    float end_rad = (start_deg + sweep_deg) * PI_F / 180.0f;
    if (FAILED(g_app.d2d->lpVtbl->CreatePathGeometry(g_app.d2d, &path)) ||
        path == NULL) {
        return;
    }
    if (FAILED(path->lpVtbl->Open(path, &sink)) || sink == NULL) {
        LUMA_RELEASE(path);
        return;
    }
    start.x = cx + cosf(start_rad) * radius;
    start.y = cy + sinf(start_rad) * radius;
    end.x = cx + cosf(end_rad) * radius;
    end.y = cy + sinf(end_rad) * radius;
    ID2D1GeometrySink_BeginFigure(sink, start, D2D1_FIGURE_BEGIN_HOLLOW);
    memset(&arc, 0, sizeof(arc));
    arc.point = end;
    arc.size.width = radius;
    arc.size.height = radius;
    arc.rotationAngle = 0.0f;
    arc.sweepDirection = sweep_deg >= 0.0f
                             ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                             : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
    arc.arcSize = fabsf(sweep_deg) > 180.0f ? D2D1_ARC_SIZE_LARGE
                                            : D2D1_ARC_SIZE_SMALL;
    ID2D1GeometrySink_AddArc(sink, &arc);
    ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_OPEN);
    ID2D1GeometrySink_Close(sink);
    LUMA_RELEASE(sink);
    set_brush(color);
    g_app.rt->lpVtbl->DrawGeometry(g_app.rt, (ID2D1Geometry *)path,
                                   UI_BRUSH, width, g_app.round_style);
    LUMA_RELEASE(path);
}

static void draw_arc_arrow(float cx, float cy, float radius, float start_deg,
                           float sweep_deg, Color color, float width) {
    float end_deg = start_deg + sweep_deg;
    float rad = end_deg * PI_F / 180.0f;
    float tangent = end_deg + (sweep_deg >= 0.0f ? 90.0f : -90.0f);
    float trad = tangent * PI_F / 180.0f;
    Point tip;
    Point back;
    Point wing_a;
    Point wing_b;
    Point p1;
    Point p2;

    draw_arc(cx, cy, radius, start_deg, sweep_deg, color, width);
    tip.x = cx + cosf(rad) * radius + cosf(trad) * width * 2.4f;
    tip.y = cy + sinf(rad) * radius + sinf(trad) * width * 2.4f;
    back.x = cx + cosf(rad) * radius - cosf(trad) * width * 0.8f;
    back.y = cy + sinf(rad) * radius - sinf(trad) * width * 0.8f;
    p1.x = -sinf(trad) * width * 2.7f;
    p1.y = cosf(trad) * width * 2.7f;
    wing_a.x = back.x + p1.x;
    wing_a.y = back.y + p1.y;
    wing_b.x = back.x - p1.x;
    wing_b.y = back.y - p1.y;
    fill_triangle(tip, wing_a, wing_b, color);
    (void)p2;
}

static void draw_logo_mark(Rect tile) {
    float radius = rect_w(tile) * 0.32f;
    float unit = rect_w(tile) / 40.0f;
    float left = tile.left;
    float top = tile.top;
    fill_rounded(tile, radius, g_app.theme.primary);
    fill_ellipse(tile.right - 9.0f * unit, tile.top + 9.0f * unit, 11.0f * unit,
                 10.0f * unit, color_alpha(g_app.theme.primary_container, 0.18f));
    fill_rounded(rect_make(left + 11.0f * unit, top + 8.0f * unit,
                           left + 16.0f * unit, top + 29.0f * unit),
                 2.2f * unit, g_app.theme.on_primary);
    fill_rounded(rect_make(left + 11.0f * unit, top + 8.0f * unit,
                           left + 27.0f * unit, top + 13.0f * unit),
                 2.2f * unit, g_app.theme.on_primary);
    fill_rounded(rect_make(left + 11.0f * unit, top + 17.0f * unit,
                           left + 24.0f * unit, top + 22.0f * unit),
                 2.2f * unit, g_app.theme.on_primary);
    fill_ellipse(left + 29.0f * unit, top + 28.0f * unit, 2.4f * unit,
                 2.4f * unit, g_app.theme.tertiary_container);
}

static void draw_icon(IconKind kind, Rect box, Color color) {
    float s = fminf(rect_w(box), rect_h(box)) / 24.0f;
    float ox = box.left + (rect_w(box) - 24.0f * s) * 0.5f;
    float oy = box.top + (rect_h(box) - 24.0f * s) * 0.5f;
    float stroke = fmaxf(1.25f, 1.75f * s);
    float cx = ox + 12.0f * s;
    float cy = oy + 12.0f * s;
#define IX(v) (ox + (float)(v) * s)
#define IY(v) (oy + (float)(v) * s)
    switch (kind) {
    case ICON_OPEN: {
        Rect frame = rect_make(IX(3), IY(4), IX(21), IY(20));
        stroke_rounded(frame, 2.6f * s, color, stroke, NULL);
        stroke_ellipse(IX(8.5f), IY(9.0f), 1.4f * s, 1.4f * s, color, stroke);
        line(IX(4.8f), IY(16.2f), IX(9.1f), IY(12.2f), color, stroke);
        line(IX(9.1f), IY(12.2f), IX(12.4f), IY(15.4f), color, stroke);
        line(IX(12.4f), IY(15.4f), IX(15.8f), IY(12.0f), color, stroke);
        line(IX(15.8f), IY(12.0f), IX(19.2f), IY(15.1f), color, stroke);
        break;
    }
    case ICON_UNDO:
        draw_arc_arrow(cx, cy, 7.0f * s, 205.0f, -255.0f, color, stroke);
        break;
    case ICON_REDO:
        draw_arc_arrow(cx, cy, 7.0f * s, 335.0f, 255.0f, color, stroke);
        break;
    case ICON_EXPORT:
        line(IX(12), IY(3.4f), IX(12), IY(14.0f), color, stroke);
        line(IX(8.2f), IY(10.4f), IX(12), IY(14.2f), color, stroke);
        line(IX(15.8f), IY(10.4f), IX(12), IY(14.2f), color, stroke);
        line(IX(4.8f), IY(15.4f), IX(4.8f), IY(19.5f), color, stroke);
        line(IX(4.8f), IY(19.5f), IX(19.2f), IY(19.5f), color, stroke);
        line(IX(19.2f), IY(15.4f), IX(19.2f), IY(19.5f), color, stroke);
        break;
    case ICON_COPY:
        stroke_rounded(rect_make(IX(4.0f), IY(7.0f), IX(16.0f), IY(20.0f)),
                       2.2f * s, color, stroke * 0.85f, NULL);
        stroke_rounded(rect_make(IX(8.0f), IY(4.0f), IX(20.0f), IY(17.0f)),
                       2.2f * s, color, stroke, NULL);
        break;
    case ICON_ROTATE_L90:
        draw_arc_arrow(cx, cy, 7.2f * s, 340.0f, -100.0f, color, stroke);
        line(IX(12), IY(12), IX(12), IY(12.5f), color, stroke);
        break;
    case ICON_ROTATE_R90:
        draw_arc_arrow(cx, cy, 7.2f * s, 200.0f, 100.0f, color, stroke);
        line(IX(12), IY(12), IX(12), IY(12.5f), color, stroke);
        break;
    case ICON_ROTATE_L180:
        draw_arc_arrow(cx, cy, 7.2f * s, 340.0f, -210.0f, color, stroke);
        break;
    case ICON_ROTATE_R180:
        draw_arc_arrow(cx, cy, 7.2f * s, 200.0f, 210.0f, color, stroke);
        break;
    case ICON_FLIP_H: {
        float a;
        for (a = 4.0f; a < 21.0f; a += 4.6f) {
            line(IX(12.0f), IY(a), IX(12.0f), IY(a + 2.4f), color,
                 1.35f * s);
        }
        fill_triangle(point_make(IX(3.2f), IY(6.2f)),
                      point_make(IX(3.2f), IY(17.8f)),
                      point_make(IX(9.4f), IY(12.0f)), color);
        fill_triangle(point_make(IX(20.8f), IY(6.2f)),
                      point_make(IX(20.8f), IY(17.8f)),
                      point_make(IX(14.6f), IY(12.0f)),
                      color_alpha(color, 0.45f));
        break;
    }
    case ICON_FLIP_V: {
        float a;
        for (a = 4.0f; a < 21.0f; a += 4.6f) {
            line(IX(a), IY(12.0f), IX(a + 2.4f), IY(12.0f), color,
                 1.35f * s);
        }
        fill_triangle(point_make(IX(6.2f), IY(3.2f)),
                      point_make(IX(17.8f), IY(3.2f)),
                      point_make(IX(12.0f), IY(9.4f)), color);
        fill_triangle(point_make(IX(6.2f), IY(20.8f)),
                      point_make(IX(17.8f), IY(20.8f)),
                      point_make(IX(12.0f), IY(14.6f)),
                      color_alpha(color, 0.45f));
        break;
    }
    case ICON_CHECK:
        line(IX(4.8f), IY(12.4f), IX(9.8f), IY(17.2f), color, stroke + 0.35f * s);
        line(IX(9.8f), IY(17.2f), IX(19.2f), IY(6.8f), color, stroke + 0.35f * s);
        break;
    case ICON_CLOSE:
        line(IX(6.0f), IY(6.0f), IX(18.0f), IY(18.0f), color, stroke);
        line(IX(18.0f), IY(6.0f), IX(6.0f), IY(18.0f), color, stroke);
        break;
    case ICON_SUN:
        stroke_ellipse(cx, cy, 4.2f * s, 4.2f * s, color, stroke);
        {
            int i;
            for (i = 0; i < 8; ++i) {
                float a = (float)i * 45.0f * PI_F / 180.0f;
                line(cx + cosf(a) * 7.0f * s, cy + sinf(a) * 7.0f * s,
                     cx + cosf(a) * 9.4f * s, cy + sinf(a) * 9.4f * s, color,
                     stroke);
            }
        }
        break;
    case ICON_MOON: {
        ID2D1PathGeometry *path = NULL;
        ID2D1GeometrySink *sink = NULL;
        if (SUCCEEDED(g_app.d2d->lpVtbl->CreatePathGeometry(g_app.d2d, &path)) &&
            path != NULL &&
            SUCCEEDED(path->lpVtbl->Open(path, &sink)) && sink != NULL) {
            ID2D1GeometrySink_BeginFigure(sink, point_make(IX(15.4f), IY(3.4f)),
                                      D2D1_FIGURE_BEGIN_FILLED);
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){
                          {IX(8.8f), IY(5.0f)},
                          {IX(5.8f), IY(9.2f)},
                          {IX(5.8f), IY(13.4f)}});
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){
                          {IX(5.8f), IY(18.0f)},
                          {IX(9.4f), IY(20.8f)},
                          {IX(15.4f), IY(21.0f)}});
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){
                          {IX(11.0f), IY(17.6f)},
                          {IX(9.0f), IY(14.8f)},
                          {IX(9.0f), IY(11.8f)}});
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){
                          {IX(9.0f), IY(8.2f)},
                          {IX(11.4f), IY(5.0f)},
                          {IX(15.4f), IY(3.4f)}});
            ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
            ID2D1GeometrySink_Close(sink);
            LUMA_RELEASE(sink);
            set_brush(color);
            g_app.rt->lpVtbl->FillGeometry(g_app.rt, (ID2D1Geometry *)path,
                                           UI_BRUSH, NULL);
            LUMA_RELEASE(path);
        }
        break;
    }
    case ICON_IMAGE: {
        stroke_rounded(rect_make(IX(3.5f), IY(4.5f), IX(20.5f), IY(19.5f)),
                       2.5f * s, color, stroke, NULL);
        stroke_ellipse(IX(9.0f), IY(9.2f), 1.5f * s, 1.5f * s, color, stroke);
        line(IX(5.2f), IY(16.5f), IX(10.0f), IY(12.2f), color, stroke);
        line(IX(10.0f), IY(12.2f), IX(13.4f), IY(15.3f), color, stroke);
        line(IX(13.4f), IY(15.3f), IX(16.0f), IY(13.0f), color, stroke);
        line(IX(16.0f), IY(13.0f), IX(19.0f), IY(16.0f), color, stroke);
        break;
    }
    case ICON_CROP:
        line(IX(8.0f), IY(3.5f), IX(8.0f), IY(17.0f), color, stroke);
        line(IX(8.0f), IY(17.0f), IX(20.5f), IY(17.0f), color, stroke);
        line(IX(4.0f), IY(8.0f), IX(16.5f), IY(8.0f), color, stroke);
        line(IX(16.5f), IY(8.0f), IX(16.5f), IY(21.0f), color, stroke);
        break;
    case ICON_INFO: {
        float r = 8.2f * s;
        stroke_ellipse(cx, cy, r, r, color, stroke);
        fill_ellipse(cx, IY(8.2f), 1.15f * s, 1.15f * s, color);
        line(cx, IY(11.0f), cx, IY(16.4f), color, stroke);
        break;
    }
    case ICON_ZOOM:
        stroke_ellipse(IX(10.2f), IY(10.2f), 5.8f * s, 5.8f * s, color, stroke);
        line(IX(14.6f), IY(14.6f), IX(20.5f), IY(20.5f), color, stroke);
        break;
    case ICON_SPARKLE: {
        ID2D1PathGeometry *path = NULL;
        ID2D1GeometrySink *sink = NULL;
        if (SUCCEEDED(g_app.d2d->lpVtbl->CreatePathGeometry(g_app.d2d, &path)) &&
            path != NULL &&
            SUCCEEDED(path->lpVtbl->Open(path, &sink)) && sink != NULL) {
            ID2D1GeometrySink_BeginFigure(sink, point_make(IX(12.0f), IY(2.8f)),
                                      D2D1_FIGURE_BEGIN_FILLED);
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){{IX(12.5f), IY(7.5f)},
                                             {IX(13.5f), IY(9.0f)},
                                             {IX(18.4f), IY(9.7f)}});
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){{IX(14.0f), IY(10.4f)},
                                             {IX(12.8f), IY(12.0f)},
                                             {IX(12.4f), IY(16.8f)}});
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){{IX(11.8f), IY(12.1f)},
                                             {IX(10.7f), IY(10.5f)},
                                             {IX(5.8f), IY(9.8f)}});
            sink_add_bezier(
                sink, &(D2D1_BEZIER_SEGMENT){{IX(10.2f), IY(9.1f)},
                                             {IX(11.4f), IY(7.4f)},
                                             {IX(12.0f), IY(2.8f)}});
            ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
            ID2D1GeometrySink_Close(sink);
            LUMA_RELEASE(sink);
            set_brush(color);
            g_app.rt->lpVtbl->FillGeometry(g_app.rt, (ID2D1Geometry *)path,
                                           UI_BRUSH, NULL);
            LUMA_RELEASE(path);
        }
        break;
    }
    default:
        break;
    }
#undef IX
#undef IY
}

static void create_brush_resources(void) {
    D2D1_STROKE_STYLE_PROPERTIES props;
    if (g_app.rt == NULL) return;
    LUMA_RELEASE(g_app.brush);
    LUMA_RELEASE(g_app.round_style);
    LUMA_RELEASE(g_app.dash_style);
    g_app.rt->lpVtbl->CreateSolidColorBrush(g_app.rt, &(Color){0, 0, 0, 1},
                                            NULL, &g_app.brush);
    memset(&props, 0, sizeof(props));
    props.startCap = D2D1_CAP_STYLE_ROUND;
    props.endCap = D2D1_CAP_STYLE_ROUND;
    props.dashCap = D2D1_CAP_STYLE_ROUND;
    props.lineJoin = D2D1_LINE_JOIN_ROUND;
    props.miterLimit = 10.0f;
    props.dashStyle = D2D1_DASH_STYLE_SOLID;
    g_app.d2d->lpVtbl->CreateStrokeStyle(g_app.d2d, &props, NULL, 0,
                                         &g_app.round_style);
    props.dashStyle = D2D1_DASH_STYLE_DASH;
    props.dashOffset = 0.0f;
    g_app.d2d->lpVtbl->CreateStrokeStyle(g_app.d2d, &props, NULL, 0,
                                         &g_app.dash_style);
}

static void upload_photo_bitmap(void) {
    D2D1_BITMAP_PROPERTIES props;
    D2D1_SIZE_U size;
    if (g_app.rt == NULL || !g_app.has_image || g_app.image.pixels == NULL) {
        return;
    }
    LUMA_RELEASE(g_app.bitmap);
    memset(&props, 0, sizeof(props));
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = 96.0f;
    props.dpiY = 96.0f;
    size.width = g_app.image.width;
    size.height = g_app.image.height;
    g_app.rt->lpVtbl->CreateBitmap(g_app.rt, size, g_app.image.pixels,
                                   g_app.image.stride, &props, &g_app.bitmap);
}

static void discard_device_resources(void) {
    LUMA_RELEASE(g_app.bitmap);
    LUMA_RELEASE(g_app.brush);
    LUMA_RELEASE(g_app.round_style);
    LUMA_RELEASE(g_app.dash_style);
    g_app.rt = NULL;
    LUMA_RELEASE(g_app.hwnd_rt);
}

static bool create_device_resources(void) {
    RECT rc;
    D2D1_RENDER_TARGET_PROPERTIES props;
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props;
    HRESULT hr;
    if (g_app.hwnd == NULL || g_app.d2d == NULL) return false;
    discard_device_resources();
    GetClientRect(g_app.hwnd, &rc);
    memset(&props, 0, sizeof(props));
    props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    props.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_UNKNOWN;
    props.dpiX = g_app.dpi;
    props.dpiY = g_app.dpi;
    props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
    hwnd_props.hwnd = g_app.hwnd;
    hwnd_props.pixelSize.width = (UINT)(rc.right > 0 ? rc.right : 1);
    hwnd_props.pixelSize.height = (UINT)(rc.bottom > 0 ? rc.bottom : 1);
    hwnd_props.presentOptions = D2D1_PRESENT_OPTIONS_NONE;
    hr = g_app.d2d->lpVtbl->CreateHwndRenderTarget(
        g_app.d2d, &props, &hwnd_props, &g_app.hwnd_rt);
    if (FAILED(hr) || g_app.hwnd_rt == NULL) {
        g_app.hwnd_rt = NULL;
        return false;
    }
    g_app.rt = (ID2D1RenderTarget *)g_app.hwnd_rt;
    g_app.rt->lpVtbl->SetAntialiasMode(g_app.rt,
                                       D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_app.rt->lpVtbl->SetTextAntialiasMode(g_app.rt,
                                           D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    create_brush_resources();
    upload_photo_bitmap();
    return g_app.brush != NULL;
}

static LumaControl *add_control(int id, Rect rect, const wchar_t *label,
                                const wchar_t *tooltip, IconKind icon,
                                ControlStyle style) {
    LumaControl *c;
    if (g_app.control_count >= MAX_CONTROLS) return NULL;
    c = &g_app.controls[g_app.control_count++];
    memset(c, 0, sizeof(*c));
    c->id = id;
    c->rect = rect;
    c->label = label;
    c->tooltip = tooltip;
    c->icon = icon;
    c->style = style;
    c->enabled = true;
    c->ratio_index = -1;
    return c;
}

static LumaControl *find_control(int id) {
    int i;
    for (i = 0; i < g_app.control_count; ++i) {
        if (g_app.controls[i].id == id) return &g_app.controls[i];
    }
    return NULL;
}

static void layout_app(void) {
    float w = g_app.width;
    float h = g_app.height;
    float gap;
    float cw;
    float x;
    float y;
    float content_start;
    float content_end;
    float viewport_h;
    float new_scroll;
    float delta;
    int i;
    LumaControl *c;

    g_app.compact = w < 820.0f;
    g_app.wide = w >= 1180.0f;
    g_app.sidebar_w = g_app.compact ? 96.0f : (g_app.wide ? 320.0f : 272.0f);
    g_app.margin = g_app.compact ? 16.0f : 20.0f;
    g_app.sidebar = rect_make(0.0f, 0.0f, g_app.sidebar_w, h);
    g_app.appbar = rect_make(g_app.sidebar_w, 0.0f, w, 72.0f);
    g_app.canvas = rect_make(g_app.sidebar_w, 72.0f, w, h);
    g_app.footer_h = g_app.compact ? 60.0f : 68.0f;
    g_app.footer_top = h - g_app.footer_h;
    g_app.crop_panel_visible = g_app.crop_mode && g_app.has_image;
    g_app.crop_panel_height = g_app.compact ? 118.0f : 108.0f;
    g_app.crop_panel_top = g_app.footer_top - g_app.crop_panel_height;
    if (!g_app.crop_panel_visible) g_app.crop_panel_top = g_app.footer_top;
    g_app.actions_y = 136.0f;
    g_app.tools_top = g_app.compact ? 134.0f
                                   : (g_app.actions_y + 44.0f + 16.0f);
    g_app.tools_bottom = (g_app.crop_panel_visible ? g_app.crop_panel_top
                                                  : g_app.footer_top) -
                         8.0f;
    if (g_app.tools_bottom < g_app.tools_top + 40.0f) {
        g_app.tools_bottom = g_app.tools_top + 40.0f;
    }
    g_app.view_pad = g_app.compact ? 24.0f : 48.0f;
    g_app.control_count = 0;
    g_app.actions_pinned = !g_app.compact;

    cw = g_app.sidebar_w - g_app.margin * 2.0f;
    x = g_app.margin;

    c = add_control(CTRL_OPEN,
                    rect_make(x, g_app.compact ? 70.0f : 76.0f, x + cw,
                              (g_app.compact ? 70.0f : 76.0f) + 50.0f),
                    L"打开图片", L"打开图片  (Ctrl+O)", ICON_OPEN,
                    STYLE_PRIMARY);
    if (c) {
        c->enabled = !g_app.loading;
        c->icon_only = true;
    }

    if (!g_app.compact) {
        float bw = (cw - 24.0f) / 4.0f;
        c = add_control(CTRL_UNDO,
                        rect_make(x, g_app.actions_y, x + bw,
                                  g_app.actions_y + 44.0f),
                        L"撤销", L"撤销  (Ctrl+Z)", ICON_UNDO, STYLE_ACTIONS);
        if (c) c->enabled = g_app.undo_count > 0;
        c = add_control(CTRL_REDO,
                        rect_make(x + bw + 8.0f, g_app.actions_y,
                                  x + bw * 2.0f + 8.0f,
                                  g_app.actions_y + 44.0f),
                        L"重做", L"重做  (Ctrl+Y)", ICON_REDO, STYLE_ACTIONS);
        if (c) c->enabled = g_app.redo_count > 0;
        c = add_control(CTRL_EXPORT,
                        rect_make(x + (bw + 8.0f) * 2.0f, g_app.actions_y,
                                  x + (bw + 8.0f) * 2.0f + bw,
                                  g_app.actions_y + 44.0f),
                        L"导出", L"导出图片  (Ctrl+S)", ICON_EXPORT,
                        STYLE_ACTIONS);
        if (c) c->enabled = g_app.has_image && !g_app.loading;
        c = add_control(CTRL_COPY,
                        rect_make(x + (bw + 8.0f) * 3.0f, g_app.actions_y,
                                  x + cw, g_app.actions_y + 44.0f),
                        L"复制", L"复制到原图旁边（不覆盖原图）", ICON_COPY,
                        STYLE_ACTIONS);
        if (c) c->enabled = g_app.has_image && !g_app.loading;
    }

    /* Tool content, placed inside the scrollable viewport. */
    y = g_app.tools_top - g_app.scroll_y + 2.0f;
    content_start = y;
    gap = g_app.compact ? 8.0f : 10.0f;

    if (g_app.compact) {
        for (i = 0; i < 4; ++i) {
            const wchar_t *label = i == 0 ? L"撤销"
                                : i == 1 ? L"重做"
                                : i == 2 ? L"导出"
                                         : L"复制";
            const wchar_t *tip = i == 0 ? L"撤销  (Ctrl+Z)"
                              : i == 1 ? L"重做  (Ctrl+Y)"
                              : i == 2 ? L"导出图片  (Ctrl+S)"
                                       : L"复制到原图旁边（不覆盖原图）";
            IconKind icon = i == 0 ? ICON_UNDO
                          : i == 1 ? ICON_REDO
                          : i == 2 ? ICON_EXPORT
                                   : ICON_COPY;
            int id = i == 0 ? CTRL_UNDO
                   : i == 1 ? CTRL_REDO
                   : i == 2 ? CTRL_EXPORT
                            : CTRL_COPY;
            c = add_control(id, rect_make(x, y, x + cw, y + 44.0f), label, tip,
                            icon, STYLE_ACTIONS);
            if (c) {
                c->scrolled = true;
                if (c->id == CTRL_UNDO) c->enabled = g_app.undo_count > 0;
                if (c->id == CTRL_REDO) c->enabled = g_app.redo_count > 0;
                if (c->id == CTRL_EXPORT || c->id == CTRL_COPY) {
                    c->enabled = g_app.has_image && !g_app.loading;
                }
            }
            y += 44.0f + gap;
        }
        y += 2.0f;
        g_app.rotation_header_y = y;
        y += 28.0f;
    } else {
        g_app.rotation_header_y = y;
        y += 30.0f;
    }

    {
        int cols = g_app.compact ? 1 : 2;
        int rows = (6 + cols - 1) / cols;
        float card_w = (cw - gap * (float)(cols - 1)) / (float)cols;
        float card_h = g_app.compact ? 46.0f : 50.0f;
        int ids[6] = {CTRL_ROT_L90, CTRL_ROT_R90, CTRL_ROT_L180,
                      CTRL_ROT_R180, CTRL_FLIP_H, CTRL_FLIP_V};
        IconKind icons[6] = {ICON_ROTATE_L90, ICON_ROTATE_R90,
                             ICON_ROTATE_L180, ICON_ROTATE_R180,
                             ICON_FLIP_H, ICON_FLIP_V};
        const wchar_t *labels[6] = {L"90°", L"90°", L"180°", L"180°",
                                    L"水平镜像", L"垂直镜像"};
        const wchar_t *tips[6] = {L"向左旋转 90°", L"向右旋转 90°",
                                  L"向左旋转 180°", L"向右旋转 180°",
                                  L"水平镜像  (H)", L"垂直镜像  (Shift+H)"};
        g_app.rotation_cards_y = y;
        for (i = 0; i < 6; ++i) {
            int col = i % cols;
            int row = i / cols;
            Rect r = rect_make(x + (card_w + gap) * (float)col,
                               y + (card_h + gap) * (float)row,
                               x + (card_w + gap) * (float)col + card_w,
                               y + (card_h + gap) * (float)row + card_h);
            c = add_control(ids[i], r, labels[i], tips[i], icons[i], STYLE_CARD);
            if (c) {
                c->scrolled = true;
                c->enabled = g_app.has_image && !g_app.loading &&
                             !g_app.crop_mode;
                if (ids[i] == CTRL_FLIP_H) c->selected = g_app.mirror_h;
                if (ids[i] == CTRL_FLIP_V) c->selected = g_app.mirror_v;
                if (i >= 4) c->icon_only = true;
            }
        }
        y += card_h * (float)rows + gap * (float)(rows - 1) + 8.0f;
    }

    {
        Rect divider = rect_make(x + 2.0f, y + 10.0f, x + cw - 2.0f, y + 10.0f);
        g_app.crop_divider_y = rect_cy(divider);
    }
    g_app.crop_header_y = y + 22.0f;
    y = g_app.crop_header_y + 28.0f;

    {
        int cols = g_app.compact ? 1 : (g_app.wide ? 3 : 2);
        float card_w = (cw - gap * (float)(cols - 1)) / (float)cols;
        float card_h = g_app.compact ? 46.0f : 52.0f;
        g_app.crop_cards_y = y;
        for (i = 0; i < 8; ++i) {
            int col = i % cols;
            int row = i / cols;
            Rect r = rect_make(x + (card_w + gap) * (float)col,
                               y + (card_h + gap) * (float)row,
                               x + (card_w + gap) * (float)col + card_w,
                               y + (card_h + gap) * (float)row + card_h);
            c = add_control(CTRL_RATIO_16_9 + i, r, RATIOS[i].label,
                            RATIOS[i].tooltip, ICON_NONE, STYLE_RATIO);
            if (c) {
                c->scrolled = true;
                c->ratio_index = i;
                c->aspect = RATIOS[i].aspect;
                c->selected = g_app.crop_mode && g_app.crop_preset == i;
                c->enabled = g_app.has_image && !g_app.loading;
            }
        }
        y += card_h * (float)((8 + cols - 1) / cols) + gap *
                 (float)(((8 + cols - 1) / cols) - 1);
        y += g_app.compact ? 8.0f : 10.0f;
    }

    g_app.crop_hint_y = y;
    y += 12.0f;
    content_end = y;

    g_app.content_height = content_end - content_start;
    viewport_h = g_app.tools_bottom - g_app.tools_top;
    g_app.scroll_max = g_app.content_height - viewport_h;
    if (g_app.scroll_max < 0.0f) g_app.scroll_max = 0.0f;
    new_scroll = clampf(g_app.scroll_y, 0.0f, g_app.scroll_max);
    delta = new_scroll - g_app.scroll_y;
    g_app.scroll_y = new_scroll;
    if (delta != 0.0f) {
        for (i = 0; i < g_app.control_count; ++i) {
            if (g_app.controls[i].scrolled) {
                g_app.controls[i].rect.top -= delta;
                g_app.controls[i].rect.bottom -= delta;
            }
        }
        g_app.rotation_header_y -= delta;
        g_app.rotation_cards_y -= delta;
        g_app.crop_header_y -= delta;
        g_app.crop_divider_y -= delta;
        g_app.crop_cards_y -= delta;
        g_app.crop_hint_y -= delta;
    }

    if (g_app.crop_panel_visible) {
        float by = g_app.crop_panel_top + (g_app.compact ? 30.0f : 40.0f);
        float bh = g_app.compact ? 36.0f : 44.0f;
        if (g_app.compact) {
            c = add_control(CTRL_APPLY_CROP,
                            rect_make(x, by, x + cw, by + bh),
                            L"应用裁剪", L"应用裁剪  (Enter)", ICON_CHECK,
                            STYLE_PRIMARY);
            if (c) {
                c->enabled = g_app.has_image && !g_app.loading;
                c->icon_only = true;
            }
            c = add_control(CTRL_CANCEL_CROP,
                            rect_make(x, by + bh + 8.0f, x + cw,
                                      by + bh * 2.0f + 8.0f),
                            L"取消", L"取消裁剪  (Esc)", ICON_CLOSE,
                            STYLE_TONAL);
            if (c) {
                c->enabled = g_app.has_image && !g_app.loading;
                c->icon_only = true;
            }
        } else {
            float aw = cw * 0.56f;
            c = add_control(CTRL_APPLY_CROP,
                            rect_make(x, by, x + aw, by + bh),
                            L"应用裁剪", L"应用裁剪  (Enter)", ICON_CHECK,
                            STYLE_PRIMARY);
            if (c) {
                c->enabled = g_app.has_image && !g_app.loading;
                c->icon_only = true;
            }
            c = add_control(CTRL_CANCEL_CROP,
                            rect_make(x + aw + 8.0f, by, x + cw, by + bh),
                            L"取消", L"取消裁剪  (Esc)", ICON_CLOSE,
                            STYLE_TONAL);
            if (c) {
                c->enabled = g_app.has_image && !g_app.loading;
                c->icon_only = true;
            }
        }
    }

    {
        float fh = g_app.height - g_app.footer_top;
        float tw = g_app.compact ? 44.0f : 48.0f;
        float th = 44.0f;
        g_app.theme_button_y = g_app.footer_top + (fh - th) * 0.5f;
        c = add_control(CTRL_THEME,
                        rect_make((g_app.sidebar_w - tw) * 0.5f,
                                  g_app.theme_button_y,
                                  (g_app.sidebar_w + tw) * 0.5f,
                                  g_app.theme_button_y + th),
                        g_app.dark ? L"浅色外观" : L"深色外观",
                        L"切换深色 / 浅色外观  (T)",
                        g_app.dark ? ICON_SUN : ICON_MOON, STYLE_THEME);
        if (c) c->icon_only = true;
    }
    (void)c;
}

static void rotated_pixel_size(int *out_w, int *out_h) {
    int w = (int)g_app.image.width;
    int h = (int)g_app.image.height;
    if (g_app.rotation == 1 || g_app.rotation == 3) {
        *out_w = h;
        *out_h = w;
    } else {
        *out_w = w;
        *out_h = h;
    }
}

static void crop_display_size(float *out_w, float *out_h) {
    int rw;
    int rh;
    rotated_pixel_size(&rw, &rh);
    *out_w = (float)rw * (g_app.crop.right - g_app.crop.left);
    *out_h = (float)rh * (g_app.crop.bottom - g_app.crop.top);
    if (*out_w < 1.0f) *out_w = 1.0f;
    if (*out_h < 1.0f) *out_h = 1.0f;
}

static void update_image_view(void) {
    float avail_w;
    float avail_h;
    float src_w;
    float src_h;
    float fit;
    float disp_w;
    float disp_h;
    float cx;
    float cy;
    float slack_x;
    float slack_y;
    float limit_x;
    float limit_y;
    int rw;
    int rh;

    if (!g_app.has_image || (!g_app.image_visible && g_app.image.pixels == NULL)) {
        g_app.image_visible = false;
        return;
    }
    avail_w = rect_w(g_app.canvas) - g_app.view_pad * 2.0f;
    avail_h = rect_h(g_app.canvas) - g_app.view_pad * 2.0f;
    if (avail_w < 80.0f) avail_w = 80.0f;
    if (avail_h < 80.0f) avail_h = 80.0f;

    rotated_pixel_size(&rw, &rh);
    if (g_app.crop_mode) {
        src_w = (float)rw;
        src_h = (float)rh;
    } else {
        crop_display_size(&src_w, &src_h);
    }
    fit = fminf(avail_w / src_w, avail_h / src_h);
    fit = clampf(fit, 0.01f, 4.0f);
    g_app.fit_scale = fit;
    g_app.zoom = clampf(g_app.zoom, 0.1f, 16.0f);
    g_app.display_scale = fit * g_app.zoom;
    disp_w = src_w * g_app.display_scale;
    disp_h = src_h * g_app.display_scale;

    slack_x = fmaxf(0.0f, (disp_w - avail_w) * 0.5f);
    slack_y = fmaxf(0.0f, (disp_h - avail_h) * 0.5f);
    limit_x = slack_x + fmaxf(36.0f, avail_w * 0.18f);
    limit_y = slack_y + fmaxf(36.0f, avail_h * 0.18f);
    g_app.pan.x = clampf(g_app.pan.x, -limit_x, limit_x);
    g_app.pan.y = clampf(g_app.pan.y, -limit_y, limit_y);

    cx = rect_cx(g_app.canvas) + g_app.pan.x;
    cy = rect_cy(g_app.canvas) + g_app.pan.y;
    g_app.image_rect = rect_make(cx - disp_w * 0.5f, cy - disp_h * 0.5f,
                                 cx + disp_w * 0.5f, cy + disp_h * 0.5f);
    g_app.image_visible = true;
}

static void set_fit_view(void) {
    g_app.zoom = 1.0f;
    g_app.pan = point_make(0.0f, 0.0f);
    update_image_view();
}

static void zoom_view(float factor, float mouse_x, float mouse_y) {
    float old_scale;
    float new_zoom;
    float new_scale;
    float cx;
    float cy;
    float rel_x = 0.0f;
    float rel_y = 0.0f;
    float new_cx;
    float new_cy;

    if (!g_app.has_image) return;
    if (mouse_x < g_app.canvas.left || mouse_x > g_app.canvas.right ||
        mouse_y < g_app.canvas.top || mouse_y > g_app.canvas.bottom) {
        mouse_x = rect_cx(g_app.canvas);
        mouse_y = rect_cy(g_app.canvas);
    }
    old_scale = g_app.display_scale;
    new_zoom = clampf(g_app.zoom * factor, 0.1f, 16.0f);
    if (fabsf(new_zoom - g_app.zoom) < 0.0001f) return;
    if (old_scale > 0.0f) {
        cx = rect_cx(g_app.canvas) + g_app.pan.x;
        cy = rect_cy(g_app.canvas) + g_app.pan.y;
        rel_x = (mouse_x - cx) / old_scale;
        rel_y = (mouse_y - cy) / old_scale;
    }
    g_app.zoom = new_zoom;
    g_app.display_scale = g_app.fit_scale * g_app.zoom;
    new_scale = g_app.display_scale;
    new_cx = mouse_x - rel_x * new_scale;
    new_cy = mouse_y - rel_y * new_scale;
    g_app.pan.x = new_cx - rect_cx(g_app.canvas);
    g_app.pan.y = new_cy - rect_cy(g_app.canvas);
    update_image_view();
}

static void draw_keycap(Rect r, const wchar_t *text) {
    fill_rounded(r, 6.0f, g_app.theme.container);
    stroke_rounded(r, 6.0f, color_alpha(g_app.theme.outline_variant, 0.8f), 1.0f,
                   NULL);
    draw_text_raw(text, g_app.f_tiny, r, g_app.theme.on_surface_variant, 1, 1,
                  false, true);
}

static void draw_section_header(float y, IconKind icon, Color accent) {
    float x = g_app.margin;
    float cw = g_app.sidebar_w - g_app.margin * 2.0f;
    draw_icon(icon, rect_make(x, y, x + 20.0f, y + 20.0f), accent);
    line(x + 28.0f, y + 10.0f, x + cw, y + 10.0f,
         color_alpha(g_app.theme.outline_variant, 0.7f), 1.0f);
}

static void draw_aspect_preview(Rect area, float aspect, bool selected,
                                bool enabled) {
    float max_w = rect_w(area);
    float max_h = rect_h(area);
    float w;
    float h;
    Rect r;
    Color outline = selected ? g_app.theme.primary
                             : color_alpha(g_app.theme.primary, 0.62f);
    if (!enabled) {
        outline = color_alpha(g_app.theme.on_surface_variant, 0.38f);
    }
    if (aspect <= 0.0f) {
        w = fminf(max_w, 28.0f);
        h = fminf(max_h, 20.0f);
        r = rect_make(rect_cx(area) - w * 0.5f, rect_cy(area) - h * 0.5f,
                      rect_cx(area) + w * 0.5f, rect_cy(area) + h * 0.5f);
        stroke_rounded(r, 4.0f, outline, 1.3f, g_app.dash_style);
        line(r.left + 4.0f, r.bottom - 4.0f, r.right - 4.0f, r.top + 4.0f,
             color_alpha(outline, 0.7f), 1.0f);
        return;
    }
    if (aspect >= max_w / max_h) {
        w = max_w;
        h = w / aspect;
    } else {
        h = max_h;
        w = h * aspect;
    }
    w = fmaxf(w, 10.0f);
    h = fmaxf(h, 10.0f);
    r = rect_make(rect_cx(area) - w * 0.5f, rect_cy(area) - h * 0.5f,
                  rect_cx(area) + w * 0.5f, rect_cy(area) + h * 0.5f);
    if (selected) {
        fill_rounded(r, 2.5f, color_alpha(g_app.theme.primary, 0.13f));
    }
    stroke_rounded(r, 2.5f, outline, selected ? 1.7f : 1.3f, NULL);
}

static void draw_control(LumaControl *c) {
    LumaTheme *t = &g_app.theme;
    Color fill;
    Color border;
    Color text;
    Color icon;
    Color overlay;
    float radius;
    bool hover;
    bool pressed;
    bool disabled;
    int align;

    if (c == NULL) return;
    hover = g_app.hover_id == c->id && c->enabled;
    pressed = hover && g_app.pressed_id == c->id && g_app.mouse_down;
    disabled = !c->enabled;
    overlay = color_alpha(t->on_surface, 0.0f);
    border = color_alpha(t->outline_variant, 0.0f);
    fill = t->container_lowest;
    text = t->on_surface;
    icon = t->primary;
    radius = 14.0f;
    align = 0;

    switch (c->style) {
    case STYLE_PRIMARY:
        fill = t->primary;
        text = t->on_primary;
        icon = t->on_primary;
        overlay = t->on_primary;
        radius = c->id == CTRL_OPEN ? 17.0f : 14.0f;
        break;
    case STYLE_TONAL:
        fill = color_alpha(t->container_high, g_app.dark ? 0.6f : 0.55f);
        border = color_alpha(t->outline_variant, 0.5f);
        text = t->on_surface;
        icon = t->primary;
        overlay = t->on_surface;
        radius = 14.0f;
        break;
    case STYLE_ACTIONS:
        if (c->selected) {
            fill = t->primary_container;
            border = t->primary;
            text = t->on_primary_container;
            icon = t->on_primary_container;
            overlay = t->primary;
        } else {
            fill = color_alpha(t->container, g_app.dark ? 0.9f : 0.85f);
            border = color_alpha(t->outline_variant, 0.42f);
            text = t->on_surface;
            icon = t->primary;
            overlay = t->on_surface;
        }
        radius = 13.0f;
        break;
    case STYLE_CARD:
        fill = c->selected ? t->primary_container : t->container_lowest;
        border = c->selected ? t->primary
                             : color_alpha(t->outline_variant, 0.62f);
        text = c->selected ? t->on_primary_container : t->on_surface;
        icon = c->selected ? t->on_primary_container : t->primary;
        overlay = c->selected ? t->primary : t->on_surface;
        radius = 15.0f;
        break;
    case STYLE_RATIO:
        fill = c->selected ? t->primary_container : color_alpha(t->surface, 0.0f);
        border = c->selected ? t->primary
                             : color_alpha(t->outline_variant, 0.0f);
        text = c->selected ? t->on_primary_container : t->on_surface;
        icon = c->selected ? t->on_primary_container : t->primary;
        overlay = c->selected ? t->primary : t->on_surface;
        radius = 14.0f;
        break;
    case STYLE_THEME:
        fill = color_alpha(t->container, g_app.dark ? 0.85f : 0.9f);
        border = color_alpha(t->outline_variant, 0.48f);
        text = t->on_surface;
        icon = t->primary;
        overlay = t->on_surface;
        radius = 15.0f;
        break;
    }

    if (disabled) {
        if (c->style != STYLE_RATIO) {
            fill = color_mix(fill, t->surface, 0.48f);
        }
        border = color_alpha(border, 0.35f);
        text = color_alpha(text, 0.38f);
        icon = color_alpha(icon, 0.38f);
    }

    fill_rounded(c->rect, radius, fill);
    if (border.a > 0.01f) {
        stroke_rounded(rect_inset(c->rect, 0.5f, 0.5f), radius - 0.5f, border,
                       c->selected ? 1.6f : 1.0f, NULL);
    }
    if (c->enabled && (hover || pressed)) {
        RoundClip clip = push_round_clip(c->rect, radius);
        fill_rounded(c->rect, radius,
                     color_alpha(overlay, pressed ? 0.11f : 0.06f));
        pop_round_clip(&clip);
    }

    if (c->style == STYLE_CARD) {
        if (c->icon_only) {
            draw_icon(c->icon,
                      rect_make(rect_cx(c->rect) - 10.0f,
                                rect_cy(c->rect) - 10.0f,
                                rect_cx(c->rect) + 10.0f,
                                rect_cy(c->rect) + 10.0f),
                      icon);
        } else if (g_app.compact) {
            float iy = c->rect.top + 4.0f;
            draw_icon(c->icon,
                      rect_make(rect_cx(c->rect) - 9.0f, iy,
                                rect_cx(c->rect) + 9.0f, iy + 18.0f),
                      icon);
            draw_text_raw(c->label, g_app.f_tiny,
                          rect_make(c->rect.left + 2.0f, c->rect.top + 25.0f,
                                    c->rect.right - 2.0f, c->rect.bottom - 2.0f),
                          text, 1, 1, false, true);
        } else {
            float icon_size = 22.0f;
            float gap = 8.0f;
            float tw = text_width(c->label, g_app.f_label);
            float start = rect_cx(c->rect) - (icon_size + gap + tw) * 0.5f;
            draw_icon(c->icon,
                      rect_make(start, rect_cy(c->rect) - 11.0f,
                                start + icon_size, rect_cy(c->rect) + 11.0f),
                      icon);
            draw_text_raw(c->label, g_app.f_label,
                          rect_make(start + icon_size + gap, c->rect.top + 4.0f,
                                    start + icon_size + gap + tw + 2.0f,
                                    c->rect.bottom - 4.0f),
                          text, 0, 1, false, true);
        }
    } else if (c->style == STYLE_RATIO) {
        Rect preview;
        Rect label;
        if (g_app.compact) {
            preview = rect_make(c->rect.left + 5.0f, c->rect.top + 5.0f,
                                c->rect.right - 5.0f, c->rect.top + 26.0f);
            label = rect_make(c->rect.left + 2.0f, c->rect.top + 26.0f,
                              c->rect.right - 2.0f, c->rect.bottom - 2.0f);
        } else {
            preview = rect_make(c->rect.left + 8.0f, c->rect.top + 7.0f,
                                c->rect.right - 8.0f, c->rect.top + 38.0f);
            label = rect_make(c->rect.left + 2.0f, c->rect.top + 38.0f,
                              c->rect.right - 2.0f, c->rect.bottom - 2.0f);
        }
        draw_aspect_preview(preview, c->aspect, c->selected, c->enabled);
        draw_text_raw(c->label, g_app.compact ? g_app.f_tiny : g_app.f_caption,
                      label, text, 1, 1, false, true);
        if (c->selected) {
            Rect badge = rect_make(c->rect.right - 17.0f, c->rect.top + 5.0f,
                                   c->rect.right - 5.0f, c->rect.top + 17.0f);
            fill_ellipse(rect_cx(badge), rect_cy(badge), 6.0f, 6.0f, t->primary);
            draw_icon(ICON_CHECK,
                      rect_make(rect_cx(badge) - 4.0f, rect_cy(badge) - 4.0f,
                                rect_cx(badge) + 4.0f, rect_cy(badge) + 4.0f),
                      t->on_primary);
        }
    } else {
        bool icon_only = c->icon_only || c->style == STYLE_ACTIONS ||
                         (g_app.compact &&
                          (c->style == STYLE_PRIMARY ||
                           c->style == STYLE_TONAL));
        if (icon_only && c->id == CTRL_OPEN && !g_app.compact) {
            float icon_box = 22.0f;
            float icon_left = c->rect.left + 17.0f;
            float bw = text_width(L"Ctrl O", g_app.f_tiny) + 14.0f;
            Rect badge_rect = rect_make(c->rect.right - bw - 12.0f,
                                        rect_cy(c->rect) - 11.0f,
                                        c->rect.right - 12.0f,
                                        rect_cy(c->rect) + 11.0f);
            draw_icon(c->icon,
                      rect_make(icon_left, rect_cy(c->rect) - icon_box * 0.5f,
                                icon_left + icon_box,
                                rect_cy(c->rect) + icon_box * 0.5f),
                      icon);
            fill_rounded(badge_rect, 7.0f,
                         color_alpha(t->on_primary, 0.14f));
            draw_text_raw(L"Ctrl O", g_app.f_tiny, badge_rect,
                          color_alpha(t->on_primary, 0.95f), 1, 1, false, true);
        } else if (icon_only) {
            float icon_box = c->style == STYLE_PRIMARY ? 22.0f : 20.0f;
            draw_icon(c->icon,
                      rect_make(rect_cx(c->rect) - icon_box * 0.5f,
                                rect_cy(c->rect) - icon_box * 0.5f,
                                rect_cx(c->rect) + icon_box * 0.5f,
                                rect_cy(c->rect) + icon_box * 0.5f),
                      icon);
        } else {
            float icon_box = c->style == STYLE_PRIMARY && c->id == CTRL_OPEN
                                 ? 22.0f
                                 : 20.0f;
            float icon_left = c->rect.left + 14.0f;
            float label_left = icon_left + icon_box + 11.0f;
            float label_right = c->rect.right - 12.0f;
            draw_icon(c->icon,
                      rect_make(icon_left, rect_cy(c->rect) - icon_box * 0.5f,
                                icon_left + icon_box,
                                rect_cy(c->rect) + icon_box * 0.5f),
                      icon);
            draw_text_raw(c->label,
                          c->style == STYLE_PRIMARY ? g_app.f_button
                                                    : g_app.f_label,
                          rect_make(label_left, c->rect.top + 4.0f, label_right,
                                    c->rect.bottom - 4.0f),
                          text, align, 1, false, true);
        }
    }

    if (g_app.focus_visible && g_app.focus_id == c->id && c->enabled) {
        stroke_rounded(rect_inset(c->rect, -2.0f, -2.0f), radius + 2.0f,
                       g_app.theme.primary, 2.0f, NULL);
    }
}

static void draw_scrollbar(void) {
    float track_top;
    float track_bottom;
    float viewport;
    float thumb_h;
    float thumb_y;
    float ratio;
    if (g_app.scroll_max <= 0.5f) return;
    track_top = g_app.tools_top + 8.0f;
    track_bottom = g_app.tools_bottom - 8.0f;
    viewport = track_bottom - track_top;
    if (viewport < 20.0f) return;
    thumb_h = viewport * viewport / g_app.content_height;
    thumb_h = clampf(thumb_h, 34.0f, viewport);
    ratio = g_app.scroll_y / g_app.scroll_max;
    thumb_y = track_top + (viewport - thumb_h) * ratio;
    fill_rounded(rect_make(g_app.sidebar_w - 6.0f, track_top,
                           g_app.sidebar_w - 3.0f, track_bottom),
                 1.5f, color_alpha(g_app.theme.on_surface, 0.06f));
    fill_rounded(rect_make(g_app.sidebar_w - 6.0f, thumb_y,
                           g_app.sidebar_w - 3.0f, thumb_y + thumb_h),
                 1.5f, color_alpha(g_app.theme.on_surface, 0.22f));
}

static void paint_brand(void) {
    LumaTheme *t = &g_app.theme;
    Rect tile;
    if (g_app.compact) {
        tile = rect_make((g_app.sidebar_w - 40.0f) * 0.5f, 18.0f,
                         (g_app.sidebar_w + 40.0f) * 0.5f, 58.0f);
        draw_logo_mark(tile);
        return;
    }
    tile = rect_make(g_app.margin, 18.0f, g_app.margin + 40.0f, 58.0f);
    draw_logo_mark(tile);
    draw_text_raw(L"Fustimage", g_app.f_brand,
                  rect_make(tile.right + 12.0f, 17.0f,
                            g_app.sidebar_w - g_app.margin, 55.0f),
                  t->on_surface, 0, 1, false, true);
}

static void paint_crop_panel_title(void) {
    LumaTheme *t = &g_app.theme;
    const wchar_t *ratio = (g_app.crop_preset >= 0 &&
                            g_app.crop_preset < 8)
                               ? RATIOS[g_app.crop_preset].label
                               : L"∞";
    float x = g_app.margin;
    float cw = g_app.sidebar_w - g_app.margin * 2.0f;
    Color pill_bg = color_alpha(t->primary_container,
                                g_app.dark ? 0.75f : 0.9f);
    Color pill_text = t->on_primary_container;
    float tw = text_width(ratio, g_app.f_tiny) + 14.0f;
    float y = g_app.crop_panel_top + (g_app.compact ? 8.0f : 12.0f);
    Rect pill;

    draw_icon(ICON_CROP, rect_make(x, y + (g_app.compact ? 1.0f : 2.0f),
                                   x + 18.0f,
                                   y + (g_app.compact ? 19.0f : 20.0f)),
              t->primary);
    pill = rect_make(x + cw - tw, y, x + cw,
                     y + (g_app.compact ? 18.0f : 22.0f));
    fill_rounded(pill, g_app.compact ? 9.0f : 11.0f, pill_bg);
    draw_text_raw(ratio, g_app.f_tiny, pill, pill_text, 1, 1, false, true);
}

static void paint_footer(void) {
    LumaTheme *t = &g_app.theme;
    Rect bg = rect_make(0.0f, g_app.footer_top, g_app.sidebar_w, g_app.height);
    fill_rect(bg, color_mix(t->container_low, t->container, 0.55f));
    line(0.0f, g_app.footer_top + 0.5f, g_app.sidebar_w,
         g_app.footer_top + 0.5f, color_alpha(t->outline_variant, 0.55f), 1.0f);
}

static void paint_sidebar(void) {
    LumaTheme *t = &g_app.theme;
    Rect clip;
    int i;
    LumaControl *open;
    LumaControl *apply;
    LumaControl *cancel;
    LumaControl *theme;
    LumaControl *undo;
    LumaControl *redo;
    LumaControl *exportc;
    LumaControl *copy;

    fill_rect(g_app.sidebar, t->container_low);

    paint_brand();

    open = find_control(CTRL_OPEN);
    if (open) draw_control(open);

    undo = find_control(CTRL_UNDO);
    redo = find_control(CTRL_REDO);
    exportc = find_control(CTRL_EXPORT);
    copy = find_control(CTRL_COPY);
    if (!g_app.compact) {
        if (undo) draw_control(undo);
        if (redo) draw_control(redo);
        if (exportc) draw_control(exportc);
        if (copy) draw_control(copy);
    }

    if (g_app.crop_panel_visible) {
        Rect panel = rect_make(0.0f, g_app.crop_panel_top, g_app.sidebar_w,
                               g_app.footer_top);
        fill_rect(panel, color_mix(t->container_low, t->container_high, 0.35f));
        line(0.0f, g_app.crop_panel_top + 0.5f, g_app.sidebar_w,
             g_app.crop_panel_top + 0.5f,
             color_alpha(t->outline_variant, 0.65f), 1.0f);
        paint_crop_panel_title();
    }

    paint_footer();

    clip = rect_make(0.0f, g_app.tools_top, g_app.sidebar_w, g_app.tools_bottom);
    if (clip.bottom > clip.top) {
        g_app.rt->lpVtbl->PushAxisAlignedClip(g_app.rt, &clip,
                                              D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        draw_section_header(g_app.rotation_header_y, ICON_ROTATE_R90, t->primary);
        line(g_app.margin + 2.0f, g_app.crop_divider_y,
             g_app.sidebar_w - g_app.margin - 2.0f,
             g_app.crop_divider_y,
             color_alpha(t->outline_variant, 0.9f), 1.0f);
        draw_section_header(g_app.crop_header_y, ICON_CROP, t->tertiary);
        for (i = 0; i < g_app.control_count; ++i) {
            if (g_app.controls[i].scrolled) draw_control(&g_app.controls[i]);
        }
        g_app.rt->lpVtbl->PopAxisAlignedClip(g_app.rt);
    }

    draw_scrollbar();

    if (g_app.crop_panel_visible) {
        apply = find_control(CTRL_APPLY_CROP);
        cancel = find_control(CTRL_CANCEL_CROP);
        if (apply) draw_control(apply);
        if (cancel) draw_control(cancel);
    }
    theme = find_control(CTRL_THEME);
    if (theme) draw_control(theme);

    line(g_app.sidebar_w - 0.5f, 0.0f, g_app.sidebar_w - 0.5f,
         g_app.height, color_alpha(t->outline_variant, 0.7f), 1.0f);
}

static void draw_status_pill(Rect r, IconKind icon, Color bg, Color fg) {
    fill_rounded(r, 13.0f, bg);
    draw_icon(icon, rect_make(rect_cx(r) - 7.0f, rect_cy(r) - 7.0f,
                              rect_cx(r) + 7.0f, rect_cy(r) + 7.0f),
              fg);
}

static void paint_appbar(void) {
    LumaTheme *t = &g_app.theme;
    wchar_t subtitle[256];
    wchar_t title[200];
    IconKind status_icon;
    Color status_bg;
    Color status_fg;
    float status_w;
    Rect status;
    Rect tile;
    float text_left;

    fill_rect(g_app.appbar, t->surface);
    line(g_app.appbar.left, g_app.appbar.bottom - 0.5f, g_app.appbar.right,
         g_app.appbar.bottom - 0.5f, color_alpha(t->outline_variant, 0.6f),
         1.0f);

    tile = rect_make(g_app.appbar.left + (g_app.compact ? 18.0f : 24.0f),
                     17.0f,
                     g_app.appbar.left + (g_app.compact ? 56.0f : 62.0f), 55.0f);
    fill_rounded(tile, 12.0f,
                 color_alpha(t->primary_container, g_app.dark ? 0.8f : 1.0f));
    draw_icon(ICON_IMAGE,
              rect_make(rect_cx(tile) - 11.0f, rect_cy(tile) - 11.0f,
                        rect_cx(tile) + 11.0f, rect_cy(tile) + 11.0f),
              t->on_primary_container);

    if (g_app.loading && !g_app.has_image) {
        copy_utf16(title, 200, g_app.loading_name[0] ? g_app.loading_name
                                                     : L"Fustimage");
    } else if (g_app.has_image) {
        copy_utf16(title, 200, g_app.display_name[0] ? g_app.display_name
                                                     : L"Fustimage");
    } else {
        title[0] = L'\0';
    }

    if (g_app.has_image) {
        swprintf(subtitle, 256, L"%ls  ·  %ls  ·  %u × %u",
                 g_app.format_name[0] ? g_app.format_name : L"",
                 g_app.size_text[0] ? g_app.size_text : L"",
                 g_app.image.width, g_app.image.height);
    } else {
        subtitle[0] = L'\0';
    }

    if (g_app.crop_mode) {
        status_icon = ICON_CROP;
        status_bg = color_alpha(t->tertiary_container, g_app.dark ? 0.9f : 1.0f);
        status_fg = t->on_tertiary_container;
    } else if (g_app.loading) {
        status_icon = ICON_SPARKLE;
        status_bg = t->container_high;
        status_fg = t->primary;
    } else if (!g_app.has_image) {
        status_icon = ICON_OPEN;
        status_bg = color_alpha(t->container, 0.9f);
        status_fg = t->on_surface_variant;
    } else if (g_app.exported) {
        status_icon = ICON_CHECK;
        status_bg = color_alpha(t->secondary_container, g_app.dark ? 0.85f : 1.0f);
        status_fg = t->on_secondary_container;
    } else if (g_app.edited) {
        status_icon = ICON_EXPORT;
        status_bg = color_alpha(t->tertiary_container, g_app.dark ? 0.85f : 1.0f);
        status_fg = t->on_tertiary_container;
    } else {
        status_icon = ICON_IMAGE;
        status_bg = color_alpha(t->container, 0.95f);
        status_fg = t->on_surface_variant;
    }
    status_w = 38.0f;
    status = rect_make(g_app.appbar.right - (g_app.compact ? 16.0f : 24.0f) -
                           status_w,
                       rect_cy(g_app.appbar) - 14.0f,
                       g_app.appbar.right - (g_app.compact ? 16.0f : 24.0f),
                       rect_cy(g_app.appbar) + 14.0f);
    draw_status_pill(status, status_icon, status_bg, status_fg);

    text_left = tile.right + 14.0f;
    draw_text_trimmed(title, g_app.f_title,
                      rect_make(text_left, 13.0f, status.left - 14.0f, 37.0f),
                      t->on_surface, false);
    draw_text_trimmed(subtitle, g_app.f_caption,
                      rect_make(text_left, 39.0f, status.left - 14.0f, 56.0f),
                      t->on_surface_variant, false);
}

static D2D1_MATRIX_3X2_F luma_identity_matrix(void) {
    D2D1_MATRIX_3X2_F m;
    memset(&m, 0, sizeof(m));
    m._11 = 1.0f;
    m._22 = 1.0f;
    return m;
}

static D2D1_MATRIX_3X2_F luma_matrix_mul(D2D1_MATRIX_3X2_F a,
                                         D2D1_MATRIX_3X2_F b) {
    D2D1_MATRIX_3X2_F r;
    r._11 = a._11 * b._11 + a._12 * b._21;
    r._12 = a._11 * b._12 + a._12 * b._22;
    r._21 = a._21 * b._11 + a._22 * b._21;
    r._22 = a._21 * b._12 + a._22 * b._22;
    r._31 = a._31 * b._11 + a._32 * b._21 + b._31;
    r._32 = a._31 * b._12 + a._32 * b._22 + b._32;
    return r;
}

static void draw_photo(void) {
    LumaTheme *t = &g_app.theme;
    D2D1_MATRIX_3X2_F m;
    D2D1_MATRIX_3X2_F identity;
    D2D1_RECT_F dest;
    D2D1_RECT_F border;
    int rw;
    int rh;
    float s;
    float shown_l;
    float shown_t;
    float ox;
    float oy;
    float bw;
    float bh;
    RoundClip clip;

    if (!g_app.has_image || g_app.bitmap == NULL) return;
    rotated_pixel_size(&rw, &rh);
    s = g_app.display_scale;
    shown_l = g_app.crop_mode ? 0.0f : g_app.crop.left;
    shown_t = g_app.crop_mode ? 0.0f : g_app.crop.top;
    ox = g_app.image_rect.left - shown_l * (float)rw * s;
    oy = g_app.image_rect.top - shown_t * (float)rh * s;
    bw = (float)g_app.image.width;
    bh = (float)g_app.image.height;

    memset(&m, 0, sizeof(m));
    switch (g_app.rotation) {
    case 1:
        m._11 = 0.0f;
        m._12 = s;
        m._21 = -s;
        m._22 = 0.0f;
        m._31 = ox + bh * s;
        m._32 = oy;
        break;
    case 2:
        m._11 = -s;
        m._12 = 0.0f;
        m._21 = 0.0f;
        m._22 = -s;
        m._31 = ox + bw * s;
        m._32 = oy + bh * s;
        break;
    case 3:
        m._11 = 0.0f;
        m._12 = -s;
        m._21 = s;
        m._22 = 0.0f;
        m._31 = ox;
        m._32 = oy + bw * s;
        break;
    default:
        m._11 = s;
        m._12 = 0.0f;
        m._21 = 0.0f;
        m._22 = s;
        m._31 = ox;
        m._32 = oy;
        break;
    }

    if (g_app.mirror_h || g_app.mirror_v) {
        D2D1_MATRIX_3X2_F flip;
        float full_cx = ox + (float)rw * s * 0.5f;
        float full_cy = oy + (float)rh * s * 0.5f;
        flip._11 = g_app.mirror_h ? -1.0f : 1.0f;
        flip._12 = 0.0f;
        flip._21 = 0.0f;
        flip._22 = g_app.mirror_v ? -1.0f : 1.0f;
        flip._31 = g_app.mirror_h ? full_cx * 2.0f : 0.0f;
        flip._32 = g_app.mirror_v ? full_cy * 2.0f : 0.0f;
        m = luma_matrix_mul(m, flip);
    }

    draw_soft_shadow(rect_inset(g_app.image_rect, -4.0f, -4.0f), 22.0f, 0.95f);
    clip = push_round_clip(g_app.image_rect, 18.0f);
    g_app.rt->lpVtbl->SetTransform(g_app.rt, &m);
    dest.left = 0.0f;
    dest.top = 0.0f;
    dest.right = bw;
    dest.bottom = bh;
    g_app.rt->lpVtbl->DrawBitmap(g_app.rt, g_app.bitmap, &dest, 1.0f,
                                 D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                 NULL);
    identity = luma_identity_matrix();
    g_app.rt->lpVtbl->SetTransform(g_app.rt, &identity);
    pop_round_clip(&clip);
    border = g_app.image_rect;
    stroke_rounded(border, 18.0f, color_alpha(t->on_surface, 0.12f), 1.0f, NULL);
}

static void crop_handle_corner(float x, float y, float dx, float dy) {
    Color white = rgba(0xFFFFFF, 0.96f);
    Color shadow = rgba(0x000000, 0.28f);
    float len = 18.0f;
    Rect h = rect_make(dx > 0.0f ? x : x - len,
                       y - 2.0f,
                       dx > 0.0f ? x + len : x,
                       y + 2.0f);
    Rect v = rect_make(x - 2.0f,
                       dy > 0.0f ? y : y - len,
                       x + 2.0f,
                       dy > 0.0f ? y + len : y);
    fill_rounded(rect_offset(h, 0.0f, 1.0f), 2.0f, shadow);
    fill_rounded(rect_offset(v, 1.0f, 0.0f), 2.0f, shadow);
    fill_rounded(h, 2.0f, white);
    fill_rounded(v, 2.0f, white);
}

static void draw_crop_overlay(void) {
    Rect ir = g_app.image_rect;
    Rect cr;
    Rect dim_top;
    Rect dim_bottom;
    Rect dim_left;
    Rect dim_right;
    wchar_t info[96];
    const wchar_t *preset;
    float x1;
    float y1;
    float x2;
    float y2;
    float info_w;
    Rect info_rect;
    int rw;
    int rh;
    Color dim = rgba(0x000000, 0.52f);
    Color white = rgba(0xFFFFFF, 0.92f);
    Color grid = rgba(0xFFFFFF, 0.28f);
    int i;

    if (!g_app.crop_mode || !g_app.image_visible) return;
    x1 = ir.left + rect_w(ir) * g_app.crop.left;
    y1 = ir.top + rect_h(ir) * g_app.crop.top;
    x2 = ir.left + rect_w(ir) * g_app.crop.right;
    y2 = ir.top + rect_h(ir) * g_app.crop.bottom;
    cr = rect_make(x1, y1, x2, y2);

    dim_top = rect_make(ir.left, ir.top, ir.right, y1);
    dim_bottom = rect_make(ir.left, y2, ir.right, ir.bottom);
    dim_left = rect_make(ir.left, y1, x1, y2);
    dim_right = rect_make(x2, y1, ir.right, y2);
    fill_rect(dim_top, dim);
    fill_rect(dim_bottom, dim);
    fill_rect(dim_left, dim);
    fill_rect(dim_right, dim);

    for (i = 1; i <= 2; ++i) {
        float gx = cr.left + rect_w(cr) * (float)i / 3.0f;
        float gy = cr.top + rect_h(cr) * (float)i / 3.0f;
        line(gx, cr.top, gx, cr.bottom, grid, 1.0f);
        line(cr.left, gy, cr.right, gy, grid, 1.0f);
    }
    stroke_rect(rect_inset(cr, -0.5f, -0.5f), rgba(0x000000, 0.28f), 3.0f);
    stroke_rect(cr, white, 1.5f);

    crop_handle_corner(cr.left, cr.top, 1.0f, 1.0f);
    crop_handle_corner(cr.right, cr.top, -1.0f, 1.0f);
    crop_handle_corner(cr.left, cr.bottom, 1.0f, -1.0f);
    crop_handle_corner(cr.right, cr.bottom, -1.0f, -1.0f);
    fill_rounded(rect_make(rect_cx(cr) - 8.0f, cr.top - 1.5f,
                           rect_cx(cr) + 8.0f, cr.top + 1.5f),
                 1.5f, white);
    fill_rounded(rect_make(rect_cx(cr) - 8.0f, cr.bottom - 1.5f,
                           rect_cx(cr) + 8.0f, cr.bottom + 1.5f),
                 1.5f, white);
    fill_rounded(rect_make(cr.left - 1.5f, rect_cy(cr) - 8.0f, cr.left + 1.5f,
                           rect_cy(cr) + 8.0f),
                 1.5f, white);
    fill_rounded(rect_make(cr.right - 1.5f, rect_cy(cr) - 8.0f,
                           cr.right + 1.5f, rect_cy(cr) + 8.0f),
                 1.5f, white);

    preset = (g_app.crop_preset >= 0 && g_app.crop_preset < 8)
                 ? RATIOS[g_app.crop_preset].label
                 : L"∞";
    rotated_pixel_size(&rw, &rh);
    swprintf(info, 96, L"%ls  ·  %d × %d", preset,
             (int)floorf((float)rw * (g_app.crop.right - g_app.crop.left) +
                         0.5f),
             (int)floorf((float)rh * (g_app.crop.bottom - g_app.crop.top) +
                         0.5f));
    info_w = text_width(info, g_app.f_tiny) + 24.0f;
    info_rect = rect_make(cr.left, cr.top - 34.0f, cr.left + info_w,
                          cr.top - 10.0f);
    if (info_rect.top < g_app.canvas.top + 8.0f) {
        info_rect = rect_offset(info_rect, 0.0f,
                                cr.top + 10.0f - info_rect.top);
    }
    if (info_rect.right > g_app.canvas.right - 10.0f) {
        info_rect = rect_offset(info_rect,
                                g_app.canvas.right - 10.0f - info_rect.right,
                                0.0f);
    }
    fill_rounded(info_rect, 12.0f, rgba(0x1C1B20, 0.86f));
    draw_text_raw(info, g_app.f_tiny, info_rect, white, 1, 1, false, true);
}

static void draw_spinner(float cx, float cy, float radius) {
    float angle = fmodf((float)g_app.now * 240.0f, 360.0f);
    stroke_ellipse(cx, cy, radius, radius,
                   color_alpha(g_app.theme.on_surface, 0.14f), 2.4f);
    draw_arc(cx, cy, radius, angle, 260.0f, g_app.theme.primary, 2.4f);
}

static void draw_empty_state(void) {
    LumaTheme *t = &g_app.theme;
    float cw = rect_w(g_app.canvas);
    float ch = rect_h(g_app.canvas);
    float card_w = fminf(430.0f, cw - 64.0f);
    float card_h = fminf(252.0f, ch - 48.0f);
    float cx = rect_cx(g_app.canvas);
    float cy = rect_cy(g_app.canvas);
    Rect card;
    float top;
    Rect illustration;
    Rect photo;
    Rect keycap;
    float key_w = 58.0f;

    if (card_w < 280.0f) card_w = cw - 32.0f;
    if (card_h < 210.0f) card_h = ch - 32.0f;
    card = rect_make(cx - card_w * 0.5f, cy - card_h * 0.5f + 6.0f,
                     cx + card_w * 0.5f, cy + card_h * 0.5f + 6.0f);
    top = card.top;

    draw_soft_shadow(card, 26.0f, 0.65f);
    fill_rounded(card, 26.0f,
                 color_mix(t->container_lowest, t->surface, g_app.dark ? 0.25f
                                                                      : 0.35f));
    stroke_rounded(rect_inset(card, 0.5f, 0.5f), 25.5f,
                   color_alpha(t->outline_variant, 0.5f), 1.0f, NULL);

    illustration = rect_make(cx - 52.0f, top + 35.0f, cx + 52.0f, top + 139.0f);
    fill_ellipse(rect_cx(illustration), rect_cy(illustration), 50.0f, 50.0f,
                 color_alpha(t->primary_container, g_app.dark ? 0.5f : 0.72f));
    stroke_ellipse(rect_cx(illustration), rect_cy(illustration), 50.0f, 50.0f,
                   color_alpha(t->primary, 0.18f), 1.0f);
    photo = rect_make(cx - 37.0f, top + 61.0f, cx + 37.0f, top + 119.0f);
    fill_rounded(rect_offset(photo, 0.0f, 4.0f), 13.0f,
                 color_alpha(t->shadow, 0.12f));
    fill_rounded(photo, 13.0f, t->surface_bright);
    stroke_rounded(photo, 13.0f, color_alpha(t->outline_variant, 0.7f), 1.0f,
                   NULL);
    draw_icon(ICON_IMAGE,
              rect_make(rect_cx(photo) - 16.0f, rect_cy(photo) - 15.0f,
                        rect_cx(photo) + 16.0f, rect_cy(photo) + 15.0f),
              t->primary);
    fill_ellipse(photo.right - 3.0f, photo.bottom - 3.0f, 10.0f, 10.0f,
                 t->primary);
    line(photo.right - 8.0f, photo.bottom - 3.0f, photo.right + 2.0f,
         photo.bottom - 3.0f, t->on_primary, 1.6f);
    line(photo.right - 3.0f, photo.bottom - 8.0f, photo.right - 3.0f,
         photo.bottom + 2.0f, t->on_primary, 1.6f);
    draw_icon(ICON_SPARKLE, rect_make(cx + 34.0f, top + 49.0f, cx + 54.0f,
                                      top + 69.0f),
              t->tertiary);

    keycap = rect_make(cx - key_w * 0.5f, top + 182.0f,
                       cx + key_w * 0.5f, top + 210.0f);
    draw_keycap(keycap, L"Ctrl O");
}

static void draw_loading_state(void) {
    LumaTheme *t = &g_app.theme;
    Rect card;
    float cx = rect_cx(g_app.canvas);
    float cy = rect_cy(g_app.canvas);
    float name_w = 0.0f;
    float card_w = 92.0f;
    Rect name_rect;

    fill_rect(g_app.canvas, color_alpha(t->canvas, 0.62f));
    if (g_app.loading_name[0]) {
        name_w = fminf(text_width(g_app.loading_name, g_app.f_caption), 220.0f);
        card_w = name_w + 86.0f;
    }
    card = rect_make(cx - card_w * 0.5f, cy - 32.0f,
                     cx + card_w * 0.5f, cy + 32.0f);
    draw_soft_shadow(card, 18.0f, 0.7f);
    fill_rounded(card, 18.0f, t->surface);
    stroke_rounded(card, 18.0f, color_alpha(t->outline_variant, 0.55f), 1.0f,
                   NULL);
    draw_spinner(card.left + 34.0f, rect_cy(card), 14.0f);
    if (name_w > 0.0f) {
        name_rect = rect_make(card.left + 62.0f, card.top + 10.0f,
                              card.right - 16.0f, card.bottom - 10.0f);
        draw_text_trimmed(g_app.loading_name, g_app.f_caption, name_rect,
                          t->on_surface_variant, false);
    }
}

static void draw_zoom_pill(void) {
    LumaTheme *t = &g_app.theme;
    wchar_t label[32];
    float tw;
    Rect pill;
    bool has_label;
    if (!g_app.image_visible || g_app.crop_mode) return;
    has_label = fabsf(g_app.zoom - 1.0f) >= 0.01f;
    if (has_label) swprintf(label, 32, L"%d%%", (int)(g_app.zoom * 100.0f + 0.5f));
    else label[0] = L'\0';
    tw = has_label ? text_width(label, g_app.f_tiny) : 0.0f;
    pill = rect_make(g_app.canvas.right - 22.0f - (tw + (has_label ? 38.0f : 36.0f)),
                     g_app.canvas.bottom - 22.0f - 28.0f,
                     g_app.canvas.right - 22.0f,
                     g_app.canvas.bottom - 22.0f);
    fill_rounded(pill, 10.0f, color_alpha(t->surface, 0.94f));
    stroke_rounded(pill, 10.0f, color_alpha(t->outline_variant, 0.6f), 1.0f,
                   NULL);
    draw_icon(ICON_ZOOM,
              rect_make(pill.left + (has_label ? 8.0f : 11.0f),
                        rect_cy(pill) - 7.0f,
                        pill.left + (has_label ? 22.0f : 25.0f),
                        rect_cy(pill) + 7.0f),
              t->on_surface_variant);
    if (has_label) {
        draw_text_raw(label, g_app.f_tiny,
                      rect_make(pill.left + 26.0f, pill.top, pill.right - 8.0f,
                                pill.bottom),
                      t->on_surface, 0, 1, false, true);
    }
}

static void draw_toast(void) {
    LumaTheme *t = &g_app.theme;
    Color bg;
    Color fg;
    float alpha;
    float tw;
    float width;
    Rect card;
    Rect text;
    Color icon_color;

    if (!g_app.toast_active) return;
    if (g_app.now > g_app.toast_until) {
        g_app.toast_active = false;
        return;
    }
    alpha = clampf((float)(g_app.now - g_app.toast_start) / 0.12f, 0.0f, 1.0f);
    alpha = fminf(alpha,
                  clampf((float)(g_app.toast_until - g_app.now) / 0.28f, 0.0f,
                         1.0f));
    if (alpha <= 0.0f) return;
    if (g_app.toast_error) {
        bg = t->error_container;
        fg = t->on_error_container;
        icon_color = t->error;
    } else {
        bg = t->inverse_surface;
        fg = t->inverse_on_surface;
        icon_color = t->inverse_on_surface;
    }
    if (!g_app.toast_error) {
        width = 54.0f;
        card = rect_make(rect_cx(g_app.canvas) - width * 0.5f,
                         g_app.canvas.bottom - 28.0f - 46.0f -
                             (g_app.crop_mode ? 40.0f : 0.0f),
                         rect_cx(g_app.canvas) + width * 0.5f,
                         g_app.canvas.bottom - 28.0f -
                             (g_app.crop_mode ? 40.0f : 0.0f));
        fill_rounded(card, 14.0f, color_alpha(bg, alpha * 0.97f));
        stroke_rounded(card, 14.0f,
                       color_alpha(t->outline, alpha * 0.4f), 1.0f, NULL);
        draw_icon(ICON_CHECK,
                  rect_make(rect_cx(card) - 10.0f, rect_cy(card) - 10.0f,
                            rect_cx(card) + 10.0f, rect_cy(card) + 10.0f),
                  color_alpha(icon_color, alpha));
        return;
    }

    tw = text_width(g_app.toast, g_app.f_body_strong);
    width = fminf(rect_w(g_app.canvas) - 48.0f, tw + 64.0f);
    if (width < 180.0f) width = 180.0f;
    card = rect_make(rect_cx(g_app.canvas) - width * 0.5f,
                     g_app.canvas.bottom - 28.0f - 46.0f -
                         (g_app.crop_mode ? 40.0f : 0.0f),
                     rect_cx(g_app.canvas) + width * 0.5f,
                     g_app.canvas.bottom - 28.0f -
                         (g_app.crop_mode ? 40.0f : 0.0f));
    fill_rounded(card, 14.0f, color_alpha(bg, alpha * 0.97f));
    stroke_rounded(card, 14.0f,
                   color_alpha(t->error, alpha * 0.4f), 1.0f, NULL);
    draw_icon(ICON_INFO,
              rect_make(card.left + 16.0f, rect_cy(card) - 9.0f,
                        card.left + 34.0f, rect_cy(card) + 9.0f),
              color_alpha(icon_color, alpha));
    text = rect_make(card.left + 42.0f, card.top + 4.0f, card.right - 14.0f,
                     card.bottom - 4.0f);
    draw_text_trimmed(g_app.toast, g_app.f_body_strong, text,
                      color_alpha(fg, alpha), true);
}

static void paint_canvas(void) {
    LumaTheme *t = &g_app.theme;
    float x;
    float y;
    Rect clip = g_app.canvas;
    Color dot_color;

    if (rect_w(clip) <= 0.0f || rect_h(clip) <= 0.0f) return;
    g_app.rt->lpVtbl->PushAxisAlignedClip(g_app.rt, &clip,
                                          D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    fill_rect(g_app.canvas, t->canvas);
    fill_ellipse(g_app.canvas.left + rect_w(g_app.canvas) * 0.18f,
                 g_app.canvas.top + rect_h(g_app.canvas) * 0.18f,
                 rect_w(g_app.canvas) * 0.28f, rect_h(g_app.canvas) * 0.30f,
                 color_alpha(t->primary, g_app.dark ? 0.035f : 0.028f));
    fill_ellipse(g_app.canvas.right - rect_w(g_app.canvas) * 0.12f,
                 g_app.canvas.bottom - rect_h(g_app.canvas) * 0.08f,
                 rect_w(g_app.canvas) * 0.22f, rect_h(g_app.canvas) * 0.24f,
                 color_alpha(t->tertiary, g_app.dark ? 0.03f : 0.022f));

    dot_color = color_alpha(t->on_surface, g_app.dark ? 0.08f : 0.045f);
    for (y = g_app.canvas.top + 16.0f; y < g_app.canvas.bottom;
         y += 24.0f) {
        for (x = g_app.canvas.left + 16.0f; x < g_app.canvas.right;
             x += 24.0f) {
            fill_ellipse(x, y, 0.9f, 0.9f, dot_color);
        }
    }

    if (g_app.has_image && g_app.image_visible) {
        draw_photo();
        if (g_app.crop_mode) draw_crop_overlay();
    }
    if (g_app.loading) {
        if (!g_app.has_image) {
            fill_rect(g_app.canvas, t->canvas);
            draw_loading_state();
        } else {
            draw_loading_state();
        }
    } else if (!g_app.has_image) {
        draw_empty_state();
    }
    if (g_app.has_image && !g_app.loading) draw_zoom_pill();
    draw_toast();
    g_app.rt->lpVtbl->PopAxisAlignedClip(g_app.rt);
}

static void paint_tooltip(void) {
    LumaControl *c;
    wchar_t text[160];
    float tw, left, top;
    Rect tip;
    if (g_app.hover_id < 0 || g_app.mouse_down || g_app.pressed_id != -1) return;
    if (GetTickCount64() - g_app.hover_since < 650) return;
    c = find_control(g_app.hover_id);
    if (c == NULL || c->tooltip == NULL || c->tooltip[0] == L'\0') return;
    wcsncpy(text, c->tooltip, 159);
    text[159] = L'\0';
    tw = text_width(text, g_app.f_caption);
    left = rect_cx(c->rect) - tw * 0.5f - 10.0f;
    if (left < 8.0f) left = 8.0f;
    if (left + tw + 20.0f > g_app.width - 8.0f)
        left = g_app.width - tw - 28.0f;
    top = c->rect.top - 32.0f;
    if (top < 6.0f) top = c->rect.bottom + 6.0f;
    tip = rect_make(left, top, left + tw + 20.0f, top + 26.0f);
    fill_rounded(tip, 8.0f, color_alpha(g_app.theme.inverse_surface, 0.96f));
    draw_text_raw(text, g_app.f_caption, tip, g_app.theme.inverse_on_surface, 1,
                  1, false, true);
}

static void paint_app(void) {
    HRESULT hr;
    D2D1_MATRIX_3X2_F identity;
    if (g_app.rt == NULL) return;
    identity = luma_identity_matrix();
    g_app.rt->lpVtbl->SetTransform(g_app.rt, &identity);
    g_app.rt->lpVtbl->BeginDraw(g_app.rt);
    g_app.rt->lpVtbl->Clear(g_app.rt, &g_app.theme.surface);
    paint_sidebar();
    paint_appbar();
    paint_canvas();
    paint_tooltip();
    hr = g_app.rt->lpVtbl->EndDraw(g_app.rt, NULL, NULL);
    if (hr == D2DERR_RECREATE_TARGET) {
        discard_device_resources();
        InvalidateRect(g_app.hwnd, NULL, FALSE);
    }
}

#include "ui_actions.inc"
#include "ui_window.inc"




