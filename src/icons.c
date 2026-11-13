/* icons.c — vector icons drawn as geometry in a 24x24 box.

   Using paths instead of an icon font keeps the binary dependency-free and the
   stroke weight in sync with the 4dp grid (2dp stroke at 24dp, 1.5dp at 18dp).
   Geometry is built once on the first paint (~0.2ms) and reused afterwards. */

#include "app.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG (M_PI / 180.0)

typedef struct {
    ID2D1GeometrySink *sink;
} Pen;

typedef struct {
    ID2D1PathGeometry *stroke;
    ID2D1PathGeometry *fill;
} IconGeom;

static IconGeom g_icons[ICON_COUNT];
static bool g_icons_ready;

/* --------------------------------------------------------------- primitives */

static D2D1_POINT_2F pt(float x, float y) {
    D2D1_POINT_2F p;
    p.x = x;
    p.y = y;
    return p;
}

static void begin(Pen *pen, float x, float y) {
    pen->sink->lpVtbl->Base.BeginFigure((ID2D1SimplifiedGeometrySink *)pen->sink, pt(x, y),
                                   D2D1_FIGURE_BEGIN_FILLED);
}

static void end_open(Pen *pen) {
    pen->sink->lpVtbl->Base.EndFigure((ID2D1SimplifiedGeometrySink *)pen->sink, D2D1_FIGURE_END_OPEN);
}

static void end_closed(Pen *pen) {
    pen->sink->lpVtbl->Base.EndFigure((ID2D1SimplifiedGeometrySink *)pen->sink, D2D1_FIGURE_END_CLOSED);
}

static void linep(Pen *pen, float x, float y) {
    pen->sink->lpVtbl->AddLine(pen->sink, pt(x, y));
}

/* Arc from angle a0 to a1 (degrees, y down so positive = clockwise) starting at
   (cx + r*cos a0, cy + r*sin a0). */
static void arcp(Pen *pen, float cx, float cy, float r, float a0, float a1) {
    D2D1_ARC_SEGMENT seg;
    float sweep = a1 - a0;
    while (sweep < 0.0f) sweep += 360.0f;
    while (sweep >= 360.0f) sweep -= 360.0f;
    seg.point = pt(cx + r * (float)cos(a1 * DEG), cy + r * (float)sin(a1 * DEG));
    seg.size.width = r;
    seg.size.height = r;
    seg.rotationAngle = 0.0f;
    seg.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    seg.arcSize = sweep > 180.0f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
    pen->sink->lpVtbl->AddArc(pen->sink, &seg);
}

static void arcp_rev(Pen *pen, float cx, float cy, float r, float a0,
                     float a1) {
    D2D1_ARC_SEGMENT seg;
    float sweep = a0 - a1;
    while (sweep < 0.0f) sweep += 360.0f;
    while (sweep >= 360.0f) sweep -= 360.0f;
    seg.point = pt(cx + r * (float)cos(a1 * DEG), cy + r * (float)sin(a1 * DEG));
    seg.size.width = r;
    seg.size.height = r;
    seg.rotationAngle = 0.0f;
    seg.sweepDirection = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
    seg.arcSize = sweep > 180.0f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
    pen->sink->lpVtbl->AddArc(pen->sink, &seg);
}

static void circle_path(Pen *pen, float cx, float cy, float r) {
    begin(pen, cx - r, cy);
    arcp(pen, cx, cy, r, 180.0f, 360.0f);
    arcp(pen, cx, cy, r, 0.0f, 180.0f);
    end_closed(pen);
}

/* ------------------------------------------------------------ icon builders */

static void build_open(Pen *p) {
    begin(p, 3.0f, 19.0f);
    linep(p, 3.0f, 6.5f);
    linep(p, 9.0f, 6.5f);
    linep(p, 11.2f, 8.7f);
    linep(p, 20.0f, 8.7f);
    linep(p, 20.0f, 19.0f);
    end_closed(p);
    begin(p, 3.0f, 10.5f);
    linep(p, 20.0f, 10.5f);
    end_open(p);
}

static void bez(Pen *pen, float c1x, float c1y, float c2x, float c2y,
                float x, float y);

static void build_rotate(Pen *p, float dir, int half) {
    /* Three quarter circle plus a chevron at the end. dir = 1 clockwise. */
    if (half) {
        /* Half turn reads as a U-turn arrow: down, round the bottom, up again,
           with the head at the end of the return leg. */
        float ax = dir > 0.0f ? 4.5f : 19.5f;
        float l1 = dir > 0.0f ? 18.0f : 6.0f;
        float l2 = dir > 0.0f ? 6.0f : 18.0f;
        begin(p, ax, 5.0f);
        linep(p, l1, 5.0f);
        linep(p, l1, 11.0f);
        bez(p, l1, 16.2f, l2, 16.2f, l2, 11.0f);
        linep(p, l2, 5.4f);
        end_open(p);
        begin(p, l2, 4.8f);
        linep(p, l2 - 3.1f, 8.9f);
        end_open(p);
        begin(p, l2, 4.8f);
        linep(p, l2 + 3.1f, 8.9f);
        end_open(p);
        return;
    }
    if (dir > 0.0f) {
        begin(p, 12.0f + 7.5f * (float)cos(120.0 * DEG),
              12.0f + 7.5f * (float)sin(120.0 * DEG));
        arcp(p, 12.0f, 12.0f, 7.5f, 120.0f, 360.0f);
        end_open(p);
        begin(p, 19.5f, 8.0f);
        linep(p, 19.5f, 12.0f);
        linep(p, 15.5f, 12.0f);
        end_open(p);
    } else {
        begin(p, 12.0f + 7.5f * (float)cos(60.0 * DEG),
              12.0f + 7.5f * (float)sin(60.0 * DEG));
        arcp_rev(p, 12.0f, 12.0f, 7.5f, 60.0f, -180.0f);
        end_open(p);
        begin(p, 4.5f, 8.0f);
        linep(p, 4.5f, 12.0f);
        linep(p, 8.5f, 12.0f);
        end_open(p);
    }
}

static void build_flip_h(Pen *p) {
    begin(p, 12.0f, 4.0f);
    linep(p, 12.0f, 7.0f);
    end_open(p);
    begin(p, 12.0f, 10.5f);
    linep(p, 12.0f, 13.5f);
    end_open(p);
    begin(p, 12.0f, 17.0f);
    linep(p, 12.0f, 20.0f);
    end_open(p);
    begin(p, 9.0f, 8.0f);
    linep(p, 3.5f, 12.0f);
    linep(p, 9.0f, 16.0f);
    end_closed(p);
    begin(p, 15.0f, 8.0f);
    linep(p, 20.5f, 12.0f);
    linep(p, 15.0f, 16.0f);
    end_closed(p);
}

static void build_flip_v(Pen *p) {
    begin(p, 4.0f, 12.0f);
    linep(p, 7.0f, 12.0f);
    end_open(p);
    begin(p, 10.5f, 12.0f);
    linep(p, 13.5f, 12.0f);
    end_open(p);
    begin(p, 17.0f, 12.0f);
    linep(p, 20.0f, 12.0f);
    end_open(p);
    begin(p, 8.0f, 9.0f);
    linep(p, 12.0f, 3.5f);
    linep(p, 16.0f, 9.0f);
    end_closed(p);
    begin(p, 8.0f, 15.0f);
    linep(p, 12.0f, 20.5f);
    linep(p, 16.0f, 15.0f);
    end_closed(p);
}

static void bez(Pen *pen, float c1x, float c1y, float c2x, float c2y,
                float x, float y) {
    D2D1_BEZIER_SEGMENT seg;
    seg.point1.x = c1x;
    seg.point1.y = c1y;
    seg.point2.x = c2x;
    seg.point2.y = c2y;
    seg.point3.x = x;
    seg.point3.y = y;
    pen->sink->lpVtbl->AddBezier(pen->sink, &seg);
}

static void build_contrast(Pen *p) {
    circle_path(p, 12.0f, 12.0f, 8.0f);
}

static void build_contrast_fill(Pen *s) {
    /* Right half: top of the circle, clockwise down to the bottom, back up the
       centre line. */
    begin(s, 12.0f, 4.0f);
    arcp(s, 12.0f, 12.0f, 8.0f, 270.0f, 90.0f);
    linep(s, 12.0f, 4.0f);
    end_closed(s);
}

static void build_invert(Pen *p) {
    /* Droplet, like Material's invert_colors. */
    begin(p, 12.0f, 3.0f);
    bez(p, 7.0f, 8.0f, 5.5f, 12.0f, 5.5f, 13.6f);
    arcp(p, 12.0f, 13.6f, 6.5f, 180.0f, 0.0f);
    bez(p, 18.5f, 12.0f, 17.0f, 8.0f, 12.0f, 3.0f);
    end_closed(p);
}

static void build_invert_fill(Pen *s) {
    begin(s, 12.0f, 3.0f);
    bez(s, 7.0f, 8.0f, 5.5f, 12.0f, 5.5f, 13.6f);
    arcp(s, 12.0f, 13.6f, 6.5f, 180.0f, 90.0f);
    linep(s, 12.0f, 13.6f);
    linep(s, 12.0f, 3.0f);
    end_closed(s);
}

static void build_undo(Pen *p, float dir) {
    if (dir > 0.0f) {
        begin(p, 9.0f, 7.0f);
        linep(p, 5.0f, 11.0f);
        linep(p, 9.0f, 15.0f);
        end_open(p);
        begin(p, 5.0f, 11.0f);
        linep(p, 14.0f, 11.0f);
        arcp(p, 14.0f, 15.0f, 4.0f, 270.0f, 90.0f);
        end_open(p);
    } else {
        begin(p, 15.0f, 7.0f);
        linep(p, 19.0f, 11.0f);
        linep(p, 15.0f, 15.0f);
        end_open(p);
        begin(p, 19.0f, 11.0f);
        linep(p, 10.0f, 11.0f);
        arcp_rev(p, 10.0f, 15.0f, 4.0f, 270.0f, 90.0f);
        end_open(p);
    }
}

static void build_save(Pen *p) {
    begin(p, 12.0f, 4.0f);
    linep(p, 12.0f, 14.0f);
    end_open(p);
    begin(p, 8.0f, 10.5f);
    linep(p, 12.0f, 14.5f);
    linep(p, 16.0f, 10.5f);
    end_open(p);
    begin(p, 5.0f, 16.5f);
    linep(p, 5.0f, 20.0f);
    linep(p, 19.0f, 20.0f);
    linep(p, 19.0f, 16.5f);
    end_open(p);
}

/* quick save: arrow down into a folder, i.e. write next to the source */
static void build_save_near(Pen *p) {
    begin(p, 3.0f, 19.5f);
    linep(p, 3.0f, 15.0f);
    linep(p, 21.0f, 15.0f);
    linep(p, 21.0f, 19.5f);
    end_open(p);
    begin(p, 12.0f, 3.5f);
    linep(p, 12.0f, 11.5f);
    end_open(p);
    begin(p, 8.4f, 8.0f);
    linep(p, 12.0f, 11.6f);
    linep(p, 15.6f, 8.0f);
    end_open(p);
}

static void build_sun(Pen *p) {
    circle_path(p, 12.0f, 12.0f, 4.2f);
    {
        static const float dirs[8][2] = {
            {0.0f, -1.0f}, {0.7071f, -0.7071f}, {1.0f, 0.0f}, {0.7071f, 0.7071f},
            {0.0f, 1.0f}, {-0.7071f, 0.7071f}, {-1.0f, 0.0f}, {-0.7071f, -0.7071f}};
        int i;
        for (i = 0; i < 8; ++i) {
            begin(p, 12.0f + dirs[i][0] * 6.6f, 12.0f + dirs[i][1] * 6.6f);
            linep(p, 12.0f + dirs[i][0] * 8.6f, 12.0f + dirs[i][1] * 8.6f);
            end_open(p);
        }
    }
}

static void build_moon(Pen *p) {
    /* Crescent as a single closed contour: outer circle r=9 at (12,12) walked
       counter-clockwise through the left, inner circle r=8 at (16.5,12) walked
       clockwise back through the left.  Filling this contour gives the body. */
    begin(p, 16.14f, 4.02f);
    arcp_rev(p, 12.0f, 12.0f, 9.0f, -62.9f, -297.1f);
    arcp(p, 16.5f, 12.0f, 8.0f, 92.6f, 267.4f);
    end_closed(p);
}
static void build_fit(Pen *p) {
    begin(p, 4.0f, 9.0f);
    linep(p, 4.0f, 4.0f);
    linep(p, 9.0f, 4.0f);
    end_open(p);
    begin(p, 15.0f, 4.0f);
    linep(p, 20.0f, 4.0f);
    linep(p, 20.0f, 9.0f);
    end_open(p);
    begin(p, 20.0f, 15.0f);
    linep(p, 20.0f, 20.0f);
    linep(p, 15.0f, 20.0f);
    end_open(p);
    begin(p, 9.0f, 20.0f);
    linep(p, 4.0f, 20.0f);
    linep(p, 4.0f, 15.0f);
    end_open(p);
    begin(p, 9.0f, 9.0f);
    linep(p, 15.0f, 9.0f);
    linep(p, 15.0f, 15.0f);
    linep(p, 9.0f, 15.0f);
    end_closed(p);
}

static void build_actual(Pen *p) {
    begin(p, 5.0f, 5.0f);
    linep(p, 19.0f, 5.0f);
    linep(p, 19.0f, 19.0f);
    linep(p, 5.0f, 19.0f);
    end_closed(p);
    begin(p, 9.5f, 12.0f);
    linep(p, 14.5f, 12.0f);
    end_open(p);
    begin(p, 12.0f, 9.5f);
    linep(p, 12.0f, 14.5f);
    end_open(p);
}
static void build_check(Pen *p) {
    begin(p, 4.5f, 12.5f);
    linep(p, 9.8f, 17.8f);
    linep(p, 19.5f, 6.6f);
    end_open(p);
}

static void build_close(Pen *p) {
    begin(p, 6.0f, 6.0f);
    linep(p, 18.0f, 18.0f);
    end_open(p);
    begin(p, 18.0f, 6.0f);
    linep(p, 6.0f, 18.0f);
    end_open(p);
}

static void build_image(Pen *p) {
    /* Rounded picture frame with a mountain and a sun. */
    begin(p, 6.0f, 3.5f);
    linep(p, 18.0f, 3.5f);
    arcp(p, 18.0f, 6.0f, 2.5f, 270.0f, 0.0f);
    linep(p, 20.5f, 18.0f);
    arcp(p, 18.0f, 18.0f, 2.5f, 0.0f, 90.0f);
    linep(p, 6.0f, 20.5f);
    arcp(p, 6.0f, 18.0f, 2.5f, 90.0f, 180.0f);
    linep(p, 3.5f, 6.0f);
    arcp(p, 6.0f, 6.0f, 2.5f, 180.0f, 270.0f);
    end_closed(p);
    begin(p, 8.0f, 9.0f);
    p->sink->lpVtbl->AddLine(p->sink, pt(8.0f, 9.0f));
    {
        D2D1_ARC_SEGMENT dot;
        dot.point = pt(8.0f, 9.2f);
        dot.size.width = 1.4f;
        dot.size.height = 1.4f;
        dot.rotationAngle = 0.0f;
        dot.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
        dot.arcSize = D2D1_ARC_SIZE_SMALL;
        p->sink->lpVtbl->AddArc(p->sink, &dot);
    }
    end_open(p);
    begin(p, 5.0f, 17.5f);
    linep(p, 10.0f, 12.0f);
    linep(p, 14.0f, 16.0f);
    linep(p, 16.5f, 13.5f);
    linep(p, 20.0f, 17.5f);
    end_open(p);
}

/* MD3 "tune": three sliders, each with a knob ring. */
static void build_tune(Pen *p) {
    begin(p, 3.0f, 6.0f);
    linep(p, 21.0f, 6.0f);
    end_open(p);
    begin(p, 3.0f, 12.0f);
    linep(p, 21.0f, 12.0f);
    end_open(p);
    begin(p, 3.0f, 18.0f);
    linep(p, 21.0f, 18.0f);
    end_open(p);
    circle_path(p, 8.0f, 6.0f, 3.2f);
    circle_path(p, 16.0f, 12.0f, 3.2f);
    circle_path(p, 8.0f, 18.0f, 3.2f);
}
static void build_crop(Pen *p) {
    begin(p, 6.0f, 3.0f);
    linep(p, 6.0f, 18.0f);
    linep(p, 21.0f, 18.0f);
    end_open(p);
    begin(p, 3.0f, 6.0f);
    linep(p, 18.0f, 6.0f);
    linep(p, 18.0f, 21.0f);
    end_open(p);
}

/* ---------------------------------------------------------------- one-time */

static ID2D1PathGeometry *new_geometry(ID2D1Factory *factory, Pen *out) {
    ID2D1PathGeometry *geom = NULL;
    if (FAILED(factory->lpVtbl->CreatePathGeometry(factory, &geom)) ||
        geom == NULL) {
        return NULL;
    }
    if (FAILED(geom->lpVtbl->Open(geom, &out->sink))) {
        fus_release(geom);
        return NULL;
    }
    return geom;
}

static ID2D1PathGeometry *finish_geometry(ID2D1PathGeometry *geom, Pen *pen) {
    pen->sink->lpVtbl->Base.Close((ID2D1SimplifiedGeometrySink *)pen->sink);
    fus_release(pen->sink);
    pen->sink = NULL;
    return geom;
}

static void build_one(ID2D1Factory *factory, int icon,
                      void (*fn)(Pen *), bool needs_fill) {
    Pen pen;
    ID2D1PathGeometry *geom;
    Pen fill;
    ID2D1PathGeometry *fill_geom = NULL;

    memset(&pen, 0, sizeof(pen));
    geom = new_geometry(factory, &pen);
    if (geom == NULL) {
        return;
    }
    if (needs_fill) {
        memset(&fill, 0, sizeof(fill));
        fill_geom = new_geometry(factory, &fill);
    }
    if (icon == ICON_CONTRAST) {
        build_contrast(&pen);
        if (fill_geom != NULL) {
            build_contrast_fill(&fill);
        }
    } else if (icon == ICON_INVERT) {
        build_invert(&pen);
        if (fill_geom != NULL) {
            build_invert_fill(&fill);
        }
    } else if (icon == ICON_MOON) {
        /* a crescent reads far better solid than as an outline */
        build_moon(&pen);
        if (fill_geom != NULL) {
            build_moon(&fill);
        }
    } else if (fn != NULL) {
        fn(&pen);
    }
    finish_geometry(geom, &pen);
    if (fill_geom != NULL) {
        finish_geometry(fill_geom, &fill);
    }
    g_icons[icon].stroke = geom;
    g_icons[icon].fill = fill_geom;
}

static void build_rotate_cw(Pen *p) { build_rotate(p, 1.0f, 0); }
static void build_rotate_ccw(Pen *p) { build_rotate(p, -1.0f, 0); }
static void build_rotate_cw_180(Pen *p) { build_rotate(p, 1.0f, 1); }
static void build_rotate_ccw_180(Pen *p) { build_rotate(p, -1.0f, 1); }
static void build_undo_icon(Pen *p) { build_undo(p, 1.0f); }
static void build_redo_icon(Pen *p) { build_undo(p, -1.0f); }

ID2D1PathGeometry **icons_get(ID2D1Factory *factory) {
    if (g_icons_ready) {
        return NULL;
    }
    build_one(factory, ICON_OPEN, build_open, false);
    build_one(factory, ICON_ROTATE_CW, build_rotate_cw, false);
    build_one(factory, ICON_ROTATE_CCW, build_rotate_ccw, false);
    build_one(factory, ICON_ROTATE_CW_180, build_rotate_cw_180, false);
    build_one(factory, ICON_ROTATE_CCW_180, build_rotate_ccw_180, false);
    build_one(factory, ICON_FIT, build_fit, false);
    build_one(factory, ICON_ACTUAL, build_actual, false);
    build_one(factory, ICON_FLIP_H, build_flip_h, false);
    build_one(factory, ICON_FLIP_V, build_flip_v, false);
    build_one(factory, ICON_CONTRAST, NULL, true);
    build_one(factory, ICON_INVERT, NULL, true);
    build_one(factory, ICON_UNDO, build_undo_icon, false);
    build_one(factory, ICON_REDO, build_redo_icon, false);
    build_one(factory, ICON_SAVE, build_save, false);
    build_one(factory, ICON_SAVE_NEAR, build_save_near, false);
    build_one(factory, ICON_SUN, build_sun, false);
    build_one(factory, ICON_MOON, NULL, true);
    build_one(factory, ICON_CHECK, build_check, false);
    build_one(factory, ICON_CLOSE, build_close, false);
    build_one(factory, ICON_IMAGE, build_image, false);
    build_one(factory, ICON_CROP, build_crop, false);
    build_one(factory, ICON_TUNE, build_tune, false);
    g_icons_ready = true;
    return NULL;
}

void icons_release(void) {
    int i;
    for (i = 0; i < ICON_COUNT; ++i) {
        if (g_icons[i].stroke != NULL) {
            fus_release(g_icons[i].stroke);
            g_icons[i].stroke = NULL;
        }
        if (g_icons[i].fill != NULL) {
            fus_release(g_icons[i].fill);
            g_icons[i].fill = NULL;
        }
    }
    g_icons_ready = false;
}

/* ------------------------------------------------------------------ drawing */

static void set_solid(FusApp *app, D2D1_COLOR_F color) {
    app->brush->lpVtbl->SetColor(app->brush, &color);
}

void draw_icon(FusApp *app, int icon, D2D1_RECT_F box, D2D1_COLOR_F color) {
    float size = box.right - box.left;
    float scale;
    D2D1_MATRIX_3X2_F saved;
    D2D1_MATRIX_3X2_F m;
    ID2D1PathGeometry *geom;

    if (icon < 0 || icon >= ICON_COUNT) {
        return;
    }
    geom = g_icons[icon].stroke;
    if (geom == NULL) {
        return;
    }
    scale = size / 24.0f;
    RT(app)->lpVtbl->GetTransform(RT(app), &saved);
    m._11 = scale;
    m._12 = 0.0f;
    m._21 = 0.0f;
    m._22 = scale;
    m._31 = box.left + (size - 24.0f * scale) * 0.5f;
    m._32 = box.top + (size - 24.0f * scale) * 0.5f;
    RT(app)->lpVtbl->SetTransform(RT(app), &m);

    set_solid(app, color);
    RT(app)->lpVtbl->DrawGeometry(RT(app), (ID2D1Geometry *)geom,
                                  (ID2D1Brush *)app->brush, 2.0f, NULL);
    if (g_icons[icon].fill != NULL) {
        RT(app)->lpVtbl->FillGeometry(RT(app), (ID2D1Geometry *)g_icons[icon].fill,
                                      (ID2D1Brush *)app->brush, NULL);
    }
    RT(app)->lpVtbl->SetTransform(RT(app), &saved);
}
