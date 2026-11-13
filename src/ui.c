/* ui.c — Material 3 interface: structure, paint, hit testing, pointer input.

   Structure (one destination, so no navigation component):
     app bar (small, 64)  ·  supporting pane (tools, 340/320 dp)  ·  canvas
   Below 840 dp the pane becomes a modal side sheet; the app bar grows a tools
   icon button.  Nothing here draws a raw hex value: every colour comes from a
   semantic role in FusTheme. */

#include "app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* --------------------------------------------------------------- constants */

#define APPBAR_H 64.0f
#define PANE_W_EXPANDED 340.0f
#define PANE_W_LARGE 360.0f
#define MARGIN 16.0f
#define SECTION_GAP 24.0f
#define COMPONENT_GAP 8.0f
#define BUTTON_H 40.0f
#define CHIP_H 32.0f
#define ICON_BTN 40.0f
#define SHEET_W 320.0f

#define FADE_MS 220.0f
#define SNACK_MS 3600.0f
#define SNACK_SLIDE_MS 180.0f

/* Presets — order matches the ACT_CROP_* block in app.h (free first). */
const FusCropPreset g_presets[] = {
    {L"自由", 0.0f},   {L"16:9", 16.0f / 9.0f}, {L"1:1", 1.0f},
    {L"9:16", 9.0f / 16.0f}, {L"4:3", 4.0f / 3.0f}, {L"3:4", 3.0f / 4.0f},
    {L"3:2", 3.0f / 2.0f},   {L"2:3", 2.0f / 3.0f},
};
const int g_preset_count = (int)(sizeof(g_presets) / sizeof(g_presets[0]));

/* ------------------------------------------------------------ small helpers */

static D2D1_RECT_F rectf(float l, float t, float r, float b) {
    D2D1_RECT_F out;
    out.left = l;
    out.top = t;
    out.right = r;
    out.bottom = b;
    return out;
}

static bool point_in(D2D1_RECT_F r, float x, float y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void set_solid(FusApp *app, D2D1_COLOR_F c) {
    app->brush->lpVtbl->SetColor(app->brush, &c);
}

static void fill_rect(FusApp *app, D2D1_RECT_F r, D2D1_COLOR_F c) {
    set_solid(app, c);
    RT(app)->lpVtbl->FillRectangle(RT(app), &r, (ID2D1Brush *)app->brush);
}

static void fill_round(FusApp *app, D2D1_RECT_F r, float radius,
                       D2D1_COLOR_F c) {
    D2D1_ROUNDED_RECT rr;
    rr.rect = r;
    rr.radiusX = radius;
    rr.radiusY = radius;
    set_solid(app, c);
    RT(app)->lpVtbl->FillRoundedRectangle(RT(app), &rr,
                                          (ID2D1Brush *)app->brush);
}

static void stroke_round(FusApp *app, D2D1_RECT_F r, float radius,
                         D2D1_COLOR_F c, float width) {
    D2D1_ROUNDED_RECT rr;
    rr.rect = r;
    rr.radiusX = radius;
    rr.radiusY = radius;
    set_solid(app, c);
    RT(app)->lpVtbl->DrawRoundedRectangle(RT(app), &rr,
                                          (ID2D1Brush *)app->brush, width,
                                          NULL);
}

static void fill_circle(FusApp *app, float cx, float cy, float r,
                        D2D1_COLOR_F c) {
    D2D1_ELLIPSE e;
    e.point.x = cx;
    e.point.y = cy;
    e.radiusX = r;
    e.radiusY = r;
    set_solid(app, c);
    RT(app)->lpVtbl->FillEllipse(RT(app), &e, (ID2D1Brush *)app->brush);
}

static void text_in(FusApp *app, const wchar_t *s, IDWriteTextFormat *fmt,
                    D2D1_RECT_F box, D2D1_COLOR_F c,
                    DWRITE_TEXT_ALIGNMENT halign,
                    DWRITE_PARAGRAPH_ALIGNMENT valign) {
    fmt->lpVtbl->SetTextAlignment(fmt, halign);
    fmt->lpVtbl->SetParagraphAlignment(fmt, valign);
    set_solid(app, c);
    RT(app)->lpVtbl->DrawText(RT(app), s, (UINT32)wcslen(s), fmt, &box,
                              (ID2D1Brush *)app->brush,
                              D2D1_DRAW_TEXT_OPTIONS_CLIP,
                              DWRITE_MEASURING_MODE_NATURAL);
}

/* Single-line measurement for centring icon+label groups. */
static float text_width(FusApp *app, const wchar_t *s, IDWriteTextFormat *fmt) {
    IDWriteTextLayout *layout = NULL;
    DWRITE_TEXT_METRICS m;
    float w = 0.0f;
    if (FAILED(app->dw->lpVtbl->CreateTextLayout(
            app->dw, s, (UINT32)wcslen(s), fmt, 4096.0f, 4096.0f, &layout)) ||
        layout == NULL) {
        return 0.0f;
    }
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &m))) {
        w = m.widthIncludingTrailingWhitespace;
    }
    fus_release(layout);
    return w;
}

/* -------------------------------------------------------------- hit records */

static FusHit *hit_add(FusApp *app, int action, D2D1_RECT_F r, float radius) {
    FusHit *h;
    if (app->hit_count >= FUS_MAX_HITS) {
        return NULL;
    }
    h = &app->hits[app->hit_count++];
    h->action = action;
    h->rect = r;
    h->radius = radius;
    return h;
}

static bool hit_is_hot(FusApp *app, int action) {
    return app->hot == action;
}

static bool hit_is_active(FusApp *app, int action) {
    return app->active == action;
}

/* State layer over the widget's own shape (M3: 8% hover, 10% pressed, always
   of the on-colour, never a repaint with another role). */
static void state_layer(FusApp *app, D2D1_RECT_F r, float radius,
                        D2D1_COLOR_F on_color, bool hover, bool pressed) {
    float a = 0.0f;
    if (pressed) {
        a = 0.10f;
    } else if (hover) {
        a = 0.08f;
    }
    if (a <= 0.0f) {
        return;
    }
    fill_round(app, r, radius, color_alpha(on_color, a));
}

/* --------------------------------------------------------------- primitives */

typedef struct {
    int action;
    const wchar_t *label;
    int icon;
    bool disabled;
} ButtonSpec;

static void draw_icon_button(FusApp *app, int action, int icon, D2D1_RECT_F r,
                             bool selected, bool disabled) {
    const FusTheme *t = &app->theme;
    bool hover = !disabled && hit_is_hot(app, action);
    bool pressed = !disabled && hit_is_active(app, action);
    D2D1_COLOR_F fg = disabled ? color_alpha(t->on_surface, 0.38f)
                               : (selected ? t->primary : t->on_surface_variant);

    if (selected) {
        fill_circle(app, (r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f,
                    (r.right - r.left) * 0.5f, t->secondary_container);
        fg = t->on_secondary_container;
    }
    draw_icon(app, icon, rectf(r.left + 8.0f, r.top + 8.0f, r.right - 8.0f,
                               r.bottom - 8.0f),
              fg);
    if (!disabled) {
        state_layer(app, r, (r.bottom - r.top) * 0.5f, fg, hover, pressed);
    }
    hit_add(app, action, r, (r.bottom - r.top) * 0.5f);
}

static void draw_chip(FusApp *app, int action, const wchar_t *label,
                      D2D1_RECT_F r, bool selected, bool disabled) {
    const FusTheme *t = &app->theme;
    bool hover = !disabled && hit_is_hot(app, action);
    bool pressed = !disabled && hit_is_active(app, action);
    D2D1_COLOR_F bg = selected ? t->secondary_container : (D2D1_COLOR_F){0, 0, 0, 0};
    D2D1_COLOR_F fg = selected ? t->on_secondary_container : t->on_surface_variant;
    float radius = 8.0f;
    float inner_x = r.left + 12.0f;
    float check = selected ? 14.0f : 0.0f;

    if (disabled) {
        fg = color_alpha(t->on_surface, 0.38f);
    }
    if (selected) {
        fill_round(app, r, radius, bg);
    } else {
        stroke_round(app, r, radius, color_alpha(t->outline, 1.0f), 1.0f);
    }
    if (selected) {
        draw_icon(app, ICON_CHECK, rectf(inner_x, r.top + 9.0f,
                                         inner_x + check, r.top + 23.0f),
                  fg);
        inner_x += check + 6.0f;
    }
    if (selected) {
        text_in(app, label, app->type.label_large,
                rectf(inner_x, r.top, r.right - 12.0f, r.bottom), fg,
                DWRITE_TEXT_ALIGNMENT_LEADING,
                DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    } else {
        text_in(app, label, app->type.label_large,
                rectf(r.left + 12.0f, r.top, r.right - 12.0f, r.bottom), fg,
                DWRITE_TEXT_ALIGNMENT_CENTER,
                DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (!disabled) {
        state_layer(app, r, radius, fg, hover, pressed);
    }
    hit_add(app, action, r, radius);
}

/* --------------------------------------------------------------- layout math */

static int size_class_for(float w) {
    if (w < 600.0f) return CLS_COMPACT;
    if (w < 840.0f) return CLS_MEDIUM;
    if (w < 1200.0f) return CLS_EXPANDED;
    return CLS_LARGE;
}

static float pane_width_for(int cls) {
    if (cls == CLS_LARGE) return PANE_W_LARGE;
    if (cls == CLS_EXPANDED) return PANE_W_EXPANDED;
    return 0.0f; /* sheet */
}

/* Image geometry ---------------------------------------------------------- */

static void oriented_size(FusApp *app, float *w, float *h) {
    bool odd = (app->rotation & 1) != 0;
    float iw = odd ? app->img_h : app->img_w;
    float ih = odd ? app->img_w : app->img_h;
    LumaCrop c = app->cropping ? (LumaCrop){0.0f, 0.0f, 1.0f, 1.0f} : app->crop;
    *w = iw * (c.right - c.left);
    *h = ih * (c.bottom - c.top);
}

static D2D1_MATRIX_3X2_F mat_mul(D2D1_MATRIX_3X2_F a, D2D1_MATRIX_3X2_F b) {
    D2D1_MATRIX_3X2_F r;
    r._11 = a._11 * b._11 + a._21 * b._12;
    r._12 = a._12 * b._11 + a._22 * b._12;
    r._21 = a._11 * b._21 + a._21 * b._22;
    r._22 = a._12 * b._21 + a._22 * b._22;
    r._31 = a._11 * b._31 + a._21 * b._32 + a._31;
    r._32 = a._12 * b._31 + a._22 * b._32 + a._32;
    return r;
}

static D2D1_MATRIX_3X2_F mat_translate(float x, float y) {
    D2D1_MATRIX_3X2_F m;
    m._11 = 1; m._12 = 0; m._21 = 0; m._22 = 1; m._31 = x; m._32 = y;
    return m;
}

static D2D1_MATRIX_3X2_F mat_scale(float x, float y) {
    D2D1_MATRIX_3X2_F m;
    m._11 = x; m._12 = 0; m._21 = 0; m._22 = y; m._31 = 0; m._32 = 0;
    return m;
}

static D2D1_MATRIX_3X2_F mat_rotate(float radians) {
    D2D1_MATRIX_3X2_F m;
    float c = (float)cos(radians), s = (float)sin(radians);
    m._11 = c; m._12 = s; m._21 = -s; m._22 = c; m._31 = 0; m._32 = 0;
    return m;
}

static float fit_scale(FusApp *app) {
    float ow, oh, sx, sy;
    oriented_size(app, &ow, &oh);
    if (ow <= 0.0f || oh <= 0.0f) return 1.0f;
    sx = (app->canvas.right - app->canvas.left - 96.0f) / ow;
    sy = (app->canvas.bottom - app->canvas.top - 96.0f) / oh;
    if (sx > 1.0f) sx = 1.0f;
    if (sy > 1.0f) sy = 1.0f;
    if (sx < 0.02f) sx = 0.02f;
    if (sy < 0.02f) sy = 0.02f;
    return sx < sy ? sx : sy;
}

float ui_effective_zoom(FusApp *app) {
    return app->fit ? fit_scale(app) : app->zoom;
}

D2D1_MATRIX_3X2_F ui_image_matrix(FusApp *app) {
    float ow, oh, m;
    float cx = (app->canvas.left + app->canvas.right) * 0.5f + app->pan_x;
    float cy = (app->canvas.top + app->canvas.bottom) * 0.5f + app->pan_y;
    oriented_size(app, &ow, &oh);
    m = ui_effective_zoom(app);
    {
        D2D1_MATRIX_3X2_F x = mat_translate(cx, cy);
        x = mat_mul(x, mat_scale(app->mirror_h ? -1.0f : 1.0f,
                                 app->mirror_v ? -1.0f : 1.0f));
        x = mat_mul(x, mat_rotate((float)app->rotation * 1.5707963267948966f));
        x = mat_mul(x, mat_scale(m, m));
        x = mat_mul(x, mat_translate(-ow * 0.5f, -oh * 0.5f));
        return x;
    }
}

/* Oriented-image pixel coordinates -> screen dp.  No rotation or mirror here:
   the crop rectangle lives in oriented space, which is exactly the bounding
   box the rotated image occupies. */
D2D1_MATRIX_3X2_F ui_oriented_matrix(FusApp *app) {
    float ow, oh, m;
    float cx = (app->canvas.left + app->canvas.right) * 0.5f + app->pan_x;
    float cy = (app->canvas.top + app->canvas.bottom) * 0.5f + app->pan_y;
    oriented_size(app, &ow, &oh);
    m = ui_effective_zoom(app);
    {
        D2D1_MATRIX_3X2_F x = mat_translate(cx, cy);
        x = mat_mul(x, mat_scale(m, m));
        x = mat_mul(x, mat_translate(-ow * 0.5f, -oh * 0.5f));
        return x;
    }
}

/* Screen point -> oriented-image pixel coordinates (inverse of the display
   matrix).  Crop drags use this. */
bool ui_screen_to_image(FusApp *app, float sx, float sy, float *ix,
                        float *iy) {
    D2D1_MATRIX_3X2_F m = ui_image_matrix(app);
    float det = m._11 * m._22 - m._12 * m._21;
    float dx, dy;
    if (fabsf(det) < 1e-6f) {
        return false;
    }
    dx = sx - m._31;
    dy = sy - m._32;
    *ix = (dx * m._22 - dy * m._21) / det;
    *iy = (dy * m._11 - dx * m._12) / det;
    return true;
}

/* ------------------------------------------------------------------- paint */

static void paint_appbar(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F bar = app->appbar;
    float x = MARGIN;
    float cy = (bar.top + bar.bottom) * 0.5f;
    bool compact = app->pane_w == 0.0f;

    fill_rect(app, bar, t->surface);

    if (compact) {
        draw_icon_button(app, ACT_TOOLS, ICON_TUNE,
                         rectf(x, cy - ICON_BTN * 0.5f, x + ICON_BTN,
                               cy + ICON_BTN * 0.5f),
                         app->sheet_open, false);
        x += ICON_BTN + 4.0f;
    }

    /* no brand chrome: the window title carries the name */


    /* trailing actions, right aligned */
    {
        float right = bar.right - 8.0f;
        float cyb = cy;

        if (app->cropping && app->pane_w == 0.0f) {
            D2D1_RECT_F apply = rectf(right - ICON_BTN, cyb - ICON_BTN * 0.5f,
                                      right, cyb + ICON_BTN * 0.5f);
            D2D1_RECT_F cancel;
            fill_circle(app, (apply.left + apply.right) * 0.5f,
                        (apply.top + apply.bottom) * 0.5f, ICON_BTN * 0.5f,
                        t->primary);
            draw_icon(app, ICON_CHECK,
                      rectf((apply.left + apply.right) * 0.5f - 11.0f,
                            (apply.top + apply.bottom) * 0.5f - 11.0f,
                            (apply.left + apply.right) * 0.5f + 11.0f,
                            (apply.top + apply.bottom) * 0.5f + 11.0f),
                      t->on_primary);
            state_layer(app, apply, ICON_BTN * 0.5f, t->on_primary,
                        hit_is_hot(app, ACT_CROP_APPLY),
                        hit_is_active(app, ACT_CROP_APPLY));
            hit_add(app, ACT_CROP_APPLY, apply, ICON_BTN * 0.5f);
            right -= ICON_BTN;
            cancel = rectf(right - ICON_BTN, cyb - ICON_BTN * 0.5f, right,
                           cyb + ICON_BTN * 0.5f);
            draw_icon_button(app, ACT_CROP_CANCEL, ICON_CLOSE, cancel, false,
                             false);
            return;
        }

        /* filled icon-only save */
        {
            D2D1_RECT_F save = rectf(right - ICON_BTN, cyb - ICON_BTN * 0.5f,
                                     right, cyb + ICON_BTN * 0.5f);
            bool disabled = !app->has_image || app->exporting;
            D2D1_COLOR_F bg = disabled ? color_alpha(t->on_surface, 0.12f)
                                       : t->primary;
            D2D1_COLOR_F fg = disabled ? color_alpha(t->on_surface, 0.38f)
                                       : t->on_primary;
            fill_circle(app, (save.left + save.right) * 0.5f,
                        (save.top + save.bottom) * 0.5f, ICON_BTN * 0.5f, bg);
            draw_icon(app, ICON_SAVE,
                      rectf((save.left + save.right) * 0.5f - 11.0f,
                            (save.top + save.bottom) * 0.5f - 11.0f,
                            (save.left + save.right) * 0.5f + 11.0f,
                            (save.top + save.bottom) * 0.5f + 11.0f),
                      fg);
            if (!disabled) {
                state_layer(app, save, ICON_BTN * 0.5f, fg,
                            hit_is_hot(app, ACT_SAVE),
                            hit_is_active(app, ACT_SAVE));
            }
            hit_add(app, ACT_SAVE, save, ICON_BTN * 0.5f);
            right -= ICON_BTN + 4.0f;
        }

        /* quick save: write the result next to the source file, no dialog */
        draw_icon_button(app, ACT_SAVE_NEAR, ICON_SAVE_NEAR,
                         rectf(right - ICON_BTN, cyb - ICON_BTN * 0.5f, right,
                               cyb + ICON_BTN * 0.5f),
                         false, !app->has_image || app->exporting);
        right -= ICON_BTN;

        draw_icon_button(app, ACT_THEME, app->dark ? ICON_SUN : ICON_MOON,
                         rectf(right - ICON_BTN, cy - ICON_BTN * 0.5f, right,
                               cy + ICON_BTN * 0.5f),
                         false, false);
        right -= ICON_BTN;

        draw_icon_button(app, ACT_REDO, ICON_REDO,
                         rectf(right - ICON_BTN, cy - ICON_BTN * 0.5f, right,
                               cy + ICON_BTN * 0.5f),
                         false, false);
        right -= ICON_BTN;
        draw_icon_button(app, ACT_UNDO, ICON_UNDO,
                         rectf(right - ICON_BTN, cy - ICON_BTN * 0.5f, right,
                               cy + ICON_BTN * 0.5f),
                         false, false);
    }
}/* One square icon tile with an optional selected state. */
static void draw_icon_tile(FusApp *app, int action, int icon, D2D1_RECT_F r,
                           bool selected, bool disabled) {
    const FusTheme *t = &app->theme;
    bool hover = !disabled && hit_is_hot(app, action);
    bool pressed = !disabled && hit_is_active(app, action);
    D2D1_COLOR_F bg = selected ? t->secondary_container : t->surface_container;
    D2D1_COLOR_F fg = selected ? t->on_secondary_container : t->on_surface;
    float radius = 12.0f;
    float cx = (r.left + r.right) * 0.5f;
    float cy = (r.top + r.bottom) * 0.5f;

    if (disabled) {
        bg = color_alpha(t->on_surface, 0.12f);
        fg = color_alpha(t->on_surface, 0.38f);
    }
    fill_round(app, r, radius, bg);
    draw_icon(app, icon, rectf(cx - 12.0f, cy - 12.0f, cx + 12.0f, cy + 12.0f),
              fg);
    if (!disabled) {
        state_layer(app, r, radius, fg, hover, pressed);
    }
    hit_add(app, action, r, radius);
}

/* 1dp hairline used to separate icon groups instead of section labels. */
static void hairline(FusApp *app, float x0, float x1, float y) {
    fill_rect(app, rectf(x0, y, x1, y + 1.0f), app->theme.outline_variant);
}

static void draw_confirmation(FusApp *app, D2D1_RECT_F r, bool enabled) {
    const FusTheme *t = &app->theme;
    float half = (r.right - r.left - COMPONENT_GAP) * 0.5f;
    D2D1_RECT_F cancel = rectf(r.left, r.top, r.left + half, r.bottom);
    D2D1_RECT_F apply = rectf(r.left + half + COMPONENT_GAP, r.top, r.right,
                              r.bottom);
    D2D1_COLOR_F fg_cancel = enabled ? t->on_secondary_container
                                     : color_alpha(t->on_surface, 0.38f);
    D2D1_COLOR_F bg_cancel = enabled ? t->secondary_container
                                     : color_alpha(t->on_surface, 0.12f);

    fill_round(app, cancel, BUTTON_H * 0.5f, bg_cancel);
    draw_icon(app, ICON_CLOSE,
              rectf((cancel.left + cancel.right) * 0.5f - 12.0f,
                    (cancel.top + cancel.bottom) * 0.5f - 12.0f,
                    (cancel.left + cancel.right) * 0.5f + 12.0f,
                    (cancel.top + cancel.bottom) * 0.5f + 12.0f),
              fg_cancel);
    if (enabled) {
        state_layer(app, cancel, BUTTON_H * 0.5f, fg_cancel,
                    hit_is_hot(app, ACT_CROP_CANCEL),
                    hit_is_active(app, ACT_CROP_CANCEL));
    }
    hit_add(app, ACT_CROP_CANCEL, cancel, BUTTON_H * 0.5f);

    {
        D2D1_COLOR_F bg = enabled ? t->primary
                                  : color_alpha(t->on_surface, 0.12f);
        D2D1_COLOR_F fg = enabled ? t->on_primary
                                  : color_alpha(t->on_surface, 0.38f);
        fill_round(app, apply, BUTTON_H * 0.5f, bg);
        draw_icon(app, ICON_CHECK,
                  rectf((apply.left + apply.right) * 0.5f - 12.0f,
                        (apply.top + apply.bottom) * 0.5f - 12.0f,
                        (apply.left + apply.right) * 0.5f + 12.0f,
                        (apply.top + apply.bottom) * 0.5f + 12.0f),
                  fg);
        if (enabled) {
            state_layer(app, apply, BUTTON_H * 0.5f, fg,
                        hit_is_hot(app, ACT_CROP_APPLY),
                        hit_is_active(app, ACT_CROP_APPLY));
        }
        hit_add(app, ACT_CROP_APPLY, apply, BUTTON_H * 0.5f);
    }
}

static void paint_pane(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F pane = app->pane;
    float x = pane.left + MARGIN;
    float w = pane.right - pane.left - MARGIN * 2.0f;
    float y = pane.top + MARGIN - app->pane_scroll;
    bool enabled = app->has_image;
    float bw = (w - COMPONENT_GAP * 3.0f) / 4.0f;

    fill_rect(app, pane, t->surface);
    RT(app)->lpVtbl->PushAxisAlignedClip(RT(app), &pane,
                                         D2D1_ANTIALIAS_MODE_ALIASED);

    if (app->cropping) {
        /* apply / cancel first, mirroring the original crop panel */
        draw_confirmation(app, rectf(x, y, x + w, y + BUTTON_H), true);
        y += BUTTON_H + SECTION_GAP;
    } else {
        /* the single filled action of the screen: open a picture */
        float cx = x + 28.0f;
        float cy = y + 28.0f;
        D2D1_COLOR_F bg = t->primary;
        D2D1_COLOR_F fg = t->on_primary;
        fill_circle(app, cx, cy, 28.0f, bg);
        draw_icon(app, ICON_OPEN, rectf(cx - 13.0f, cy - 13.0f, cx + 13.0f,
                                        cy + 13.0f), fg);
        state_layer(app, rectf(cx - 28.0f, cy - 28.0f, cx + 28.0f, cy + 28.0f),
                    28.0f, fg, hit_is_hot(app, ACT_OPEN),
                    hit_is_active(app, ACT_OPEN));
        hit_add(app, ACT_OPEN, rectf(cx - 28.0f, cy - 28.0f, cx + 28.0f,
                                     cy + 28.0f), 28.0f);
        y += 56.0f + 20.0f;
        hairline(app, x, x + w, y);
        y += 1.0f + 16.0f;

        /* rotate */
        draw_icon_tile(app, ACT_ROTATE_CCW, ICON_ROTATE_CCW,
                       rectf(x, y, x + bw, y + 48.0f), false, !enabled);
        draw_icon_tile(app, ACT_ROTATE_CW, ICON_ROTATE_CW,
                       rectf(x + bw + COMPONENT_GAP, y,
                             x + bw * 2.0f + COMPONENT_GAP, y + 48.0f),
                       false, !enabled);
        draw_icon_tile(app, ACT_ROTATE_180_CCW, ICON_ROTATE_CCW_180,
                       rectf(x + bw * 2.0f + COMPONENT_GAP * 2.0f, y,
                             x + bw * 3.0f + COMPONENT_GAP * 2.0f, y + 48.0f),
                       false, !enabled);
        draw_icon_tile(app, ACT_ROTATE_180_CW, ICON_ROTATE_CW_180,
                       rectf(x + bw * 3.0f + COMPONENT_GAP * 3.0f, y, x + w,
                             y + 48.0f),
                       false, !enabled);
        y += 48.0f + 20.0f;
        hairline(app, x, x + w, y);
        y += 1.0f + 16.0f;
    }

    /* crop ratios; the free entry is the crop glyph */
    {
        float chip_x = x;
        int i;
        for (i = 0; i < g_preset_count; ++i) {
            float cw;
            if (i == 0) {
                cw = 56.0f;
            } else {
                cw = 12.0f + text_width(app, g_presets[i].label,
                                        app->type.label_large) +
                     12.0f + 20.0f;
            }
            if (chip_x + cw > x + w && chip_x > x) {
                chip_x = x;
                y += CHIP_H + COMPONENT_GAP;
            }
            if (i == 0) {
                draw_icon_tile(app, ACT_CROP_FREE, ICON_CROP,
                               rectf(chip_x, y, chip_x + cw, y + CHIP_H),
                               app->crop_preset == 0, !enabled);
            } else {
                draw_chip(app, ACT_CROP_FREE + i, g_presets[i].label,
                          rectf(chip_x, y, chip_x + cw, y + CHIP_H),
                          app->crop_preset == i, !enabled);
            }
            chip_x += cw + COMPONENT_GAP;
        }
        y += CHIP_H + 20.0f;
        hairline(app, x, x + w, y);
        y += 1.0f + 16.0f;
    }

    if (app->cropping) {
        /* nothing else needs the pane while a crop is pending */
    } else {
        /* flip / filters */
        draw_icon_tile(app, ACT_FLIP_H, ICON_FLIP_H,
                       rectf(x, y, x + bw, y + 48.0f), false, !enabled);
        draw_icon_tile(app, ACT_FLIP_V, ICON_FLIP_V,
                       rectf(x + bw + COMPONENT_GAP, y,
                             x + bw * 2.0f + COMPONENT_GAP, y + 48.0f),
                       false, !enabled);
        draw_icon_tile(app, ACT_GRAYSCALE, ICON_CONTRAST,
                       rectf(x + bw * 2.0f + COMPONENT_GAP * 2.0f, y,
                             x + bw * 3.0f + COMPONENT_GAP * 2.0f, y + 48.0f),
                       app->grayscale, !enabled);
        draw_icon_tile(app, ACT_INVERT, ICON_INVERT,
                       rectf(x + bw * 3.0f + COMPONENT_GAP * 3.0f, y, x + w,
                             y + 48.0f),
                       app->invert, !enabled);
        y += 48.0f + 20.0f;
        hairline(app, x, x + w, y);
        y += 1.0f + 16.0f;

    }

    app->pane_content_h = (y - (pane.top + MARGIN) + app->pane_scroll) + MARGIN;
    RT(app)->lpVtbl->PopAxisAlignedClip(RT(app));
}
static void paint_empty_state(FusApp *app) {
    const FusTheme *t = &app->theme;
    float cx = (app->canvas.left + app->canvas.right) * 0.5f;
    float cy = (app->canvas.top + app->canvas.bottom) * 0.5f;
    D2D1_RECT_F box;
    float circle = 96.0f;

    fill_circle(app, cx, cy - 88.0f, circle * 0.5f, t->primary_container);
    draw_icon(app, ICON_IMAGE,
              rectf(cx - 22.0f, cy - 110.0f, cx + 22.0f, cy - 66.0f),
              t->on_primary_container);

    box = rectf(cx - 260.0f, cy - 16.0f, cx + 260.0f, cy + 20.0f);
    text_in(app, L"还没有打开图片", app->type.title_medium, box, t->on_surface,
            DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    box = rectf(cx - 260.0f, cy + 24.0f, cx + 260.0f, cy + 48.0f);
    text_in(app, L"把图片拖到这里，或按 Ctrl+O", app->type.body_small, box,
            t->on_surface_variant, DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    /* the whole canvas accepts a drop (see WM_DROPFILES) */
    hit_add(app, ACT_OPEN, app->canvas, 0.0f);
}
static ID2D1BitmapBrush *checker_brush(FusApp *app) {
    const FusTheme *t = &app->theme;
    UINT32 *px;
    UINT cell = 8;
    UINT i, j;

    if (app->checker != NULL) {
        return app->checker;
    }
    px = (UINT32 *)malloc((size_t)cell * cell * 4u);
    if (px == NULL) return NULL;
    for (j = 0; j < cell; ++j) {
        for (i = 0; i < cell; ++i) {
            bool odd = (((i / 4) + (j / 4)) & 1) != 0;
            D2D1_COLOR_F c = odd ? t->surface_container_high
                                 : t->surface_container_lowest;
            UINT32 r = (UINT32)(c.r * 255.0f);
            UINT32 g = (UINT32)(c.g * 255.0f);
            UINT32 b = (UINT32)(c.b * 255.0f);
            px[j * cell + i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    {
        D2D1_BITMAP_PROPERTIES bp;
        D2D1_SIZE_U size;
        ID2D1Bitmap *bitmap = NULL;
        bp.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        bp.dpiX = 96.0f;
        bp.dpiY = 96.0f;
        size.width = cell;
        size.height = cell;
        if (SUCCEEDED(RT(app)->lpVtbl->CreateBitmap(RT(app), size, px,
                                                    cell * 4u, &bp,
                                                    &bitmap)) &&
            bitmap != NULL) {
            D2D1_BITMAP_BRUSH_PROPERTIES bbp;
            bbp.extendModeX = D2D1_EXTEND_MODE_WRAP;
            bbp.extendModeY = D2D1_EXTEND_MODE_WRAP;
            bbp.interpolationMode =
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
            RT(app)->lpVtbl->CreateBitmapBrush(RT(app), bitmap, &bbp, NULL,
                                               &app->checker);
            fus_release(bitmap);
        }
    }
    free(px);
    return app->checker;
}

/* Crop mode: dim everything outside the working rectangle, outline it, and
   float the confirm/cancel bar over the canvas (Material's crop affordance). */
static void paint_crop_overlay(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F c = app->canvas;
    D2D1_MATRIX_3X2_F m = ui_oriented_matrix(app);
    float ow, oh, x0, y0, x1, y1;
    D2D1_RECT_F r;
    D2D1_COLOR_F scrim = {0.0f, 0.0f, 0.0f, 0.45f};

    oriented_size(app, &ow, &oh);
    x0 = m._11 * (app->crop_work[0] * ow) + m._31;
    y0 = m._22 * (app->crop_work[1] * oh) + m._32;
    x1 = m._11 * (app->crop_work[2] * ow) + m._31;
    y1 = m._22 * (app->crop_work[3] * oh) + m._32;
    r = rectf(x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, x0 < x1 ? x1 : x0,
              y0 < y1 ? y1 : y0);

    RT(app)->lpVtbl->PushAxisAlignedClip(RT(app), &c,
                                         D2D1_ANTIALIAS_MODE_ALIASED);

    /* four bands of scrim around the kept rectangle */
    if (r.top > c.top) {
        fill_rect(app, rectf(c.left, c.top, c.right, r.top), scrim);
    }
    if (r.bottom < c.bottom) {
        fill_rect(app, rectf(c.left, r.bottom, c.right, c.bottom), scrim);
    }
    if (r.left > c.left) {
        fill_rect(app, rectf(c.left, r.top, r.left, r.bottom), scrim);
    }
    if (r.right < c.right) {
        fill_rect(app, rectf(r.right, r.top, c.right, r.bottom), scrim);
    }

    /* frame, thirds, handles */
    set_solid(app, t->on_surface);
    RT(app)->lpVtbl->DrawRectangle(RT(app), &r, (ID2D1Brush *)app->brush, 1.5f,
                                   NULL);
    {
        float gx1 = r.left + (r.right - r.left) / 3.0f;
        float gx2 = r.left + (r.right - r.left) * 2.0f / 3.0f;
        float gy1 = r.top + (r.bottom - r.top) / 3.0f;
        float gy2 = r.top + (r.bottom - r.top) * 2.0f / 3.0f;
        D2D1_COLOR_F guide = color_alpha(t->on_surface, 0.4f);
        set_solid(app, guide);
        RT(app)->lpVtbl->DrawLine(RT(app), (D2D1_POINT_2F){gx1, r.top},
                                  (D2D1_POINT_2F){gx1, r.bottom},
                                  (ID2D1Brush *)app->brush, 1.0f, NULL);
        RT(app)->lpVtbl->DrawLine(RT(app), (D2D1_POINT_2F){gx2, r.top},
                                  (D2D1_POINT_2F){gx2, r.bottom},
                                  (ID2D1Brush *)app->brush, 1.0f, NULL);
        RT(app)->lpVtbl->DrawLine(RT(app), (D2D1_POINT_2F){r.left, gy1},
                                  (D2D1_POINT_2F){r.right, gy1},
                                  (ID2D1Brush *)app->brush, 1.0f, NULL);
        RT(app)->lpVtbl->DrawLine(RT(app), (D2D1_POINT_2F){r.left, gy2},
                                  (D2D1_POINT_2F){r.right, gy2},
                                  (ID2D1Brush *)app->brush, 1.0f, NULL);
    }
    {
        float hx[8] = {r.left, r.right, r.left, r.right,
                       (r.left + r.right) * 0.5f, (r.left + r.right) * 0.5f,
                       r.left, r.right};
        float hy[8] = {r.top, r.top, r.bottom, r.bottom,
                       r.top, r.bottom, (r.top + r.bottom) * 0.5f,
                       (r.top + r.bottom) * 0.5f};
        int i;
        for (i = 0; i < 8; ++i) {
            fill_circle(app, hx[i], hy[i], 6.5f, t->surface);
            fill_circle(app, hx[i], hy[i], 4.5f, t->on_surface);
        }
    }

    RT(app)->lpVtbl->PopAxisAlignedClip(RT(app));
}
/* The canvas shares the app bar and pane colour; only the corner where they
   meet is rounded, which is what makes the two bars read as one surface. */
static void fill_canvas_bg(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F c = app->canvas;
    const float r = 28.0f;

    fill_rect(app, c, t->surface_container_lowest);
    if (app->pane_w <= 0.0f) {
        return;
    }
    /* square off the junction, then carve the quarter circle back out */
    fill_rect(app, rectf(c.left, c.top, c.left + r, c.top + r), t->surface);
    fill_circle(app, c.left + r, c.top + r, r, t->surface_container_lowest);
}
static void paint_canvas(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F c = app->canvas;

    if (!app->has_image) {
        paint_empty_state(app);
        return;
    }

    /* everything below draws the picture and its overlays: clip it to the
       canvas so a zoomed or panned image can never spill over the rail */
    RT(app)->lpVtbl->PushAxisAlignedClip(RT(app), &c,
                                         D2D1_ANTIALIAS_MODE_ALIASED);

    {
        float ow, oh;
        D2D1_MATRIX_3X2_F saved, x;
        D2D1_RECT_F dest;
        D2D1_RECT_F src;
        LumaCrop crop = app->cropping ? (LumaCrop){0.0f, 0.0f, 1.0f, 1.0f}
                                      : app->crop;
        float alpha = app->fade;

        oriented_size(app, &ow, &oh);
        x = ui_image_matrix(app);
        RT(app)->lpVtbl->GetTransform(RT(app), &saved);
        RT(app)->lpVtbl->SetTransform(RT(app), &x);

        dest = rectf(0.0f, 0.0f, ow, oh);
        src.left = crop.left * app->img_w;
        src.top = crop.top * app->img_h;
        src.right = crop.right * app->img_w;
        src.bottom = crop.bottom * app->img_h;

        if (app->bitmap != NULL) {
            /* checkerboard behind the photo so PNG transparency reads */
            if (checker_brush(app) != NULL) {
                D2D1_RECT_F bord = rectf(-2.0f, -2.0f, ow + 2.0f, oh + 2.0f);
                RT(app)->lpVtbl->FillRectangle(RT(app), &bord,
                                               (ID2D1Brush *)checker_brush(app));
            }
            RT(app)->lpVtbl->DrawBitmap(RT(app), app->bitmap, &dest, alpha,
                                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                        &src);
            /* 1dp outline around the picture, like a floating sheet */
            set_solid(app, color_alpha(t->outline_variant, 0.9f * alpha));
            RT(app)->lpVtbl->DrawRectangle(RT(app), &dest,
                                           (ID2D1Brush *)app->brush, 1.0f,
                                           NULL);
        }
        RT(app)->lpVtbl->SetTransform(RT(app), &saved);
    }

    if (app->cropping) {
        paint_crop_overlay(app);
    }

    /* indeterminate progress while decoding */
    if (app->loading) {
        float phase = (float)((app->now_ms - app->load_start_ms) % 1200) / 1200.0f;
        float w = c.right - c.left;
        float bar_w = w * 0.28f;
        float x0 = c.left + (w + bar_w) * phase - bar_w;
        fill_rect(app, rectf(c.left, c.top, c.right, c.top + 4.0f),
                  t->surface_container_highest);
        fill_rect(app, rectf(x0, c.top, x0 + bar_w, c.top + 4.0f), t->primary);
    }
    RT(app)->lpVtbl->PopAxisAlignedClip(RT(app));
}

static void paint_snackbar(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F r;
    float w = 420.0f;
    float slide = 1.0f;
    float cx = (app->canvas.left + app->canvas.right) * 0.5f;

    if (!ui_snackbar_visible(app)) {
        return;
    }
    {
        uint64_t elapsed = app->now_ms - (app->snackbar_until_ms - (uint64_t)SNACK_MS);
        if (elapsed < (uint64_t)SNACK_SLIDE_MS) {
            slide = (float)elapsed / SNACK_SLIDE_MS;
        }
    }
    r = rectf(cx - w * 0.5f, app->height_dp - 16.0f - 48.0f + (1.0f - slide) * 12.0f,
              cx + w * 0.5f, app->height_dp - 16.0f + (1.0f - slide) * 12.0f);
    fill_round(app, r, 4.0f, t->inverse_surface);
    text_in(app, app->snackbar, app->type.body_medium,
            rectf(r.left + 16.0f, r.top, r.right - 96.0f, r.bottom),
            t->inverse_on_surface, DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (app->snackbar_action[0] != L'\0') {
        text_in(app, app->snackbar_action, app->type.label_large,
                rectf(r.right - 96.0f, r.top, r.right - 16.0f, r.bottom),
                t->inverse_on_surface, DWRITE_TEXT_ALIGNMENT_TRAILING,
                DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        hit_add(app, app->snackbar_action_id,
                rectf(r.right - 96.0f, r.top, r.right - 16.0f, r.bottom), 4.0f);
    }
}

static void paint_sheet(FusApp *app) {
    const FusTheme *t = &app->theme;
    D2D1_RECT_F sheet;
    D2D1_RECT_F scrim = rectf(0, APPBAR_H, (float)app->width_dp,
                              (float)app->height_dp);
    float slide = app->sheet_anim;

    if (app->pane_w != 0.0f || slide <= 0.0f) {
        return;
    }
    fill_rect(app, scrim, (D2D1_COLOR_F){0.0f, 0.0f, 0.0f, 0.32f * slide});
    hit_add(app, ACT_TOOLS, scrim, 0.0f); /* click outside closes */
    sheet = rectf(-SHEET_W * (1.0f - slide), APPBAR_H,
                  SHEET_W - SHEET_W * (1.0f - slide), (float)app->height_dp);
    fill_round(app, sheet, 0.0f, t->surface_container_low);
    hit_add(app, ACT_NONE, sheet, 0.0f); /* swallowing clicks inside the sheet */
    app->pane = sheet;
    paint_pane(app);
}

/* Short hints shown next to the cursor. The interface itself carries no text
   labels, so every icon-only control explains itself on hover. */
static const wchar_t *tip_for(int action) {
    switch (action) {
        case ACT_OPEN: return L"打开图片  Ctrl+O";
        case ACT_SAVE: return L"保存  Ctrl+S";
        case ACT_UNDO: return L"撤销  Ctrl+Z";
        case ACT_REDO: return L"重做  Ctrl+Y";
        case ACT_THEME: return L"深色 / 浅色  T";
        case ACT_ROTATE_CW: return L"右转 90°  R";
        case ACT_ROTATE_CCW: return L"左转 90°  Shift+R";
        case ACT_ROTATE_180_CW: return L"右转 180°";
        case ACT_ROTATE_180_CCW: return L"左转 180°";
        case ACT_FLIP_H: return L"水平翻转  H";
        case ACT_FLIP_V: return L"垂直翻转  Shift+H";
        case ACT_GRAYSCALE: return L"黑白";
        case ACT_INVERT: return L"反色";
        case ACT_ZOOM_FIT: return L"适应窗口  Ctrl+F";
        case ACT_ZOOM_100: return L"实际大小  Ctrl+0";
        case ACT_CROP_FREE: return L"自由裁剪";
        case ACT_CROP_APPLY: return L"应用裁剪  Enter";
        case ACT_CROP_CANCEL: return L"取消裁剪  Esc";
        case ACT_TOOLS: return L"工具";
        default: return NULL;
    }
}

static void paint_tooltip(FusApp *app) {
    const FusTheme *t = &app->theme;
    const wchar_t *text;
    float tw, w, h, x, y;
    D2D1_RECT_F r;

    if (app->hot == ACT_NONE || app->active != ACT_NONE) {
        return;
    }
    text = tip_for(app->hot);
    if (text == NULL) {
        return;
    }
    if (app->hot >= ACT_CROP_FREE && app->hot <= ACT_CROP_2_3) {
        int index = app->hot - ACT_CROP_FREE;
        if (index > 0 && index < g_preset_count) {
            text = g_presets[index].label;
        }
    }
    tw = text_width(app, text, app->type.body_small);
    w = tw + 20.0f;
    h = 28.0f;
    x = app->pointer_x + 14.0f;
    y = app->pointer_y + 22.0f;
    if (x + w > (float)app->width_dp - 8.0f) {
        x = (float)app->width_dp - 8.0f - w;
    }
    if (y + h > (float)app->height_dp - 8.0f) {
        y = app->pointer_y - h - 12.0f;
    }
    r = rectf(x, y, x + w, y + h);
    fill_round(app, r, 6.0f, t->inverse_surface);
    text_in(app, text, app->type.body_small,
            rectf(r.left + 10.0f, r.top, r.right - 10.0f, r.bottom),
            t->inverse_on_surface, DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}
/* MD3 ripple: a circle that grows from the press point and fades out, clipped
   to the widget it belongs to so it can never leave the control's bounds. */
static void draw_ripple(FusApp *app) {
    const FusTheme *t = &app->theme;
    float p, radius, reach, dx, dy;
    D2D1_RECT_F r;
    int i;

    if (app->ripple_action == ACT_NONE) {
        return;
    }
    p = (float)(app->now_ms - app->ripple_ms) / 420.0f;
    if (p >= 1.0f) {
        app->ripple_action = ACT_NONE;
        return;
    }
    if (p < 0.0f) {
        return;
    }
    for (i = 0; i < app->hit_count; i++) {
        if (app->hits[i].action == app->ripple_action) {
            break;
        }
    }
    if (i == app->hit_count) {
        return;
    }
    r = app->hits[i].rect;
    dx = app->ripple_x - r.left;
    dy = app->ripple_y - r.top;
    reach = dx > (r.right - app->ripple_x) ? dx : (r.right - app->ripple_x);
    {
        float fy = dy > (r.bottom - app->ripple_y) ? dy
                                                   : (r.bottom - app->ripple_y);
        if (fy > reach) {
            reach = fy;
        }
    }
    radius = reach * 1.15f * p;
    RT(app)->lpVtbl->PushAxisAlignedClip(RT(app), &r,
                                         D2D1_ANTIALIAS_MODE_ALIASED);
    fill_circle(app, app->ripple_x, app->ripple_y, radius,
                color_alpha(t->on_surface, 0.16f * (1.0f - p)));
    RT(app)->lpVtbl->PopAxisAlignedClip(RT(app));
}

void ui_paint(FusApp *app) {    const FusTheme *t = &app->theme;
    RECT rc;
    float scale;
    float w;
    float h;
    int cls;

    /* ID2D1RenderTarget::GetSize returns a struct by value and the mingw
       headers cannot call that reliably, so the size comes from the window. */
    GetClientRect(app->hwnd, &rc);
    scale = app->dpi > 0.0f ? app->dpi / 96.0f : 1.0f;
    w = (float)(rc.right - rc.left) / scale;
    h = (float)(rc.bottom - rc.top) / scale;
    cls = size_class_for(w);

    app->hit_count = 0;
    app->width_dp = (int)w;
    app->height_dp = (int)h;

    /* sheet slide (200 ms, emphasized decelerate) */
    if (app->sheet_open) {
        float a = (float)(app->now_ms - app->sheet_start_ms) / 200.0f;
        app->sheet_anim = a > 1.0f ? 1.0f : (a < 0.0f ? 0.0f : a);
    } else if (app->sheet_anim > 0.0f) {
        float a = app->sheet_anim -
                  (float)(app->now_ms - app->sheet_start_ms) / 200.0f;
        app->sheet_anim = a < 0.0f ? 0.0f : a;
    }
    app->pane_w = pane_width_for(cls);
    app->appbar = rectf(0, 0, w, APPBAR_H);
    app->pane = rectf(0, APPBAR_H, app->pane_w, h);
    app->canvas = rectf(app->pane_w, APPBAR_H, w, h);

    RT(app)->lpVtbl->BeginDraw(RT(app));
    RT(app)->lpVtbl->Clear(RT(app), &t->surface);

    fill_canvas_bg(app);

    paint_appbar(app);
    {
    
    }
    if (app->pane_w > 0.0f) {
        paint_pane(app);
    }
    paint_canvas(app);
    paint_sheet(app);
    paint_snackbar(app);

    if (app->exporting) {
        float phase = (float)((app->now_ms - app->export_start_ms) % 1200) /
                      1200.0f;
        float bar_w = (app->canvas.right - app->canvas.left) * 0.28f;
        float x0 = app->canvas.left +
                   (app->canvas.right - app->canvas.left + bar_w) * phase -
                   bar_w;
        fill_rect(app, rectf(app->canvas.left, app->canvas.bottom - 4.0f,
                             app->canvas.right, app->canvas.bottom),
                  t->surface_container_highest);
        fill_rect(app, rectf(x0, app->canvas.bottom - 4.0f, x0 + bar_w,
                             app->canvas.bottom),
                  t->primary);
    }

    draw_ripple(app);
    paint_tooltip(app);

    {
        HRESULT hr = RT(app)->lpVtbl->EndDraw(RT(app), NULL, NULL);
        if (hr == D2DERR_RECREATE_TARGET) {
            app->target_lost = true;
        }
    }
    {
        static int hlog_n;
        if (hlog_n < 40) {
            hlog_n++;
            fus_log_hr("ui_hits", app->hit_count);
            fus_log_hr("ui_pane_w", (long)app->pane_w);
        }
    }
}

/* ------------------------------------------------------------------- input */

bool ui_hit_test(FusApp *app, float x, float y, int *action, FusHit *out) {
    int i;
    for (i = app->hit_count - 1; i >= 0; --i) {
        if (point_in(app->hits[i].rect, x, y)) {
            if (action != NULL) *action = app->hits[i].action;
            if (out != NULL) *out = app->hits[i];
            return true;
        }
    }
    return false;
}

void ui_pointer_move(FusApp *app, float x, float y) {
    int action = ACT_NONE;
    int prev = app->hot;
    app->pointer_x = x;
    app->pointer_y = y;
    if (!ui_hit_test(app, x, y, &action, NULL)) {
        action = ACT_NONE;
    }
    app->hot = action;
    if (prev != action) {
        ui_invalidate(app);
    }
}

void ui_pointer_down(FusApp *app, float x, float y) {
    int action = ACT_NONE;
    ui_pointer_move(app, x, y);
    if (ui_hit_test(app, x, y, &action, NULL)) {
        app->active = action;
        app->press_x = x;
        app->press_y = y;
        app->press_ms = app->now_ms;
        ui_invalidate(app);
    }
}

void ui_pointer_up(FusApp *app, float x, float y) {
    int action = ACT_NONE;
    char fire = 0;
    if (ui_hit_test(app, x, y, &action, NULL) && action == app->active) {
        fire = 1;
    }
    app->active = ACT_NONE;
    ui_invalidate(app);
    if (fire) {
        app_action(action);
    }
}

void ui_pointer_leave(FusApp *app) {
    if (app->hot != ACT_NONE) {
        app->hot = ACT_NONE;
        ui_invalidate(app);
    }
}

void ui_snackbar(FusApp *app, const wchar_t *text, const wchar_t *action,
                 int action_id) {
    wcsncpy(app->snackbar, text, 191);
    app->snackbar[191] = L'\0';
    if (action != NULL) {
        wcsncpy(app->snackbar_action, action, 15);
        app->snackbar_action[15] = L'\0';
        app->snackbar_action_id = action_id;
    } else {
        app->snackbar_action[0] = L'\0';
        app->snackbar_action_id = ACT_NONE;
    }
    app->snackbar_until_ms = app->now_ms + (uint64_t)SNACK_MS;
    ui_invalidate(app);
}

bool ui_snackbar_visible(FusApp *app) {
    return app->snackbar[0] != L'\0' && app->now_ms < app->snackbar_until_ms;
}

bool ui_snackbar_click(FusApp *app, float x, float y) {
    int action = ACT_NONE;
    if (app->snackbar_action_id == ACT_NONE) {
        return false;
    }
    if (!ui_hit_test(app, x, y, &action, NULL)) {
        return false;
    }
    return action == app->snackbar_action_id;
}

bool ui_animation_active(FusApp *app) {
    if (app->loading || app->exporting) return true;
    if (ui_snackbar_visible(app)) {
        uint64_t left = app->snackbar_until_ms - app->now_ms;
        if (left < (uint64_t)SNACK_SLIDE_MS) return true;
    }
    if (app->fade_active) return true;
    if (app->sheet_anim != 0.0f) return true;
    if (app->ripple_action != ACT_NONE) return true;
    if (app->theme_anim) return true;
    return false;
}

void ui_invalidate(FusApp *app) {
    if (app->hwnd != NULL) {
        InvalidateRect(app->hwnd, NULL, FALSE);
    }
}
