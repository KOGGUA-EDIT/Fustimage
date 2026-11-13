/* theme.c — Material 3 baseline scheme (seed #6750A4, tonal-spot) plus the
   fixed type scale.  Values come from the M3 baseline palette used by the
   material-design-3-ui skill (assets/tokens.css), so light/dark is a role swap,
   never a hand-picked colour list. */

#include "app.h"

#include <string.h>

/* ------------------------------------------------------------------ helpers */

D2D1_COLOR_F color_alpha(D2D1_COLOR_F c, float a) {
    D2D1_COLOR_F out = c;
    out.a = a;
    return out;
}

float color_luminance(D2D1_COLOR_F c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

static D2D1_COLOR_F rgb(unsigned int hex) {
    D2D1_COLOR_F c;
    c.r = ((hex >> 16) & 0xFF) / 255.0f;
    c.g = ((hex >> 8) & 0xFF) / 255.0f;
    c.b = (hex & 0xFF) / 255.0f;
    c.a = 1.0f;
    return c;
}

/* -------------------------------------------------------------------- light */

void theme_set_light(FusTheme *t) {
    t->surface = rgb(0xfdf7ff);
    t->surface_dim = rgb(0xded8e0);
    t->surface_bright = rgb(0xfdf7ff);
    t->surface_container_lowest = rgb(0xffffff);
    t->surface_container_low = rgb(0xf8f2fa);
    t->surface_container = rgb(0xf2ecf4);
    t->surface_container_high = rgb(0xece6ee);
    t->surface_container_highest = rgb(0xe6e0e9);
    t->on_surface = rgb(0x1d1b20);
    t->on_surface_variant = rgb(0x49454e);
    t->outline = rgb(0x7a757f);
    t->outline_variant = rgb(0xcac4cf);
    t->primary = rgb(0x65558f);
    t->on_primary = rgb(0xffffff);
    t->primary_container = rgb(0xe9ddff);
    t->on_primary_container = rgb(0x201047);
    t->secondary = rgb(0x625b71);
    t->on_secondary = rgb(0xffffff);
    t->secondary_container = rgb(0xe8def8);
    t->on_secondary_container = rgb(0x1e192b);
    t->tertiary = rgb(0x7e5260);
    t->tertiary_container = rgb(0xffd9e3);
    t->on_tertiary_container = rgb(0x31101d);
    t->error = rgb(0xba1a1a);
    t->error_container = rgb(0xffdad6);
    t->on_error_container = rgb(0x410002);
    t->inverse_surface = rgb(0x322f35);
    t->inverse_on_surface = rgb(0xf5eff7);
    t->surface_tint = rgb(0x65558f);
}

/* --------------------------------------------------------------------- dark */

void theme_set_dark(FusTheme *t) {
    t->surface = rgb(0x141218);
    t->surface_dim = rgb(0x141218);
    t->surface_bright = rgb(0x3b383e);
    t->surface_container_lowest = rgb(0x0f0d13);
    t->surface_container_low = rgb(0x1d1b20);
    t->surface_container = rgb(0x211f24);
    t->surface_container_high = rgb(0x2b292f);
    t->surface_container_highest = rgb(0x36343a);
    t->on_surface = rgb(0xe6e0e9);
    t->on_surface_variant = rgb(0xcac4cf);
    t->outline = rgb(0x948f99);
    t->outline_variant = rgb(0x49454e);
    t->primary = rgb(0xcfbdfe);
    t->on_primary = rgb(0x36275d);
    t->primary_container = rgb(0x4d3d75);
    t->on_primary_container = rgb(0xe9ddff);
    t->secondary = rgb(0xcbc2db);
    t->on_secondary = rgb(0x332d41);
    t->secondary_container = rgb(0x4a4458);
    t->on_secondary_container = rgb(0xe8def8);
    t->tertiary = rgb(0xefb8c8);
    t->tertiary_container = rgb(0x633b48);
    t->on_tertiary_container = rgb(0xffd9e3);
    t->error = rgb(0xffb4ab);
    t->error_container = rgb(0x93000a);
    t->on_error_container = rgb(0xffdad6);
    t->inverse_surface = rgb(0xe6e0e9);
    t->inverse_on_surface = rgb(0x322f35);
    t->surface_tint = rgb(0xcfbdfe);
}

void theme_build(FusTheme *out, bool dark) {
    if (dark) {
        theme_set_dark(out);
    } else {
        theme_set_light(out);
    }
}

void theme_apply(FusApp *app, bool dark) {
    app->dark = dark;
    theme_build(&app->theme, dark);
}

/* Cross-fade between two palettes: every role is a plain colour, so the whole
   struct can be interpolated role by role. */
void theme_blend(FusTheme *out, const FusTheme *a, const FusTheme *b, float t) {
    D2D1_COLOR_F *o = (D2D1_COLOR_F *)out;
    const D2D1_COLOR_F *pa = (const D2D1_COLOR_F *)a;
    const D2D1_COLOR_F *pb = (const D2D1_COLOR_F *)b;
    size_t i;
    size_t n = sizeof(FusTheme) / sizeof(D2D1_COLOR_F);

    if (t <= 0.0f) {
        *out = *a;
        return;
    }
    if (t >= 1.0f) {
        *out = *b;
        return;
    }
    for (i = 0; i < n; i++) {
        o[i].r = pa[i].r + (pb[i].r - pa[i].r) * t;
        o[i].g = pa[i].g + (pb[i].g - pa[i].g) * t;
        o[i].b = pa[i].b + (pb[i].b - pa[i].b) * t;
        o[i].a = pa[i].a + (pb[i].a - pa[i].a) * t;
    }
}

/* -------------------------------------------------------------- type scale */

/* One family for both slots is allowed by the scale; Segoe UI Variable is the
   Windows 11 display face and DWrite falls back to the system CJK face
   (Noto Sans SC / Microsoft YaHei) for Chinese glyphs. */
#define FUS_FONT L"Segoe UI Variable Display"

static IDWriteTextFormat *make_format(FusApp *app, float size, float line,
                                      DWRITE_FONT_WEIGHT weight) {
    IDWriteTextFormat *fmt = NULL;
    HRESULT hr = app->dw->lpVtbl->CreateTextFormat(
        app->dw, FUS_FONT, NULL, weight, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"zh-CN", &fmt);
    if (FAILED(hr) || fmt == NULL) {
        /* Older shells: Segoe UI always exists. */
        hr = app->dw->lpVtbl->CreateTextFormat(
            app->dw, L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"zh-CN", &fmt);
        if (FAILED(hr)) {
            return NULL;
        }
    }
    /* The scale fixes the line height per style; CJK glyphs are denser, so the
       M3 Latin line height is used as-is (it already leaves 8px of leading). */
    fmt->lpVtbl->SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, line,
                                line * 0.8f);
    fmt->lpVtbl->SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP);
    return fmt;
}

bool theme_create_type(FusApp *app) {
    FusType *t = &app->type;
    t->display_small = make_format(app, 36.0f, 44.0f, DWRITE_FONT_WEIGHT_NORMAL);
    t->headline_small = make_format(app, 24.0f, 32.0f, DWRITE_FONT_WEIGHT_NORMAL);
    t->title_large = make_format(app, 22.0f, 28.0f, DWRITE_FONT_WEIGHT_NORMAL);
    t->title_medium = make_format(app, 16.0f, 24.0f, DWRITE_FONT_WEIGHT_MEDIUM);
    t->body_large = make_format(app, 16.0f, 24.0f, DWRITE_FONT_WEIGHT_NORMAL);
    t->body_medium = make_format(app, 14.0f, 20.0f, DWRITE_FONT_WEIGHT_NORMAL);
    t->body_small = make_format(app, 12.0f, 16.0f, DWRITE_FONT_WEIGHT_NORMAL);
    t->label_large = make_format(app, 14.0f, 20.0f, DWRITE_FONT_WEIGHT_MEDIUM);
    t->label_medium = make_format(app, 12.0f, 16.0f, DWRITE_FONT_WEIGHT_MEDIUM);
    return t->title_large != NULL && t->body_large != NULL &&
           t->label_large != NULL && t->body_medium != NULL &&
           t->headline_small != NULL && t->title_medium != NULL;
}

static void release_format(IDWriteTextFormat **fmt) {
    if (*fmt != NULL) {
        (*fmt)->lpVtbl->Release(*fmt);
        *fmt = NULL;
    }
}

void theme_release(FusApp *app) {
    FusType *t = &app->type;
    release_format(&t->display_small);
    release_format(&t->headline_small);
    release_format(&t->title_large);
    release_format(&t->title_medium);
    release_format(&t->body_large);
    release_format(&t->body_medium);
    release_format(&t->body_small);
    release_format(&t->label_large);
    release_format(&t->label_medium);
}
