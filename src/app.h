/* app.h — shared state for Fustimage 2.0 (Material 3 rewrite).

   Design notes
   ------------
   Everything is laid out in dp (96 dpi device-independent units).  The Direct2D
   render target is given the window dpi, so drawing in dp produces crisp pixels
   on any monitor without a manual scale factor in every call site.

   Startup budget (measured on the reference machine, see bench/STARTUP.md):
     loader 17ms · window 15-20ms · D2D+WARP target 30-55ms · text formats 3ms
     first paint 25-40ms · ShowWindow 10-20ms  ->  ~120-160ms to first frame.
   Nothing that is only needed for image editing (COM, WIC, file dialogs) runs
   before the first frame. */

#ifndef FUST_APP_H
#define FUST_APP_H

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <stdbool.h>
#include <stdint.h>

#include "image.h"

/* MinGW's d2d1/dwrite headers keep the COM vtables nested (an
   ID2D1HwndRenderTarget vtbl starts with an ID2D1RenderTargetVtbl member, a
   bitmap's with an image's and so on), so a render target call has to go
   through the flat 1.0 render target vtable, while Release is always the
   first three IUnknown slots.  These two helpers hide that. */
#define RT(app) ((ID2D1RenderTarget *)((app)->rt))

static inline void fus_release(void *object) {
    if (object != NULL) {
        ((IUnknown *)object)->lpVtbl->Release((IUnknown *)object);
    }
}

/* ------------------------------------------------------------------ colors */

/* The 30-odd semantic roles this product actually uses.  No component ever
   sees a raw hex value: it asks for a role. */
typedef struct {
    D2D1_COLOR_F surface;
    D2D1_COLOR_F surface_dim;
    D2D1_COLOR_F surface_bright;
    D2D1_COLOR_F surface_container_lowest;
    D2D1_COLOR_F surface_container_low;
    D2D1_COLOR_F surface_container;
    D2D1_COLOR_F surface_container_high;
    D2D1_COLOR_F surface_container_highest;
    D2D1_COLOR_F on_surface;
    D2D1_COLOR_F on_surface_variant;
    D2D1_COLOR_F outline;
    D2D1_COLOR_F outline_variant;
    D2D1_COLOR_F primary;
    D2D1_COLOR_F on_primary;
    D2D1_COLOR_F primary_container;
    D2D1_COLOR_F on_primary_container;
    D2D1_COLOR_F secondary;
    D2D1_COLOR_F on_secondary;
    D2D1_COLOR_F secondary_container;
    D2D1_COLOR_F on_secondary_container;
    D2D1_COLOR_F tertiary;
    D2D1_COLOR_F tertiary_container;
    D2D1_COLOR_F on_tertiary_container;
    D2D1_COLOR_F error;
    D2D1_COLOR_F error_container;
    D2D1_COLOR_F on_error_container;
    D2D1_COLOR_F inverse_surface;
    D2D1_COLOR_F inverse_on_surface;
    D2D1_COLOR_F surface_tint;
} FusTheme;

/* --------------------------------------------------------------- type scale */

typedef struct {
    IDWriteTextFormat *display_small;   /* 36/44  — unused for now, kept for menus */
    IDWriteTextFormat *headline_small;  /* 24/32  — empty-state title */
    IDWriteTextFormat *title_large;     /* 22/28  — app bar title */
    IDWriteTextFormat *title_medium;    /* 16/24  — section headers, card title */
    IDWriteTextFormat *body_large;      /* 16/24  — default body */
    IDWriteTextFormat *body_medium;     /* 14/20  — supporting text */
    IDWriteTextFormat *body_small;      /* 12/16  — captions, metadata */
    IDWriteTextFormat *label_large;     /* 14/20  — buttons, chips, labels */
    IDWriteTextFormat *label_medium;    /* 12/16  — dense labels, badges */
} FusType;

/* ------------------------------------------------------------------- icons */

enum {
    ICON_OPEN = 0,
    ICON_ROTATE_CW,
    ICON_ROTATE_CCW,
    ICON_ROTATE_CW_180,
    ICON_ROTATE_CCW_180,
    ICON_FLIP_H,
    ICON_FLIP_V,
    ICON_CONTRAST,
    ICON_INVERT,
    ICON_UNDO,
    ICON_REDO,
    ICON_SAVE,
    ICON_SAVE_NEAR,
    ICON_SUN,
    ICON_MOON,
    ICON_CHECK,
    ICON_CLOSE,
    ICON_FIT,
    ICON_ACTUAL,
    ICON_CROP,
    ICON_IMAGE,
    ICON_TUNE,
    ICON_COUNT
};

/* --------------------------------------------------------------- hit testing */

/* Actions are a flat enum so the hit list stays a simple array. */
enum {
    ACT_NONE = 0,
    ACT_OPEN,
    ACT_SAVE,
    ACT_SAVE_NEAR,
    ACT_UNDO,
    ACT_REDO,
    ACT_THEME,
    ACT_ROTATE_CW,
    ACT_ROTATE_CCW,
    ACT_ROTATE_180_CW,
    ACT_ROTATE_180_CCW,
    ACT_FLIP_H,
    ACT_FLIP_V,
    ACT_GRAYSCALE,
    ACT_INVERT,
    ACT_CROP_FREE,
    ACT_CROP_16_9,
    ACT_CROP_1_1,
    ACT_CROP_9_16,
    ACT_CROP_4_3,
    ACT_CROP_3_4,
    ACT_CROP_3_2,
    ACT_CROP_2_3,
    ACT_ZOOM_FIT,
    ACT_ZOOM_100,
    ACT_CANVAS,
    ACT_TOOLS,
    ACT_CROP_APPLY,
    ACT_CROP_CANCEL,
    ACT_COUNT
};

#define FUS_MAX_HITS 48

typedef struct {
    int action;
    D2D1_RECT_F rect;   /* dp */
    float radius;       /* dp */
} FusHit;

/* Which paint style the widget used; the hit list records it so hover/press
   overlays know which shape to follow. */
enum {
    SHAPE_RECT = 0,
    SHAPE_PILL,
    SHAPE_ROUNDED
};

/* ------------------------------------------------------------------ layout */

enum { CLS_COMPACT = 0, CLS_MEDIUM, CLS_EXPANDED, CLS_LARGE };

/* Crop presets, in order of appearance. ratio == 0 means "free". */
typedef struct {
    const wchar_t *label;
    float ratio;          /* width / height */
} FusCropPreset;

/* ------------------------------------------------------------------- state */

/* The part of the document an undo step captures. */
typedef struct {
    int rotation;
    bool mirror_h, mirror_v, grayscale, invert;
    LumaCrop crop;
    int crop_preset;
} FusDocState;

typedef struct {
    HWND hwnd;
    bool dark;
    float dpi;
    int width_dp;
    int height_dp;

    FusTheme theme;
    FusType type;
    ID2D1Factory *d2d;
    IDWriteFactory *dw;
    ID2D1HwndRenderTarget *rt;
    ID2D1SolidColorBrush *brush;       /* one brush, colour set per draw */
    ID2D1BitmapBrush *dot_brush;       /* tiled 8dp dot grid for the canvas */
    ID2D1BitmapBrush *checker;         /* transparency checkerboard tile */
    ID2D1Bitmap *bitmap;               /* preview of the document */
    LumaImage base;                    /* decoded preview pixels (filters re-run on these) */
    bool have_base;

    /* document */
    bool has_image;
    wchar_t path[MAX_PATH];
    wchar_t name[MAX_PATH];
    float img_w, img_h;                /* oriented preview size, px */
    int rotation;                      /* clockwise quarter turns, 0..3 */
    bool mirror_h, mirror_v;
    bool grayscale, invert;
    LumaCrop crop;                     /* normalized, in oriented space */
    int crop_preset;                   /* index into g_presets, -1 = custom */
    float crop_work[4];                /* pending interactive crop (normalized) */
    float crop_before[4];             /* rect restored when cropping is cancelled */
    int crop_before_preset;
    bool cropping;
    int crop_grip;                    /* 0 none, 1 move, 2..9 edges/corners, 10 new rect */
    float drag_ax, drag_ay;           /* press point, oriented normalized */
    float drag_rect[4];               /* rect at press time */

    /* supporting pane as a modal sheet below 840 dp */
    bool sheet_open;
    float sheet_anim;                  /* 0..1 slide-in */
    uint64_t sheet_start_ms;
    float pane_scroll;                 /* dp scrolled inside the pane */
    float pane_content_h;

    /* undo history of FusDocState */
    FusDocState undo[64];
    int undo_len;
    int undo_pos;

    /* view */
    float zoom;                        /* 1.0 = 100% */
    bool fit;
    float pan_x, pan_y;                /* image centre offset from canvas centre */
    bool panning;
    POINT pan_origin;
    float pan_start_x, pan_start_y;

    /* async load */
    int load_gen;
    bool loading;
    uint64_t load_start_ms;
    uint64_t load_elapsed_ms;

    /* async export */
    bool exporting;
    uint64_t export_start_ms;

    /* animation clock */
    uint64_t start_ms;
    uint64_t now_ms;
    float fade;                        /* image fade-in 0..1 */
    uint64_t fade_start_ms;
    bool fade_active;
    bool target_lost;
    FusTheme theme_from;
    FusTheme theme_to;
    float theme_mix;
    bool theme_anim;
    uint64_t theme_ms;

    /* interaction */
    int hot, active;
    float ripple_x, ripple_y;
    int ripple_action;
    uint64_t ripple_ms;
    float pointer_x, pointer_y;
    float press_x, press_y;
    uint64_t press_ms;
    bool pressed_on_hit;

    /* snackbar */
    wchar_t snackbar[192];
    uint64_t snackbar_until_ms;
    wchar_t snackbar_action[16];
    int snackbar_action_id;

    /* hit list rebuilt during paint */
    FusHit hits[FUS_MAX_HITS];
    int hit_count;

    /* layout results, also filled during paint */
    D2D1_RECT_F canvas;
    D2D1_RECT_F pane;
    D2D1_RECT_F appbar;
    float pane_w;
} FusApp;

extern FusApp g_app;
extern const FusCropPreset g_presets[];
extern const int g_preset_count;

/* theme.c */
void theme_set_light(FusTheme *t);
void theme_set_dark(FusTheme *t);
void theme_apply(FusApp *app, bool dark);
void theme_build(FusTheme *out, bool dark);
void theme_blend(FusTheme *out, const FusTheme *a, const FusTheme *b, float t);
bool theme_create_type(FusApp *app);
void theme_release(FusApp *app);

/* paint helpers shared by ui.c and main.c */
D2D1_COLOR_F color_alpha(D2D1_COLOR_F c, float a);
float color_luminance(D2D1_COLOR_F c);

/* icons.c */
ID2D1PathGeometry **icons_get(ID2D1Factory *factory);
void icons_release(void);
void draw_icon(FusApp *app, int icon, D2D1_RECT_F box, D2D1_COLOR_F color);

/* ui.c — the whole interface: layout, paint, hit testing, input */
void ui_paint(FusApp *app);
bool ui_hit_test(FusApp *app, float x, float y, int *action, FusHit *out);
void ui_pointer_move(FusApp *app, float x, float y);
void ui_pointer_down(FusApp *app, float x, float y);
void ui_pointer_up(FusApp *app, float x, float y);
void ui_pointer_leave(FusApp *app);
void ui_snackbar(FusApp *app, const wchar_t *text, const wchar_t *action, int action_id);
bool ui_snackbar_visible(FusApp *app);
bool ui_snackbar_click(FusApp *app, float x, float y);
bool ui_animation_active(FusApp *app);
void ui_invalidate(FusApp *app);
void fus_log_hr(const char *label, long hr);

/* image helpers (image.c) */
bool image_load_scaled(const wchar_t *path, UINT max_side, LumaImage *out,
                       wchar_t *error, size_t cap);
void image_apply_filters(LumaImage *image, bool grayscale, bool invert);

/* main.c — the action dispatcher every widget ends up calling */
void app_action(int action);

/* ui.c helpers main.c needs */
float ui_effective_zoom(FusApp *app);
bool ui_screen_to_image(FusApp *app, float sx, float sy, float *ix, float *iy);
D2D1_MATRIX_3X2_F ui_image_matrix(FusApp *app);
D2D1_MATRIX_3X2_F ui_oriented_matrix(FusApp *app);

#endif /* FUST_APP_H */
