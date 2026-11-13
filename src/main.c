/* main.c — window, message loop, and the actions behind the interface.

   Startup order is the whole point of this file.  Measured facts from
   bench/STARTUP-ANALYSIS.md: creating a *hardware* D3D render target costs
   250-340 ms on a hybrid-GPU laptop (one-time driver init), while the WARP
   software target costs 30-55 ms and paints the same pixels.  So:

     1. factories + text formats      (~12 ms; no COM, no D2D device yet)
     2. hidden window -> ShowWindow   (~25 ms; WM_ERASEBKGND fills the surface
                                       colour, so the window is never blank)
     3. CreateHwndRenderTarget (WARP)(~30-55 ms)
     4. first paint + EndDraw         (~25-40 ms)

   Everything expensive that the app only needs later - COM, WIC, file
   dialogs, full-resolution decode - is deferred past the first frame and
   runs on worker threads so the UI never blocks. */

#define WIN32_LEAN_AND_MEAN
#include "app.h"

#include <dwmapi.h>
#include <process.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <wchar.h>
#include <wincodec.h>
#include <windowsx.h>

FusApp g_app;

static HBRUSH g_erase_brush;
static HCURSOR g_cursor_arrow;
static HCURSOR g_cursor_hand;
static HCURSOR g_cursor_cross;
static bool g_tracking_leave;

/* Optional timing log (set FUST_TIMING=<file> to dump phase timings). */
static FILE *g_tlog;
static uint64_t g_t0;

void fus_log_hr(const char *label, long hr) {
    if (g_tlog == NULL) {
        return;
    }
    fprintf(g_tlog, "%.2f\t%s %ld\n", (double)(GetTickCount64() - g_t0), label, hr);
    fflush(g_tlog);
}

static void tlog(const char *label) {
    if (g_tlog == NULL) {
        return;
    }
    fprintf(g_tlog, "%.2f\t%s\n",
            (double)(GetTickCount64() - g_t0), label);
    fflush(g_tlog);
}

static void tlog_open(void) {
    wchar_t path[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"FUST_TIMING", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return;
    }
    g_t0 = GetTickCount64();
    g_tlog = _wfopen(path, L"w");
}
static int g_debug_title;

#define TIMER_ANIM 1
#define WM_APP_RESULT (WM_APP + 1)

/* Nodes lose their preview when the window is minimised or the D2D device is
   lost; the RGB data stays in memory, so recovery is a text format rebuild at
   worst. */
enum { RES_LOADED = 1, RES_FILTERED, RES_EXPORTED };

#define PREVIEW_MAX 1600u

typedef struct {
    int kind;
    int gen;
    int upgrade;                 /* hi-res pass on top of the preview */
    bool ok;
    LumaImage image;
    wchar_t error[256];
    wchar_t path[MAX_PATH];
} Result;

typedef struct {
    int kind;
    int gen;
    int upgrade;
    HWND hwnd;
    wchar_t path[MAX_PATH];
    wchar_t out_path[MAX_PATH];
    UINT max_side;
    bool gray;
    bool invert;
    LumaImage copy;              /* filter job: mutable copy of the preview */
    int rotation;
    bool mirror_h;
    bool mirror_v;
    LumaCrop crop;
} Job;

/* IID_IDWriteFactory is not exported by the linker script of every mingw
   build, so the interface id is spelled out here. */
static const IID k_iid_dwrite_factory = {
    0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

/* ------------------------------------------------------------------ helpers */

static uint64_t now64(void) {
    return GetTickCount64();
}

static void set_erase_brush(FusApp *app) {
    D2D1_COLOR_F c = app->theme.surface;
    HBRUSH brush = CreateSolidBrush(
        RGB((BYTE)(c.r * 255.0f + 0.5f), (BYTE)(c.g * 255.0f + 0.5f),
            (BYTE)(c.b * 255.0f + 0.5f)));
    if (brush != NULL) {
        if (g_erase_brush != NULL) {
            DeleteObject(g_erase_brush);
        }
        g_erase_brush = brush;
    }
}

/* Image-space point -> dp point on screen, using the same matrix as ui.c. */
static void image_to_screen(FusApp *app, float ix, float iy, float *sx,
                            float *sy) {
    D2D1_MATRIX_3X2_F m = ui_image_matrix(app);
    *sx = m._11 * ix + m._21 * iy + m._31;
    *sy = m._12 * ix + m._22 * iy + m._32;
}

/* Duration of the theme cross-fade; overridable for slow-motion previews. */
static float theme_fade_ms(void) {
    static int cached;
    static float value;
    if (!cached && value == 0.0f) {
        const wchar_t *env = _wgetenv(L"FUST_THEME_MS");
        int v = env != NULL ? _wtoi(env) : 0;
        value = v > 0 ? (float)v : 260.0f;
        if (v <= 0) {
            cached = 1;
        }
    }
    return value;
}

static void tick(FusApp *app) {
    app->now_ms = now64();
    if (app->theme_anim) {
        float a = (float)(app->now_ms - app->theme_ms) / theme_fade_ms();
        float e;
        if (a >= 1.0f) {
            a = 1.0f;
            app->theme_anim = false;
            app->theme_mix = 1.0f;
        } else {
            if (a < 0.0f) {
                a = 0.0f;
            }
            app->theme_mix = a;
        }
        /* emphasized decelerate, the same easing the rest of the UI uses */
        e = 1.0f - (1.0f - a) * (1.0f - a) * (1.0f - a);
        theme_blend(&app->theme, &app->theme_from, &app->theme_to, e);
    }
    if (app->fade_active) {
        float a = (float)(app->now_ms - app->fade_start_ms) / 220.0f;
        if (a >= 1.0f) {
            app->fade = 1.0f;
            app->fade_active = false;
        } else {
            app->fade = a < 0.0f ? 0.0f : a;
        }
    }
    if (app->snackbar[0] != L'\0' && app->now_ms >= app->snackbar_until_ms) {
        app->snackbar[0] = L'\0';
        app->snackbar_action[0] = L'\0';
        app->snackbar_action_id = ACT_NONE;
    }
}

static void ensure_timer(FusApp *app) {
    if (ui_animation_active(app)) {
        SetTimer(app->hwnd, TIMER_ANIM, 16, NULL);
    } else {
        KillTimer(app->hwnd, TIMER_ANIM);
    }
}

/* --------------------------------------------------------------- undo stack */

static FusDocState state_now(const FusApp *app) {
    FusDocState s;
    s.rotation = app->rotation;
    s.mirror_h = app->mirror_h;
    s.mirror_v = app->mirror_v;
    s.grayscale = app->grayscale;
    s.invert = app->invert;
    s.crop = app->crop;
    s.crop_preset = app->crop_preset;
    return s;
}

/* Records the *current* state, i.e. call before mutating. */
/* Record the *current* document state.  The undo array always holds states,
   newest last, so undo/redo are plain moves along it. */
static void history_commit(FusApp *app) {
    FusDocState s = state_now(app);
    int cap = (int)(sizeof app->undo / sizeof app->undo[0]);

    app->undo_len = app->undo_pos + 1;
    if (app->undo_len >= cap) {
        memmove(&app->undo[0], &app->undo[1],
                sizeof app->undo[0] * (size_t)(cap - 1));
        app->undo_len = cap - 1;
    }
    app->undo[app->undo_len] = s;
    app->undo_len++;
    app->undo_pos = app->undo_len - 1;
}

/* ------------------------------------------------------------ async loading */

static bool image_clone(const LumaImage *src, LumaImage *dst) {
    dst->width = src->width;
    dst->height = src->height;
    dst->stride = src->stride;
    dst->pixels = (uint8_t *)malloc((size_t)src->stride * src->height);
    if (dst->pixels == NULL) {
        dst->width = dst->height = dst->stride = 0;
        return false;
    }
    memcpy(dst->pixels, src->pixels, (size_t)src->stride * src->height);
    return true;
}

static unsigned __stdcall worker_main(void *arg) {
    Job *job = (Job *)arg;
    Result *res = (Result *)calloc(1, sizeof(Result));

    if (res == NULL) {
        free(job);
        return 0;
    }
    res->kind = job->kind;
    res->gen = job->gen;
    res->upgrade = job->upgrade;

    if (job->kind == RES_LOADED) {
        res->ok = image_load_scaled(job->path, job->max_side, &res->image,
                                    res->error,
                                    sizeof res->error / sizeof res->error[0]);
        wcsncpy(res->path, job->path, MAX_PATH - 1);
    } else if (job->kind == RES_FILTERED) {
        image_apply_filters(&job->copy, job->gray, job->invert);
        res->image = job->copy;
        res->ok = true;
    } else {
        LumaImage full;
        ZeroMemory(&full, sizeof full);
        res->ok = luma_image_load(job->path, &full, res->error,
                                  sizeof res->error / sizeof res->error[0]);
        if (res->ok) {
            image_apply_filters(&full, job->gray, job->invert);
            res->ok = luma_image_export(&full, job->rotation, job->mirror_h,
                                        job->mirror_v, job->crop,
                                        job->out_path, res->error,
                                        sizeof res->error / sizeof res->error[0]);
            luma_image_free(&full);
        }
        wcsncpy(res->path, job->out_path, MAX_PATH - 1);
    }

    if (res->ok || res->kind != RES_FILTERED) {
        PostMessageW(job->hwnd, WM_APP_RESULT, 0, (LPARAM)res);
    } else {
        free(res);
    }
    free(job);
    return 0;
}

static Job *job_new(int kind, int gen, HWND hwnd) {
    Job *job = (Job *)calloc(1, sizeof(Job));
    if (job != NULL) {
        job->kind = kind;
        job->gen = gen;
        job->hwnd = hwnd;
    }
    return job;
}

static void start_load(const wchar_t *path) {
    FusApp *app = &g_app;
    Job *job = job_new(RES_LOADED, ++app->load_gen, app->hwnd);

    if (job == NULL) {
        return;
    }
    job->max_side = PREVIEW_MAX;
    wcsncpy(job->path, path, MAX_PATH - 1);
    app->loading = true;
    tlog("load_start");
    app->load_start_ms = now64();
    app->cropping = false;
    _beginthreadex(NULL, 0, worker_main, job, 0, NULL);
    ui_invalidate(app);
    ensure_timer(app);
}

/* Second pass: decode the file at full size in the background so 100% zoom is
   crisp.  The preview stays on screen until the better pixels arrive. */
static void start_upgrade(const wchar_t *path, int gen) {
    FusApp *app = &g_app;
    Job *job = job_new(RES_LOADED, gen, app->hwnd);

    if (job == NULL) {
        return;
    }
    job->upgrade = 1;
    job->max_side = 8192u;
    wcsncpy(job->path, path, MAX_PATH - 1);
    _beginthreadex(NULL, 0, worker_main, job, 0, NULL);
}

static void start_filter(void) {
    FusApp *app = &g_app;
    Job *job;

    if (app->have_base) {
        job = job_new(RES_FILTERED, app->load_gen, app->hwnd);
        if (job == NULL) {
            return;
        }
        if (!image_clone(&app->base, &job->copy)) {
            free(job);
            return;
        }
        job->gray = app->grayscale;
        job->invert = app->invert;
        _beginthreadex(NULL, 0, worker_main, job, 0, NULL);
    } else if (app->bitmap != NULL) {
        fus_release(app->bitmap);
        app->bitmap = NULL;
        ui_invalidate(app);
    }
}

static void start_export(const wchar_t *path) {
    FusApp *app = &g_app;
    Job *job;

    if (!app->has_image) {
        return;
    }
    job = job_new(RES_EXPORTED, app->load_gen, app->hwnd);
    if (job == NULL) {
        return;
    }
    wcsncpy(job->path, app->path, MAX_PATH - 1);
    wcsncpy(job->out_path, path, MAX_PATH - 1);
    job->rotation = app->rotation;
    job->mirror_h = app->mirror_h;
    job->mirror_v = app->mirror_v;
    job->gray = app->grayscale;
    job->invert = app->invert;
    job->crop = app->crop;
    app->exporting = true;
    app->export_start_ms = now64();
    _beginthreadex(NULL, 0, worker_main, job, 0, NULL);
    ui_invalidate(app);
    ensure_timer(app);
}

/* ------------------------------------------------------------- device setup */

static void discard_target(FusApp *app) {
    if (app->checker != NULL) {
        fus_release(app->checker);
        app->checker = NULL;
    }
    if (app->bitmap != NULL) {
        fus_release(app->bitmap);
        app->bitmap = NULL;
    }
    if (app->brush != NULL) {
        fus_release(app->brush);
        app->brush = NULL;
    }
    if (app->rt != NULL) {
        fus_release(app->rt);
        app->rt = NULL;
    }
}

static bool upload_bitmap(FusApp *app, const LumaImage *img) {
    D2D1_BITMAP_PROPERTIES props;
    D2D1_SIZE_U size;
    HRESULT hr;

    if (app->bitmap != NULL) {
        fus_release(app->bitmap);
        app->bitmap = NULL;
    }
    if (app->rt == NULL || img == NULL || img->pixels == NULL) {
        return false;
    }
    ZeroMemory(&props, sizeof props);
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX = 96.0f;
    props.dpiY = 96.0f;
    size.width = img->width;
    size.height = img->height;
    hr = RT(app)->lpVtbl->CreateBitmap(RT(app), size, img->pixels, img->stride,
                                       &props, &app->bitmap);
    return SUCCEEDED(hr);
}

/* Paints a filtered copy without touching the pristine preview in app->base. */
static void upload_filtered(FusApp *app) {
    LumaImage copy;

    if (!app->have_base || !upload_bitmap(app, &app->base)) {
        return;
    }
    if (!app->grayscale && !app->invert) {
        ui_invalidate(app);
        return;
    }
    if (image_clone(&app->base, &copy)) {
        image_apply_filters(&copy, app->grayscale, app->invert);
        upload_bitmap(app, &copy);
        luma_image_free(&copy);
    }
    ui_invalidate(app);
}

static HRESULT create_target(FusApp *app) {
    D2D1_RENDER_TARGET_PROPERTIES props;
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_props;
    D2D1_SIZE_U pixel_size;
    RECT rc;
    float scale = app->dpi / 96.0f;
    HRESULT hr;

    GetClientRect(app->hwnd, &rc);
    if (rc.right <= rc.left || rc.bottom <= rc.top) {
        return E_FAIL;
    }
    ZeroMemory(&props, sizeof props);
    props.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.dpiX = 96.0f * scale;
    props.dpiY = 96.0f * scale;
    props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    pixel_size.width = (UINT)(rc.right - rc.left);
    pixel_size.height = (UINT)(rc.bottom - rc.top);
    hwnd_props.hwnd = app->hwnd;
    hwnd_props.pixelSize = pixel_size;
    hwnd_props.presentOptions = D2D1_PRESENT_OPTIONS_NONE;

    hr = app->d2d->lpVtbl->CreateHwndRenderTarget(app->d2d, &props,
                                                  &hwnd_props, &app->rt);
    if (FAILED(hr)) {
        app->rt = NULL;
        return hr;
    }
    RT(app)->lpVtbl->SetAntialiasMode(RT(app), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    RT(app)->lpVtbl->SetTextAntialiasMode(RT(app),
                                          D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    if (app->brush == NULL) {
        D2D1_COLOR_F black = {0.0f, 0.0f, 0.0f, 1.0f};
        hr = RT(app)->lpVtbl->CreateSolidColorBrush(RT(app), &black, NULL,
                                                    &app->brush);
        if (FAILED(hr)) {
            return hr;
        }
    }
    if (app->have_base) {
        upload_filtered(app);
    }
    icons_get(app->d2d);
    return S_OK;
}

static void update_title(FusApp *app) {
    wchar_t title[300];

    if (g_debug_title) {
        return;
    }
    if (app->has_image && app->name[0] != L'\0') {
        _snwprintf(title, 300, L"%s - Fustimage", app->name);
    } else {
        _snwprintf(title, 300, L"Fustimage");
    }
    SetWindowTextW(app->hwnd, title);
}

static void paint_client(FusApp *app) {
    if (app->rt == NULL) {
        return;
    }
    tick(app);
    ui_paint(app);
    tlog(app->dark ? "paint_dark" : "paint_light");

    if (g_debug_title) {
        wchar_t buf[256];
        _snwprintf(buf, 256,
                 L"[dbg] fit=%d zoom=%.3f crop=%.3f,%.3f,%.3f,%.3f pre=%d "
                 L"cr=%d pan=%.1f,%.1f cw=%.3f,%.3f,%.3f,%.3f drag=%d rot=%d dark=%d",
                 app->fit ? 1 : 0, (double)app->zoom, (double)app->crop.left,
                 (double)app->crop.top, (double)app->crop.right,
                 (double)app->crop.bottom, app->crop_preset,
                 app->cropping ? 1 : 0, (double)app->pan_x,
                 (double)app->pan_y, (double)app->crop_work[0],
                 (double)app->crop_work[1], (double)app->crop_work[2],
                 (double)app->crop_work[3], app->panning ? 1 : 0,
                 (int)app->rotation, app->dark ? 1 : 0);
        SetWindowTextW(app->hwnd, buf);
    }
}

/* ------------------------------------------------------------------- theme */

static void toggle_theme(void) {
    FusApp *app = &g_app;
    BOOL dark;

    app->theme_from = app->theme;
    app->dark = !app->dark;
    theme_build(&app->theme_to, app->dark);
    app->theme_anim = true;
    app->theme_mix = 0.0f;
    app->theme_ms = now64();
    set_erase_brush(app);
    if (app->checker != NULL) {
        fus_release(app->checker);
        app->checker = NULL;
    }
    dark = app->dark ? TRUE : FALSE;
    DwmSetWindowAttribute(app->hwnd, 20, &dark, sizeof dark);
    tlog(app->dark ? "theme_dark" : "theme_light");
    ensure_timer(app);
    InvalidateRect(app->hwnd, NULL, FALSE);
}

/* --------------------------------------------------------------- dialogs */

static void ensure_com(void) {
    static bool initialized = false;
    if (!initialized) {
        if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
                                             COINIT_DISABLE_OLE1DDE))) {
            initialized = true;
        }
    }
}

static bool pick_open(wchar_t *out, size_t cap) {
    IFileOpenDialog *dialog = NULL;
    IShellItem *item = NULL;
    PWSTR path = NULL;
    COMDLG_FILTERSPEC types[2];
    bool ok = false;

    ensure_com();
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL,
                                CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog,
                                (void **)&dialog))) {
        return false;
    }
    types[0].pszName = L"图片";
    types[0].pszSpec = L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff";
    types[1].pszName = L"所有文件";
    types[1].pszSpec = L"*.*";
    dialog->lpVtbl->SetFileTypes(dialog, 2, types);
    if (SUCCEEDED(dialog->lpVtbl->Show(dialog, g_app.hwnd)) &&
        SUCCEEDED(dialog->lpVtbl->GetResult(dialog, &item))) {
        if (SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH,
                                                  &path)) &&
            path != NULL) {
            wcsncpy(out, path, cap - 1);
            out[cap - 1] = L'\0';
            ok = true;
            CoTaskMemFree(path);
        }
        fus_release(item);
    }
    fus_release(dialog);
    return ok;
}

static const wchar_t *extension_for_type(UINT type_index) {
    switch (type_index) {
    case 2: return L".jpg";
    case 3: return L".bmp";
    case 4: return L".tif";
    default: return L".png";
    }
}

static bool pick_save(wchar_t *out, size_t cap) {
    IFileSaveDialog *dialog = NULL;
    IShellItem *item = NULL;
    PWSTR path = NULL;
    COMDLG_FILTERSPEC types[4];
    UINT type_index = 1;
    bool ok = false;

    ensure_com();
    if (FAILED(CoCreateInstance(&CLSID_FileSaveDialog, NULL,
                                CLSCTX_INPROC_SERVER, &IID_IFileSaveDialog,
                                (void **)&dialog))) {
        return false;
    }
    types[0].pszName = L"PNG";
    types[0].pszSpec = L"*.png";
    types[1].pszName = L"JPEG";
    types[1].pszSpec = L"*.jpg;*.jpeg";
    types[2].pszName = L"BMP";
    types[2].pszSpec = L"*.bmp";
    types[3].pszName = L"TIFF";
    types[3].pszSpec = L"*.tif;*.tiff";
    dialog->lpVtbl->SetFileTypes(dialog, 4, types);
    dialog->lpVtbl->SetDefaultExtension(dialog, L"png");
    {
        wchar_t suggestion[MAX_PATH];
        const wchar_t *dot;

        wcsncpy(suggestion, g_app.name[0] != L'\0' ? g_app.name : L"image",
                MAX_PATH - 1);
        suggestion[MAX_PATH - 1] = L'\0';
        dot = wcsrchr(suggestion, L'.');
        if (dot != NULL && wcslen(dot) <= 5) {
            suggestion[dot - suggestion] = L'\0';
        }
        dialog->lpVtbl->SetFileName(dialog, suggestion);
    }
    if (g_app.path[0] != L'\0') {
        wchar_t folder_path[MAX_PATH];
        wcsncpy(folder_path, g_app.path, MAX_PATH - 1);
        folder_path[MAX_PATH - 1] = L'\0';
        {
            wchar_t *slash = wcsrchr(folder_path, L'\\');
            if (slash != NULL) {
                IShellItem *folder = NULL;
                *slash = L'\0';
                if (SUCCEEDED(SHCreateItemFromParsingName(
                        folder_path, NULL, &IID_IShellItem, (void **)&folder))) {
                    dialog->lpVtbl->SetDefaultFolder(dialog, folder);
                    fus_release(folder);
                }
            }
        }
    }
    if (SUCCEEDED(dialog->lpVtbl->Show(dialog, g_app.hwnd))) {
        dialog->lpVtbl->GetFileTypeIndex(dialog, &type_index);
        if (SUCCEEDED(dialog->lpVtbl->GetResult(dialog, &item)) &&
            SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH,
                                                  &path)) &&
            path != NULL) {
            const wchar_t *ext = extension_for_type(type_index);
            size_t len = wcslen(path);
            wcsncpy(out, path, cap - 1);
            out[cap - 1] = L'\0';
            if (len < cap - 6) {
                bool has_ext = false;
                const wchar_t *dot = wcsrchr(out, L'.');
                if (dot != NULL && wcslen(dot) <= 5) {
                    has_ext = (_wcsicmp(dot, L".png") == 0) ||
                              (_wcsicmp(dot, L".jpg") == 0) ||
                              (_wcsicmp(dot, L".jpeg") == 0) ||
                              (_wcsicmp(dot, L".bmp") == 0) ||
                              (_wcsicmp(dot, L".tif") == 0) ||
                              (_wcsicmp(dot, L".tiff") == 0);
                }
                if (!has_ext) {
                    wcscat(out, ext);
                }
            }
            ok = true;
            CoTaskMemFree(path);
        }
        fus_release(item);
    }
    fus_release(dialog);
    return ok;
}

/* -------------------------------------------------------------- crop logic */

static void oriented_dims(const FusApp *app, float *w, float *h) {
    if ((app->rotation & 1) != 0) {
        *w = app->img_h;
        *h = app->img_w;
    } else {
        *w = app->img_w;
        *h = app->img_h;
    }
}

static void apply_crop_preset(int index) {
    FusApp *app = &g_app;
    float ow, oh, ratio;

    if (!app->has_image || index < 0 || index >= g_preset_count) {
        return;
    }
    if (!app->cropping) {
        app->crop_before[0] = app->crop.left;
        app->crop_before[1] = app->crop.top;
        app->crop_before[2] = app->crop.right;
        app->crop_before[3] = app->crop.bottom;
        app->crop_before_preset = app->crop_preset;
    }
    app->crop_preset = index;
    ratio = g_presets[index].ratio;
    if (ratio <= 0.0f) {
        /* free: keep what is on screen, the user drags from there */
        app->crop_work[0] = app->cropping ? app->crop_work[0] : app->crop.left;
        app->crop_work[1] = app->cropping ? app->crop_work[1] : app->crop.top;
        app->crop_work[2] = app->cropping ? app->crop_work[2] : app->crop.right;
        app->crop_work[3] = app->cropping ? app->crop_work[3] : app->crop.bottom;
        app->cropping = true;
        app->crop_grip = 0;
        ui_invalidate(app);
        return;
    }
    oriented_dims(app, &ow, &oh);
    {
        float cw = 1.0f, ch = 1.0f;
        float image_ratio = ow / oh;
        if (image_ratio > ratio) {
            cw = ratio / image_ratio;
        } else {
            ch = image_ratio / ratio;
        }
        app->crop_work[0] = (1.0f - cw) * 0.5f;
        app->crop_work[1] = (1.0f - ch) * 0.5f;
        app->crop_work[2] = app->crop_work[0] + cw;
        app->crop_work[3] = app->crop_work[1] + ch;
    }
    app->cropping = true;
    app->crop_grip = 0;
    ui_invalidate(app);
}

/* ------------------------------------------------------------- interaction */

static void rotate_by(int quarters) {
    FusApp *app = &g_app;
    if (!app->has_image) {
        return;
    }
    app->rotation = (app->rotation + quarters) & 3;
    /* Crop rectangles do not survive a 90 degree turn, so start over. */
    app->crop.left = 0.0f;
    app->crop.top = 0.0f;
    app->crop.right = 1.0f;
    app->crop.bottom = 1.0f;
    app->crop_preset = 0;
    app->cropping = false;
    history_commit(app);
    ui_invalidate(app);
}

static void flip_axis(bool horizontal) {
    FusApp *app = &g_app;
    if (!app->has_image) {
        return;
    }
    if (horizontal) {
        app->mirror_h = !app->mirror_h;
    } else {
        app->mirror_v = !app->mirror_v;
    }
    app->crop.left = 0.0f;
    app->crop.top = 0.0f;
    app->crop.right = 1.0f;
    app->crop.bottom = 1.0f;
    app->crop_preset = 0;
    app->cropping = false;
    history_commit(app);
    ui_invalidate(app);
}

static void toggle_filter(bool grayscale) {
    FusApp *app = &g_app;
    if (!app->has_image) {
        return;
    }
    if (grayscale) {
        app->grayscale = !app->grayscale;
    } else {
        app->invert = !app->invert;
    }
    history_commit(app);
    start_filter();
    ui_invalidate(app);
}

static void zoom_at(float sx, float sy, float factor) {
    FusApp *app = &g_app;
    float ix, iy, nsx, nsy, zoom;

    if (!app->has_image || !ui_screen_to_image(app, sx, sy, &ix, &iy)) {
        return;
    }
    zoom = ui_effective_zoom(app) * factor;
    if (zoom < 0.05f) zoom = 0.05f;
    if (zoom > 8.0f) zoom = 8.0f;
    app->fit = false;
    app->zoom = zoom;
    image_to_screen(app, ix, iy, &nsx, &nsy);
    app->pan_x += sx - nsx;
    app->pan_y += sy - nsy;
    ui_invalidate(app);
}

static bool canvas_contains(const FusApp *app, float x, float y) {
    return x >= app->canvas.left && x < app->canvas.right && y >= app->canvas.top &&
           y < app->canvas.bottom;
}

/* Oriented normalized point (0..1 across the rotated image) under the cursor. */
static bool screen_to_oriented(FusApp *app, float sx, float sy, float *nx,
                               float *ny) {
    D2D1_MATRIX_3X2_F m = ui_oriented_matrix(app);
    float ow, oh;
    oriented_dims(app, &ow, &oh);
    if (ow <= 0.0f || oh <= 0.0f || fabsf(m._11) < 1e-6f ||
        fabsf(m._22) < 1e-6f) {
        return false;
    }
    *nx = ((sx - m._31) / m._11) / ow;
    *ny = ((sy - m._32) / m._22) / oh;
    return true;
}

static void oriented_to_screen(FusApp *app, float nx, float ny, float *sx,
                               float *sy) {
    D2D1_MATRIX_3X2_F m = ui_oriented_matrix(app);
    float ow, oh;
    oriented_dims(app, &ow, &oh);
    *sx = m._11 * (nx * ow) + m._31;
    *sy = m._22 * (ny * oh) + m._32;
}

/* Which part of the pending crop rectangle is under the cursor: 0 nothing,
   1 inside (move), 2/3 left/right, 4/5 top/bottom, 6..9 corners. */
static int crop_grip_at(FusApp *app, float x, float y) {
    float l, t, r, b;
    float tol = 14.0f;
    bool near_l, near_r, near_t, near_b, inside_x, inside_y;

    oriented_to_screen(app, app->crop_work[0], app->crop_work[1], &l, &t);
    oriented_to_screen(app, app->crop_work[2], app->crop_work[3], &r, &b);
    near_l = fabsf(x - l) <= tol;
    near_r = fabsf(x - r) <= tol;
    near_t = fabsf(y - t) <= tol;
    near_b = fabsf(y - b) <= tol;
    inside_x = x >= l - tol && x <= r + tol;
    inside_y = y >= t - tol && y <= b + tol;

    if (near_l && near_t) return 6;
    if (near_r && near_t) return 7;
    if (near_l && near_b) return 8;
    if (near_r && near_b) return 9;
    if (near_l && inside_y) return 2;
    if (near_r && inside_y) return 3;
    if (near_t && inside_x) return 4;
    if (near_b && inside_x) return 5;
    if (x > l && x < r && y > t && y < b) return 1;
    return 0;
}

/* Pull the dragged rectangle back to the locked ratio, anchored on the corner
   or edge that is not moving. */
static void crop_lock_ratio(FusApp *app, float *l, float *t, float *r,
                            float *b) {
    float ow, oh, target, pw, ph, cur;
    int grip = app->crop_grip;
    bool right_fixed = (grip == 2 || grip == 6 || grip == 8);
    bool bottom_fixed = (grip == 4 || grip == 6 || grip == 7);

    if (app->crop_preset <= 0 || app->crop_preset >= g_preset_count) {
        return;
    }
    target = g_presets[app->crop_preset].ratio;
    if (target <= 0.0f) {
        return;
    }
    oriented_dims(app, &ow, &oh);
    if (ow <= 0.0f || oh <= 0.0f) {
        return;
    }
    pw = (*r - *l) * ow;
    ph = (*b - *t) * oh;
    if (ph <= 0.0f) {
        return;
    }
    cur = pw / ph;
    if (cur > target) {
        float w = (ph * target) / ow;
        if (right_fixed) {
            *l = *r - w;
        } else {
            *r = *l + w;
        }
    } else {
        float h = (pw / target) / oh;
        if (bottom_fixed) {
            *t = *b - h;
        } else {
            *b = *t + h;
        }
    }
}

static void crop_begin(FusApp *app, float x, float y) {
    int grip = crop_grip_at(app, x, y);
    if (grip == 0) {
        /* outside the frame: start a brand new rectangle from the press point */
        if (!screen_to_oriented(app, x, y, &app->drag_ax, &app->drag_ay)) {
            return;
        }
        app->crop_grip = 10;
        app->drag_rect[0] = app->drag_rect[2] = app->drag_ax;
        app->drag_rect[1] = app->drag_rect[3] = app->drag_ay;
        app->crop_work[0] = app->drag_ax;
        app->crop_work[1] = app->drag_ay;
        app->crop_work[2] = app->drag_ax;
        app->crop_work[3] = app->drag_ay;
    } else {
        if (!screen_to_oriented(app, x, y, &app->drag_ax, &app->drag_ay)) {
            return;
        }
        app->crop_grip = grip;
        app->drag_rect[0] = app->crop_work[0];
        app->drag_rect[1] = app->crop_work[1];
        app->drag_rect[2] = app->crop_work[2];
        app->drag_rect[3] = app->crop_work[3];
    }
    app->panning = true; /* reuse as the crop-drag flag */
    ui_invalidate(app);
}

static void crop_update(FusApp *app, float x, float y) {
    float nx, ny, dx, dy, l, t, r, b;

    if (!screen_to_oriented(app, x, y, &nx, &ny)) {
        return;
    }
    dx = nx - app->drag_ax;
    dy = ny - app->drag_ay;

    l = app->drag_rect[0];
    t = app->drag_rect[1];
    r = app->drag_rect[2];
    b = app->drag_rect[3];

    if (app->crop_grip == 10) {
        /* drawing a fresh rectangle */
        float nl = nx < app->drag_ax ? nx : app->drag_ax;
        float nt = ny < app->drag_ay ? ny : app->drag_ay;
        float nr = nx < app->drag_ax ? app->drag_ax : nx;
        float nb = ny < app->drag_ay ? app->drag_ay : ny;
        if (nl < 0.0f) nl = 0.0f;
        if (nt < 0.0f) nt = 0.0f;
        if (nr > 1.0f) nr = 1.0f;
        if (nb > 1.0f) nb = 1.0f;
        app->crop_work[0] = nl;
        app->crop_work[1] = nt;
        app->crop_work[2] = nr;
        app->crop_work[3] = nb;
    } else if (app->crop_grip == 1) {
        float w = r - l, h = b - t;
        l += dx;
        r += dx;
        t += dy;
        b += dy;
        if (l < 0.0f) { r -= l; l = 0.0f; }
        if (t < 0.0f) { b -= t; t = 0.0f; }
        if (r > 1.0f) { l -= r - 1.0f; r = 1.0f; }
        if (b > 1.0f) { t -= b - 1.0f; b = 1.0f; }
        if (r - l < w) { r = l + w; }
        if (b - t < h) { b = t + h; }
        app->crop_work[0] = l;
        app->crop_work[1] = t;
        app->crop_work[2] = r;
        app->crop_work[3] = b;
    } else {
        if (app->crop_grip == 2 || app->crop_grip == 6 || app->crop_grip == 8) {
            l += dx;
        }
        if (app->crop_grip == 3 || app->crop_grip == 7 || app->crop_grip == 9) {
            r += dx;
        }
        if (app->crop_grip == 4 || app->crop_grip == 6 || app->crop_grip == 7) {
            t += dy;
        }
        if (app->crop_grip == 5 || app->crop_grip == 8 || app->crop_grip == 9) {
            b += dy;
        }
        if (l < 0.0f) l = 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (r > 1.0f) r = 1.0f;
        if (b > 1.0f) b = 1.0f;
        if (r - l < 0.02f) r = l + 0.02f;
        if (b - t < 0.02f) b = t + 0.02f;
        crop_lock_ratio(app, &l, &t, &r, &b);
        if (l < 0.0f) l = 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (r > 1.0f) r = 1.0f;
        if (b > 1.0f) b = 1.0f;
        app->crop_work[0] = l;
        app->crop_work[1] = t;
        app->crop_work[2] = r;
        app->crop_work[3] = b;
    }
    ui_invalidate(app);
}

static void crop_cancel(FusApp *app) {
    app->crop.left = app->crop_before[0];
    app->crop.top = app->crop_before[1];
    app->crop.right = app->crop_before[2];
    app->crop.bottom = app->crop_before[3];
    app->crop_preset = app->crop_before_preset;
    app->cropping = false;
    app->crop_grip = 0;
    app->panning = false;
    ui_invalidate(app);
}

static void crop_commit(FusApp *app) {
    if ((app->crop_work[2] - app->crop_work[0]) < 0.02f ||
        (app->crop_work[3] - app->crop_work[1]) < 0.02f) {
        crop_cancel(app);
        return;
    }
    app->crop.left = app->crop_work[0];
    app->crop.top = app->crop_work[1];
    app->crop.right = app->crop_work[2];
    app->crop.bottom = app->crop_work[3];
    app->cropping = false;
    app->crop_grip = 0;
    app->panning = false;
    history_commit(app);
    ui_invalidate(app);
}
/* --------------------------------------------------------------- commands */

/* Put the document back into a recorded state.  Filters live in the bitmap,
   so flipping them needs another decode pass. */
static void apply_state(FusApp *app, const FusDocState *s) {
    bool refilter = app->grayscale != s->grayscale || app->invert != s->invert;

    app->rotation = s->rotation;
    app->mirror_h = s->mirror_h;
    app->mirror_v = s->mirror_v;
    app->crop = s->crop;
    app->crop_preset = s->crop_preset;
    app->cropping = false;
    app->grayscale = s->grayscale;
    app->invert = s->invert;
    if (refilter) {
        start_filter();
    }
    ui_invalidate(app);
}

void app_action(int action) {
    FusApp *app = &g_app;

    switch (action) {
    case ACT_OPEN:
    case ACT_CANVAS: {
        wchar_t path[MAX_PATH];
        if (action == ACT_CANVAS && app->has_image) {
            break;
        }
        if (pick_open(path, MAX_PATH)) {
            start_load(path);
        }
        break;
    }
    case ACT_SAVE: {
        wchar_t path[MAX_PATH];
        if (!app->has_image || app->exporting) {
            break;
        }
        if (pick_save(path, MAX_PATH)) {
            start_export(path);
        }
        break;
    }
    case ACT_SAVE_NEAR: {
        if (!app->has_image || app->exporting) {
            break;
        }
        if (app->path[0] != L'\0') {
            wchar_t out[MAX_PATH];
            const wchar_t *dot = wcsrchr(app->path, L'.');
            const wchar_t *slash = wcsrchr(app->path, L'\\');
            size_t base = (size_t)((dot != NULL && (slash == NULL || dot > slash))
                                       ? (dot - app->path)
                                       : (int)wcslen(app->path));
            const wchar_t *ext = (dot != NULL && (slash == NULL || dot > slash))
                                     ? dot
                                     : L".png";
            _snwprintf(out, MAX_PATH, L"%.*s-edited%s", (int)base, app->path,
                       ext);
            start_export(out);
        }
        break;
    }
    case ACT_THEME:
        toggle_theme();
        break;
    case ACT_UNDO:
        if (app->undo_pos > 0) {
            app->undo_pos--;
            apply_state(app, &app->undo[app->undo_pos]);
        }
        break;
    case ACT_REDO:
        if (app->undo_pos + 1 < app->undo_len) {
            app->undo_pos++;
            apply_state(app, &app->undo[app->undo_pos]);
        }
        break;
    case ACT_ROTATE_CW:
    case ACT_ROTATE_180_CW:
        rotate_by(action == ACT_ROTATE_CW ? 1 : 2);
        break;
    case ACT_ROTATE_CCW:
    case ACT_ROTATE_180_CCW:
        rotate_by(action == ACT_ROTATE_CCW ? 3 : 2);
        break;
    case ACT_FLIP_H:
        flip_axis(true);
        break;
    case ACT_FLIP_V:
        flip_axis(false);
        break;
    case ACT_GRAYSCALE:
        toggle_filter(true);
        break;
    case ACT_INVERT:
        toggle_filter(false);
        break;
    case ACT_CROP_FREE:
        if (!app->has_image) {
            break;
        }
        app->cropping = true;
        app->crop_preset = 0;
        app->crop_work[0] = app->crop.left;
        app->crop_work[1] = app->crop.top;
        app->crop_work[2] = app->crop.right;
        app->crop_work[3] = app->crop.bottom;
        ui_invalidate(app);
        break;
    case ACT_CROP_16_9:
        apply_crop_preset(1);
        break;
    case ACT_CROP_1_1:
        apply_crop_preset(2);
        break;
    case ACT_CROP_9_16:
        apply_crop_preset(3);
        break;
    case ACT_CROP_4_3:
        apply_crop_preset(4);
        break;
    case ACT_CROP_3_4:
        apply_crop_preset(5);
        break;
    case ACT_CROP_3_2:
        apply_crop_preset(6);
        break;
    case ACT_CROP_2_3:
        apply_crop_preset(7);
        break;
    case ACT_CROP_APPLY:
        crop_commit(app);
        break;
    case ACT_CROP_CANCEL:
        crop_cancel(app);
        break;
    case ACT_ZOOM_FIT:
        app->fit = true;
        app->pan_x = 0.0f;
        app->pan_y = 0.0f;
        ui_invalidate(app);
        break;
    case ACT_ZOOM_100:
        if (app->has_image) {
            app->fit = false;
            app->zoom = 1.0f;
            app->pan_x = 0.0f;
            app->pan_y = 0.0f;
            ui_invalidate(app);
        }
        break;
    case ACT_TOOLS:
        if (app->pane_w == 0.0f) {
            app->sheet_open = !app->sheet_open;
            app->sheet_start_ms = now64();
            ui_invalidate(app);
            ensure_timer(app);
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------ result queue */

static void finish_load(FusApp *app, Result *res) {
    if (res->gen != app->load_gen) {
        luma_image_free(&res->image);
        return;
    }
    if (res->upgrade) {
        if (res->ok) {
            UINT old_w = (UINT)app->img_w;
            UINT old_h = (UINT)app->img_h;

            /* the on-screen bitmap carries the filters already, so only swap
               the pixels when the new decode is actually bigger */
            if (res->image.width > old_w || res->image.height > old_h) {
                luma_image_free(&app->base);
                app->base = res->image;
                ZeroMemory(&res->image, sizeof res->image);
                app->img_w = (float)app->base.width;
                app->img_h = (float)app->base.height;
                upload_bitmap(app, &app->base);
                app->fade = 1.0f;
                app->fade_active = false;
                ui_invalidate(app);
                tlog("upgrade_done");
            }
        }
        luma_image_free(&res->image);
        app->loading = false;
        ensure_timer(app);
        return;
    }
    app->loading = false;
    if (!res->ok) {
        ui_snackbar(app, res->error[0] != L'\0' ? res->error
                                                : L"打不开这张图片",
                    NULL, ACT_NONE);
        ui_invalidate(app);
        ensure_timer(app);
        return;
    }
    luma_image_free(&app->base);
    app->base = res->image;
    ZeroMemory(&res->image, sizeof res->image);
    app->have_base = true;
    app->has_image = true;
    app->img_w = (float)app->base.width;
    app->img_h = (float)app->base.height;
    app->rotation = 0;
    app->mirror_h = false;
    app->mirror_v = false;
    app->grayscale = false;
    app->invert = false;
    app->crop.left = 0.0f;
    app->crop.top = 0.0f;
    app->crop.right = 1.0f;
    app->crop.bottom = 1.0f;
    app->crop_preset = 0;
    app->cropping = false;
    app->fit = true;
    app->zoom = 1.0f;
    app->pan_x = 0.0f;
    app->pan_y = 0.0f;
    {
        const wchar_t *slash = wcsrchr(res->path, L'\\');
        wcsncpy(app->path, res->path, MAX_PATH - 1);
        wcsncpy(app->name, slash != NULL ? slash + 1 : res->path, MAX_PATH - 1);
    }
    tlog("load_done");
    /* Test hook: FUST_AUTOSAVE=<path> exercises the dialog-free part of the
       save path once per process (worker thread + result handling). */
    {
        static int auto_saved;
        const wchar_t *auto_path = _wgetenv(L"FUST_AUTOSAVE");
        if (!auto_saved && auto_path != NULL && auto_path[0] != L'\0') {
            auto_saved = 1;
            start_export(auto_path);
        }
    }
    app->fade = 0.0f;
    app->fade_active = true;
    app->fade_start_ms = now64();
    update_title(app);
    app->undo_len = 1;
    app->undo_pos = 0;
    app->undo[0] = state_now(app);
    upload_bitmap(app, &app->base);
    ui_invalidate(app);
    ensure_timer(app);
    if ((int)app->img_w >= (int)PREVIEW_MAX ||
        (int)app->img_h >= (int)PREVIEW_MAX) {
        start_upgrade(app->path, app->load_gen);
    }
}

static void finish_filter(FusApp *app, Result *res) {
    if (res->gen != app->load_gen) {
        luma_image_free(&res->image);
        return;
    }
    upload_bitmap(app, &res->image);
    luma_image_free(&res->image);
    ui_invalidate(app);
}

static void finish_export(FusApp *app, Result *res) {
    app->exporting = false;
    if (res->ok) {
        ui_snackbar(app, L"已保存", NULL, ACT_NONE);
    } else {
        ui_snackbar(app, res->error[0] != L'\0' ? res->error : L"保存失败", NULL,
                    ACT_NONE);
    }
    ui_invalidate(app);
    ensure_timer(app);
}

/* --------------------------------------------------------------- window */

static void apply_dark_title(HWND hwnd, bool dark) {
    BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &value, sizeof value);
    DwmSetWindowAttribute(hwnd, 19, &value, sizeof value);
    {
        int corner = 2; /* DWMWCP_ROUND */
        DwmSetWindowAttribute(hwnd, 33, &corner, sizeof corner);
    }
}

static void fit_initial_rect(RECT *out) {
    RECT work;
    int width_dp = 1320;
    int height_dp = 820;
    float scale = (float)GetDpiForSystem() / 96.0f;
    int w;
    int h;

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    w = (int)(width_dp * scale);
    h = (int)(height_dp * scale);
    if (w > work.right - work.left - 48) {
        w = work.right - work.left - 48;
    }
    if (h > work.bottom - work.top - 48) {
        h = work.bottom - work.top - 48;
    }
    out->left = work.left + ((work.right - work.left) - w) / 2;
    out->top = work.top + ((work.bottom - work.top) - h) / 2;
    out->right = out->left + w;
    out->bottom = out->top + h;
}

static bool load_theme_pref(void) {
    HKEY key;
    DWORD value = 0;
    DWORD size = sizeof value;
    bool dark = false;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Fustimage", 0, KEY_READ,
                      &key) == ERROR_SUCCESS) {
        if (RegQueryValueExW(key, L"theme", NULL, NULL, (BYTE *)&value,
                             &size) == ERROR_SUCCESS) {
            dark = value != 0;
        }
        RegCloseKey(key);
    }
    return dark;
}

static void save_theme_pref(bool dark) {
    HKEY key;
    DWORD value = dark ? 1 : 0;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Fustimage", 0, NULL, 0,
                        KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"theme", 0, REG_DWORD, (const BYTE *)&value,
                       sizeof value);
        RegCloseKey(key);
    }
}

static void set_cursor_for(float x, float y) {
    FusApp *app = &g_app;
    if (app->cropping && canvas_contains(app, x, y)) {
        SetCursor(g_cursor_cross);
    } else if (app->has_image && canvas_contains(app, x, y)) {
        SetCursor(app->panning ? g_cursor_cross : g_cursor_hand);
    } else {
        int action = ACT_NONE;
        if (ui_hit_test(app, x, y, &action, NULL) && action != ACT_NONE) {
            SetCursor(g_cursor_hand);
        } else {
            SetCursor(g_cursor_arrow);
        }
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam) {
    FusApp *app = &g_app;

    switch (message) {
    case WM_CREATE:
        app->hwnd = hwnd;
        return 0;

    case WM_ERASEBKGND: {
        HDC dc = (HDC)wparam;
        RECT rc;
        if (app->rt != NULL) {
            /* the D2D target paints the whole client area every frame; a GDI
               erase on top of it is wasted work and shows up as a flicker */
            return 1;
        }
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_erase_brush != NULL ? g_erase_brush
                                                : (HBRUSH)(COLOR_WINDOW + 1));
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        /* BeginPaint first: it clears the pending erase flag, so the frame we
           draw below is not wiped by the background fill afterwards. */
        BeginPaint(hwnd, &ps);
        if (app->rt == NULL) {
            /* the target is dropped on a theme switch, DPI change or device
               loss: build a fresh one before drawing anything */
            create_target(app);
        }
        if (app->rt != NULL) {
            paint_client(app);
        }
        if (app->target_lost) {
            /* the surface went away (dialog, display change, device reset):
               rebuild it now and repaint on the next tick */
            app->target_lost = false;
            discard_target(app);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        EndPaint(hwnd, &ps); /* validates the update region */
        return 0;
    }

    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED && app->rt != NULL) {
            D2D1_SIZE_U size;
            size.width = (UINT)LOWORD(lparam);
            size.height = (UINT)HIWORD(lparam);
            if (size.width > 0 && size.height > 0) {
                app->rt->lpVtbl->Resize(app->rt, &size);
            }
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_TIMER:
        if (wparam == TIMER_ANIM) {
            tick(app);
            if (!ui_animation_active(app)) {
                KillTimer(hwnd, TIMER_ANIM);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_APP_RESULT: {
        Result *res = (Result *)lparam;
        if (res != NULL) {
            tick(app);
            if (res->kind == RES_LOADED) {
                finish_load(app, res);
            } else if (res->kind == RES_FILTERED) {
                finish_filter(app, res);
            } else {
                finish_export(app, res);
            }
            luma_image_free(&res->image);
            free(res);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        tick(app);
        if (!g_tracking_leave) {
            TRACKMOUSEEVENT track;
            track.cbSize = sizeof track;
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            track.dwHoverTime = 0;
            g_tracking_leave = TrackMouseEvent(&track) != 0;
        }
        {
            float x = (float)GET_X_LPARAM(lparam);
            float y = (float)GET_Y_LPARAM(lparam);
            if (app->panning && app->has_image) {
                if (app->cropping) {
                    crop_update(app, x, y);
                } else {
                    app->pan_x = app->pan_start_x + (x - app->pan_origin.x);
                    app->pan_y = app->pan_start_y + (y - app->pan_origin.y);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else {
                ui_pointer_move(app, x, y);
            }
            set_cursor_for(x, y);
        }
        return 0;

    case WM_MOUSELEAVE:
        g_tracking_leave = false;
        ui_pointer_leave(app);
        return 0;

    case WM_SETCURSOR:
        return TRUE; /* WM_MOUSEMOVE keeps the cursor current */

    case WM_LBUTTONDOWN:
        tick(app);
        SetCapture(hwnd);
        {
            float x = (float)GET_X_LPARAM(lparam);
            float y = (float)GET_Y_LPARAM(lparam);
            ui_pointer_down(app, x, y);
            if (app->active != ACT_NONE) {
                app->ripple_action = app->active;
                app->ripple_x = x;
                app->ripple_y = y;
                app->ripple_ms = GetTickCount64();
                ensure_timer(app);
                InvalidateRect(hwnd, NULL, FALSE);
                /* a HUD control (crop confirm bar, chips, ...) took the
                   press -- do not start a canvas gesture under it */
                return 0;
            }
            if (app->cropping && canvas_contains(app, x, y)) {
                crop_begin(app, x, y);
            } else if (app->has_image && !app->cropping &&
                       canvas_contains(app, x, y)) {
                app->panning = true;
                app->pan_origin.x = (LONG)x;
                app->pan_origin.y = (LONG)y;
                app->pan_start_x = app->pan_x;
                app->pan_start_y = app->pan_y;
            }
        }
        return 0;

    case WM_LBUTTONUP:
        tick(app);
        ReleaseCapture();
        {
            float x = (float)GET_X_LPARAM(lparam);
            float y = (float)GET_Y_LPARAM(lparam);
            if (app->panning) {
                app->panning = false;
                if (app->cropping) {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else {
                ui_pointer_up(app, x, y);
            }
        }
        return 0;

    case WM_MOUSEWHEEL: {
        POINT pt;
        pt.x = GET_X_LPARAM(lparam);
        pt.y = GET_Y_LPARAM(lparam);
        ScreenToClient(hwnd, &pt);
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            float factor = delta > 0 ? 1.15f : (delta < 0 ? 1.0f / 1.15f : 1.0f);
            zoom_at((float)pt.x, (float)pt.y, factor);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            if (wparam == 'O') {
                app_action(ACT_OPEN);
                return 0;
            }
            if (wparam == 'S') {
                app_action(ACT_SAVE);
                return 0;
            }
            if (wparam == 'Z') {
                app_action((GetKeyState(VK_SHIFT) & 0x8000) != 0 ? ACT_REDO
                                                                : ACT_UNDO);
                return 0;
            }
            if (wparam == 'Y') {
                app_action(ACT_REDO);
                return 0;
            }
            if (wparam == '0') {
                app_action(ACT_ZOOM_100);
                return 0;
            }
            if (wparam == 'F') {
                app_action(ACT_ZOOM_FIT);
                return 0;
            }
        }
        if (wparam == 'R' && app->has_image) {
            app_action((GetKeyState(VK_SHIFT) & 0x8000) != 0
                           ? ACT_ROTATE_CCW
                           : ACT_ROTATE_CW);
            return 0;
        }
        if (wparam == 'H' && app->has_image) {
            app_action((GetKeyState(VK_SHIFT) & 0x8000) != 0 ? ACT_FLIP_V
                                                             : ACT_FLIP_H);
            return 0;
        }
        if (wparam == 'T') {
            app_action(ACT_THEME);
            return 0;
        }
        if ((wparam == VK_OEM_PLUS || wparam == VK_ADD) && app->has_image) {
            zoom_at((app->canvas.left + app->canvas.right) * 0.5f,
                    (app->canvas.top + app->canvas.bottom) * 0.5f, 1.15f);
            return 0;
        }
        if ((wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) &&
            app->has_image) {
            zoom_at((app->canvas.left + app->canvas.right) * 0.5f,
                    (app->canvas.top + app->canvas.bottom) * 0.5f,
                    1.0f / 1.15f);
            return 0;
        }
        if (wparam == VK_ESCAPE && app->cropping) {
            crop_cancel(app);
            return 0;
        }
        if (wparam == VK_RETURN && app->cropping) {
            crop_commit(app);
            return 0;
        }
        if (wparam == VK_DELETE) {
            return 0;
        }
        break;

    case WM_DROPFILES: {
        HDROP drop = (HDROP)wparam;
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(drop, 0, path, MAX_PATH) > 0) {
            start_load(path);
        }
        DragFinish(drop);
        return 0;
    }

    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lparam;
        app->dpi = (float)HIWORD(wparam);
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        discard_target(app);
        create_target(app);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)lparam;
        float scale = app->dpi > 0.0f ? app->dpi / 96.0f : 1.0f;
        info->ptMinTrackSize.x = (LONG)(640.0f * scale);
        info->ptMinTrackSize.y = (LONG)(480.0f * scale);
        return 0;
    }

    case WM_DESTROY:
        save_theme_pref(app->dark);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

/* ---------------------------------------------------------------- startup */

/* Headless render: Fustimage.exe --shot <out.png> [--dark] [--size=WxH]
   [--rot=N] [--gray] [--invert] [--crop=N] [--zoom100] <image>
   Draws the real UI into an offscreen WIC bitmap so screenshots never need a
   visible window. */
static int cli_shot(int argc, wchar_t **argv) {
    FusApp *app = &g_app;
    const wchar_t *out_path = NULL;
    const wchar_t *image_path = NULL;
    bool dark = false, zoom100 = false, cropping = false;
    int width = 1320, height = 820;
    int crop_index = -1;
    int hover_action = ACT_NONE;
    int ripple_action = ACT_NONE;
    float ripple_x = 0.0f, ripple_y = 0.0f;
    int i;
    HWND hwnd = NULL;
    IWICImagingFactory *wic = NULL;
    IWICBitmap *wic_bitmap = NULL;
    IWICBitmapLock *lock = NULL;
    ID2D1RenderTarget *rt = NULL;
    D2D1_COLOR_F black = {0.0f, 0.0f, 0.0f, 1.0f};
    wchar_t error[512];
    int rc = 1;

    if (argc > 2) {
        out_path = argv[2];
    }
    for (i = 3; i < argc; i++) {
        const wchar_t *a = argv[i];
        if (wcscmp(a, L"--dark") == 0) {
            dark = true;
        } else if (wcscmp(a, L"--zoom100") == 0) {
            zoom100 = true;
        } else if (wcscmp(a, L"--cropping") == 0) {
            cropping = true;
        } else if (wcsncmp(a, L"--hover=", 8) == 0) {
            hover_action = _wtoi(a + 8);
        } else if (wcsncmp(a, L"--ripple=", 9) == 0) {
            int ra = 0;
            if (swscanf(a + 9, L"%d,%f,%f", &ra, &ripple_x, &ripple_y) == 3) {
                ripple_action = ra;
            }
        } else if (wcscmp(a, L"--gray") == 0) {
            app->grayscale = true;
        } else if (wcscmp(a, L"--invert") == 0) {
            app->invert = true;
        } else if (wcsncmp(a, L"--rot=", 6) == 0) {
            app->rotation = _wtoi(a + 6) & 3;
        } else if (wcsncmp(a, L"--size=", 7) == 0) {
            int ww = 0, hh = 0;
            if (swscanf(a + 7, L"%dx%d", &ww, &hh) == 2 && ww > 200 && hh > 200) {
                width = ww;
                height = hh;
            }
        } else if (wcsncmp(a, L"--crop=", 7) == 0) {
            crop_index = _wtoi(a + 7);
        } else if (a[0] != L'-') {
            image_path = a;
        }
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpszClassName = L"FustimageShot";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(0, L"FustimageShot", L"shot", WS_POPUP, 0, 0,
                               width, height, NULL, NULL, wc.hInstance, NULL);
    }
    if (hwnd == NULL) {
        goto done;
    }
    app->hwnd = hwnd;
    app->dpi = 96.0f;
    app->dark = dark;
    app->fade = 1.0f;
    app->fit = !zoom100;
    app->zoom = 1.0f;
    app->crop.left = 0.0f;
    app->crop.top = 0.0f;
    app->crop.right = 1.0f;
    app->crop.bottom = 1.0f;
    app->crop_preset = 0;
    app->now_ms = now64();
    app->start_ms = app->now_ms;

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 &IID_ID2D1Factory, NULL,
                                 (void **)&app->d2d))) {
        goto done;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   &k_iid_dwrite_factory,
                                   (IUnknown **)&app->dw))) {
        goto done;
    }
    theme_apply(app, dark);
    if (!theme_create_type(app)) {
        goto done;
    }
    icons_get(app->d2d);

    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                                CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                                (void **)&wic))) {
        goto done;
    }
    if (FAILED(wic->lpVtbl->CreateBitmap(wic, (UINT)width, (UINT)height,
                                         &GUID_WICPixelFormat32bppPBGRA,
                                         WICBitmapCacheOnLoad,
                                         &wic_bitmap))) {
        goto done;
    }
    {
        D2D1_RENDER_TARGET_PROPERTIES props;
        ZeroMemory(&props, sizeof props);
        props.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
        props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        props.dpiX = 96.0f;
        props.dpiY = 96.0f;
        props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
        props.usage = D2D1_RENDER_TARGET_USAGE_NONE;
        if (FAILED(app->d2d->lpVtbl->CreateWicBitmapRenderTarget(
                app->d2d, wic_bitmap, &props, &rt))) {
            goto done;
        }
    }
    app->rt = (ID2D1HwndRenderTarget *)rt;
    RT(app)->lpVtbl->SetAntialiasMode(RT(app), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    RT(app)->lpVtbl->SetTextAntialiasMode(RT(app),
                                          D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    RT(app)->lpVtbl->CreateSolidColorBrush(RT(app), &black, NULL, &app->brush);

    if (image_path != NULL) {
        ZeroMemory(&app->base, sizeof app->base);
        if (!luma_image_load(image_path, &app->base, error,
                             sizeof error / sizeof error[0])) {
            fwprintf(stderr, L"%ls\n", error);
            goto done;
        }
        app->have_base = true;
        app->has_image = true;
        app->img_w = (float)app->base.width;
        app->img_h = (float)app->base.height;
        wcsncpy(app->path, image_path, MAX_PATH - 1);
        {
            const wchar_t *slash = wcsrchr(image_path, L'\\');
            wcsncpy(app->name, slash != NULL ? slash + 1 : image_path,
                    MAX_PATH - 1);
        }
        if (app->grayscale || app->invert) {
            image_apply_filters(&app->base, app->grayscale, app->invert);
        }
        upload_bitmap(app, &app->base);
        if (crop_index >= 0 && crop_index < g_preset_count) {
            app->crop_preset = crop_index;
            if (g_presets[crop_index].ratio > 0.0f) {
                float ow, oh;
                oriented_dims(app, &ow, &oh);
                {
                    float ratio = g_presets[crop_index].ratio;
                    float cw = 1.0f, ch = 1.0f;
                    float ir = ow / oh;
                    if (ir > ratio) {
                        cw = ratio / ir;
                    } else {
                        ch = ir / ratio;
                    }
                    app->crop.left = (1.0f - cw) * 0.5f;
                    app->crop.top = (1.0f - ch) * 0.5f;
                    app->crop.right = app->crop.left + cw;
                    app->crop.bottom = app->crop.top + ch;
                }
            }
        }
    }

    if (cropping) {
        app->cropping = true;
        app->crop_work[0] = app->crop.left;
        app->crop_work[1] = app->crop.top;
        app->crop_work[2] = app->crop.right;
        app->crop_work[3] = app->crop.bottom;
    }
    if (hover_action != ACT_NONE) {
        app->hot = hover_action;
        app->pointer_x = (float)width * 0.55f;
        app->pointer_y = (float)height * 0.62f;
    }
    if (ripple_action != ACT_NONE && app->now_ms == 0) {
        app->now_ms = 1000;
    }
    if (ripple_action != ACT_NONE) {
        app->ripple_action = ripple_action;
        app->ripple_x = ripple_x;
        app->ripple_y = ripple_y;
        app->ripple_ms = app->now_ms > 150 ? app->now_ms - 150 : 0;
    }
    ui_paint(app);

    if (SUCCEEDED(wic_bitmap->lpVtbl->Lock(wic_bitmap, NULL,
                                           WICBitmapLockRead, &lock))) {
        UINT ww = 0, hh = 0, stride = 0, buffer = 0;
        BYTE *data = NULL;
        wic_bitmap->lpVtbl->GetSize(wic_bitmap, &ww, &hh);
        lock->lpVtbl->GetStride(lock, &stride);
        lock->lpVtbl->GetDataPointer(lock, &buffer, &data);
        if (data != NULL) {
            LumaImage shot;

            LumaCrop full = {0.0f, 0.0f, 1.0f, 1.0f};
            shot.width = ww;
            shot.height = hh;
            shot.stride = stride;
            shot.pixels = (uint8_t *)malloc((size_t)stride * hh);
            if (shot.pixels != NULL) {
                memcpy(shot.pixels, data, (size_t)stride * hh);
                if (luma_image_export(&shot, 0, false, false, full, out_path,
                                      error, sizeof error / sizeof error[0])) {
                    rc = 0;
                } else {
                    fwprintf(stderr, L"%ls\n", error);
                }
                free(shot.pixels);
            }
        }
    }

done:
    if (lock != NULL) fus_release(lock);
    if (wic_bitmap != NULL) fus_release(wic_bitmap);
    if (wic != NULL) fus_release(wic);
    if (app->bitmap != NULL) fus_release(app->bitmap);
    if (app->checker != NULL) fus_release(app->checker);
    if (app->brush != NULL) fus_release(app->brush);
    if (rt != NULL) fus_release(rt);
    theme_release(app);
    luma_image_free(&app->base);
    if (app->dw != NULL) fus_release(app->dw);
    if (app->d2d != NULL) fus_release(app->d2d);
    if (hwnd != NULL) {
        DestroyWindow(hwnd);
    }
    CoUninitialize();
    return rc;
}

/* Batch mode: Fustimage.exe --export <out.png> <in.jpg> [--rot=N]
   [--mirror-h] [--mirror-v] [--gray] [--invert] [--crop=l,t,r,b]
   No window, no COM apartment on the caller: handy for scripts and tests. */
static int cli_export(int argc, wchar_t **argv) {
    LumaImage image;
    wchar_t error[512];
    int rotation = 0;
    bool mirror_h = false, mirror_v = false, gray = false, invert = false;
    LumaCrop crop = {0.0f, 0.0f, 1.0f, 1.0f};
    int i;
    bool ok;

    if (argc < 4) {
        fwprintf(stderr, L"usage: Fustimage.exe --export <out> <in> [options]\n");
        return 2;
    }
    for (i = 4; i < argc; i++) {
        if (wcscmp(argv[i], L"--gray") == 0) {
            gray = true;
        } else if (wcscmp(argv[i], L"--invert") == 0) {
            invert = true;
        } else if (wcscmp(argv[i], L"--mirror-h") == 0) {
            mirror_h = true;
        } else if (wcscmp(argv[i], L"--mirror-v") == 0) {
            mirror_v = true;
        } else if (wcsncmp(argv[i], L"--rot=", 6) == 0) {
            rotation = _wtoi(argv[i] + 6) & 3;
        } else if (wcsncmp(argv[i], L"--crop=", 7) == 0) {
            double l = 0.0, t = 0.0, r = 1.0, b = 1.0;
            if (swscanf(argv[i] + 7, L"%lf,%lf,%lf,%lf", &l, &t, &r, &b) == 4) {
                crop.left = (float)l;
                crop.top = (float)t;
                crop.right = (float)r;
                crop.bottom = (float)b;
            }
        }
    }
    ZeroMemory(&image, sizeof image);
    if (!luma_image_load(argv[3], &image, error,
                         sizeof error / sizeof error[0])) {
        fwprintf(stderr, L"%ls\n", error);
        return 1;
    }
    image_apply_filters(&image, gray, invert);
    ok = luma_image_export(&image, rotation, mirror_h, mirror_v, crop, argv[2],
                           error, sizeof error / sizeof error[0]);
    luma_image_free(&image);
    if (!ok) {
        fwprintf(stderr, L"%ls\n", error);
        return 1;
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command,
                      int show) {
    WNDCLASSEXW wc;
    RECT rect;
    MSG msg;
    FusApp *app = &g_app;
    const wchar_t *class_name = L"FustimageMain";
    const wchar_t *open_path = NULL;

    (void)previous;
    (void)command;

    tlog_open();
    tlog("entry");
    {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv != NULL) {
            if (argc > 1 && wcscmp(argv[1], L"--export") == 0) {
                int rc = cli_export(argc, argv);
                LocalFree(argv);
                return rc;
            }
            if (argc > 1 && wcscmp(argv[1], L"--shot") == 0) {
                int rc = cli_shot(argc, argv);
                LocalFree(argv);
                return rc;
            }
            LocalFree(argv);
        }
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    app->start_ms = now64();
    app->now_ms = app->start_ms;
    app->fade = 1.0f;
    app->fit = true;
    app->dpi = (float)GetDpiForSystem();
    app->dark = load_theme_pref();
    g_debug_title = GetEnvironmentVariableW(L"FUST_DEBUG_TITLE", NULL, 0) > 0;
    {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv != NULL) {
            if (argc > 1) {
                open_path = argv[1];
            }
            LocalFree(argv);
        }
    }

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 &IID_ID2D1Factory, NULL,
                                 (void **)&app->d2d))) {
        return 1;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                   &k_iid_dwrite_factory, (IUnknown **)&app->dw))) {
        return 1;
    }
    theme_apply(app, app->dark);
    if (!theme_create_type(app)) {
        return 1;
    }
    set_erase_brush(app);
    tlog("factories");

    g_cursor_arrow = LoadCursorW(NULL, IDC_ARROW);
    g_cursor_hand = LoadCursorW(NULL, IDC_HAND);
    g_cursor_cross = LoadCursorW(NULL, IDC_CROSS);

    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (wc.hIcon == NULL) {
        wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    }
    wc.hCursor = g_cursor_arrow;
    wc.hbrBackground = NULL; /* WM_ERASEBKGND paints the surface colour */
    wc.lpszClassName = class_name;
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    fit_initial_rect(&rect);
    CreateWindowExW(0, class_name, L"Fustimage", WS_OVERLAPPEDWINDOW,
                    rect.left, rect.top, rect.right - rect.left,
                    rect.bottom - rect.top, NULL, NULL, instance, NULL);
    if (app->hwnd == NULL) {
        return 1;
    }
    app->dpi = (float)GetDpiForWindow(app->hwnd);
    apply_dark_title(app->hwnd, app->dark);
    DragAcceptFiles(app->hwnd, TRUE);
    tlog("window");

    /* The window appears immediately, in the right colour; the device target
       and the first real frame follow. */
    ShowWindow(app->hwnd, show == SW_SHOWMINIMIZED ? SW_SHOWNORMAL : show);
    UpdateWindow(app->hwnd);
    tlog("shown");
    if (create_target(app) == S_OK) {
        tlog("target");
        paint_client(app);
        tlog("paint");
    }
    if (open_path != NULL && GetFileAttributesW(open_path) != INVALID_FILE_ATTRIBUTES) {
        start_load(open_path);
    }
    ensure_timer(app);

    tlog("loop");
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    discard_target(app);
    theme_release(app);
    luma_image_free(&app->base);
    if (app->d2d != NULL) fus_release(app->d2d);
    if (app->dw != NULL) fus_release(app->dw);
    return (int)msg.wParam;
}
