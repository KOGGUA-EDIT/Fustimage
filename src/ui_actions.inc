/* Luma actions, crop math, async loading and file dialogs.
   Included at the end of main.c so it can share the file's static state. */

typedef struct {
    LONG id;
    LumaImage image;
    wchar_t *path;
    wchar_t error[512];
    bool ok;
} LumaLoadResult;

static const wchar_t *luma_file_name(const wchar_t *path) {
    const wchar_t *a = wcsrchr(path, L'\\');
    const wchar_t *b = wcsrchr(path, L'/');
    const wchar_t *p = a;
    if (b != NULL && (p == NULL || b > p)) p = b;
    return p ? p + 1 : path;
}

static bool luma_path_exists(const wchar_t *path) {
    DWORD attr;
    if (path == NULL || path[0] == L'\0') return false;
    attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void luma_format_name(const wchar_t *path, wchar_t *out, size_t cap) {
    const wchar_t *name = luma_file_name(path);
    const wchar_t *dot = wcsrchr(name, L'.');
    int i = 0;
    if (cap == 0) return;
    if (dot == NULL || dot[1] == L'\0') {
        copy_utf16(out, cap, L"IMAGE");
        return;
    }
    ++dot;
    while (dot[i] != L'\0' && i < (int)cap - 1) {
        wchar_t ch = dot[i];
        if (ch >= L'a' && ch <= L'z') ch = (wchar_t)(ch - 32);
        out[i] = ch;
        ++i;
    }
    out[i] = L'\0';
}

static void luma_file_size_text(ULONGLONG bytes, wchar_t *out, size_t cap) {
    if (bytes < 1024ULL) {
        swprintf(out, cap, L"%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024ULL * 1024ULL) {
        swprintf(out, cap, L"%.1f KB", (double)bytes / 1024.0);
    } else {
        swprintf(out, cap, L"%.1f MB", (double)bytes / (1024.0 * 1024.0));
    }
}

static void luma_update_meta(const wchar_t *path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    ULONGLONG bytes = 0;
    const wchar_t *name = luma_file_name(path);
    copy_utf16(g_app.display_name, 160, name);
    luma_format_name(path, g_app.format_name, 16);
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) {
        bytes = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    }
    if (bytes > 0) luma_file_size_text(bytes, g_app.size_text, 32);
    else g_app.size_text[0] = L'\0';
}

static bool luma_crop_equal(LumaCrop a, LumaCrop b) {
    return fabsf(a.left - b.left) < 0.0001f &&
           fabsf(a.top - b.top) < 0.0001f &&
           fabsf(a.right - b.right) < 0.0001f &&
           fabsf(a.bottom - b.bottom) < 0.0001f;
}

static void luma_update_edited(void) {
    bool changed = false;
    if (g_app.has_image) {
        changed = g_app.rotation != g_app.initial_rotation ||
                  g_app.mirror_h != g_app.initial_mirror_h ||
                  g_app.mirror_v != g_app.initial_mirror_v ||
                  !luma_crop_equal(g_app.crop, g_app.initial_crop);
    }
    g_app.edited = changed;
}

static void luma_mark_changed(void) {
    g_app.exported = false;
    luma_update_edited();
}

static void luma_history_push_value(LumaEdit edit) {
    int slot;
    if (g_app.undo_count >= MAX_UNDO) {
        memmove(&g_app.undo[0], &g_app.undo[1],
                sizeof(LumaEdit) * (MAX_UNDO - 1));
        slot = MAX_UNDO - 1;
    } else {
        slot = g_app.undo_count++;
    }
    g_app.undo[slot] = edit;
    g_app.redo_count = 0;
}

static void luma_history_push(void) {
    LumaEdit edit;
    edit.rotation = g_app.rotation;
    edit.crop = g_app.crop;
    edit.mirror_h = g_app.mirror_h;
    edit.mirror_v = g_app.mirror_v;
    luma_history_push_value(edit);
}

static void luma_restore_edit(LumaEdit edit) {
    g_app.rotation = ((edit.rotation % 4) + 4) % 4;
    g_app.crop = edit.crop;
    g_app.mirror_h = edit.mirror_h;
    g_app.mirror_v = edit.mirror_v;
    g_app.crop.left = clampf(g_app.crop.left, 0.0f, 1.0f);
    g_app.crop.top = clampf(g_app.crop.top, 0.0f, 1.0f);
    g_app.crop.right = clampf(g_app.crop.right, 0.0f, 1.0f);
    g_app.crop.bottom = clampf(g_app.crop.bottom, 0.0f, 1.0f);
    if (g_app.crop.right - g_app.crop.left < 0.001f) g_app.crop.right = 1.0f;
    if (g_app.crop.bottom - g_app.crop.top < 0.001f) g_app.crop.bottom = 1.0f;
    g_app.crop_mode = false;
    g_app.crop_preset = -1;
    g_app.crop_aspect = 0.0f;
    g_app.crop_drag = 0;
    g_app.crop_before_valid = false;
    luma_mark_changed();
    update_image_view();
    layout_app();
}

static void luma_undo(void) {
    LumaEdit current;
    if (g_app.undo_count <= 0 || !g_app.has_image || g_app.crop_mode) return;
    current.rotation = g_app.rotation;
    current.crop = g_app.crop;
    current.mirror_h = g_app.mirror_h;
    current.mirror_v = g_app.mirror_v;
    if (g_app.redo_count < MAX_UNDO) g_app.redo[g_app.redo_count++] = current;
    luma_restore_edit(g_app.undo[--g_app.undo_count]);
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_redo(void) {
    LumaEdit current;
    if (g_app.redo_count <= 0 || !g_app.has_image || g_app.crop_mode) return;
    current.rotation = g_app.rotation;
    current.crop = g_app.crop;
    current.mirror_h = g_app.mirror_h;
    current.mirror_v = g_app.mirror_v;
    if (g_app.undo_count < MAX_UNDO) g_app.undo[g_app.undo_count++] = current;
    luma_restore_edit(g_app.redo[--g_app.redo_count]);
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static LumaCrop luma_rotate_crop_cw(LumaCrop c, int turns) {
    LumaCrop n;
    int i;
    turns = ((turns % 4) + 4) % 4;
    for (i = 0; i < turns; ++i) {
        n.left = 1.0f - c.bottom;
        n.top = c.left;
        n.right = 1.0f - c.top;
        n.bottom = c.right;
        c = n;
    }
    c.left = clampf(c.left, 0.0f, 0.999f);
    c.top = clampf(c.top, 0.0f, 0.999f);
    c.right = clampf(c.right, c.left + 0.001f, 1.0f);
    c.bottom = clampf(c.bottom, c.top + 0.001f, 1.0f);
    return c;
}

static void luma_rotate(int quarters) {
    if (!g_app.has_image || g_app.loading || g_app.crop_mode) return;
    quarters = ((quarters % 4) + 4) % 4;
    if (quarters == 0) return;
    luma_history_push();
    g_app.rotation = (g_app.rotation + quarters) % 4;
    g_app.crop = luma_rotate_crop_cw(g_app.crop, quarters);
    if (quarters & 1) {
        bool flip = g_app.mirror_h;
        g_app.mirror_h = g_app.mirror_v;
        g_app.mirror_v = flip;
    }
    g_app.crop_preset = -1;
    g_app.crop_aspect = 0.0f;
    luma_mark_changed();
    update_image_view();
    layout_app();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_mirror(bool horizontal) {
    LumaCrop c;
    if (!g_app.has_image || g_app.loading || g_app.crop_mode) return;
    luma_history_push();
    c = g_app.crop;
    if (horizontal) {
        g_app.mirror_h = !g_app.mirror_h;
        g_app.crop.left = 1.0f - c.right;
        g_app.crop.right = 1.0f - c.left;
    } else {
        g_app.mirror_v = !g_app.mirror_v;
        g_app.crop.top = 1.0f - c.bottom;
        g_app.crop.bottom = 1.0f - c.top;
    }
    g_app.crop_preset = -1;
    g_app.crop_aspect = 0.0f;
    luma_mark_changed();
    update_image_view();
    layout_app();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static Rect luma_crop_screen_rect(void) {
    Rect ir = g_app.image_rect;
    return rect_make(ir.left + rect_w(ir) * g_app.crop.left,
                     ir.top + rect_h(ir) * g_app.crop.top,
                     ir.left + rect_w(ir) * g_app.crop.right,
                     ir.top + rect_h(ir) * g_app.crop.bottom);
}

static void luma_crop_set_pixels(float l, float t, float r, float b,
                                 int rw, int rh) {
    const float minpx = 12.0f;
    if (rw < 2 || rh < 2) return;
    l = clampf(l, 0.0f, (float)rw);
    t = clampf(t, 0.0f, (float)rh);
    r = clampf(r, 0.0f, (float)rw);
    b = clampf(b, 0.0f, (float)rh);
    if (r - l < minpx) {
        r = l + minpx;
        if (r > (float)rw) {
            r = (float)rw;
            l = r - minpx;
        }
    }
    if (b - t < minpx) {
        b = t + minpx;
        if (b > (float)rh) {
            b = (float)rh;
            t = b - minpx;
        }
    }
    if (l < 0.0f) l = 0.0f;
    if (t < 0.0f) t = 0.0f;
    g_app.crop.left = l / (float)rw;
    g_app.crop.top = t / (float)rh;
    g_app.crop.right = r / (float)rw;
    g_app.crop.bottom = b / (float)rh;
}

static void luma_crop_fit_aspect(float aspect, bool reset) {
    int rw, rh;
    float w, h, cx, cy;
    if (!g_app.has_image) return;
    rotated_pixel_size(&rw, &rh);
    if (rw < 2 || rh < 2) return;
    if (reset) {
        w = (float)rw;
        h = (float)rh;
        if (aspect > 0.0f) {
            if (w / h > aspect) w = h * aspect;
            else h = w / aspect;
        }
        cx = (float)rw * 0.5f;
        cy = (float)rh * 0.5f;
    } else {
        w = (g_app.crop.right - g_app.crop.left) * (float)rw;
        h = (g_app.crop.bottom - g_app.crop.top) * (float)rh;
        cx = (g_app.crop.left + g_app.crop.right) * 0.5f * (float)rw;
        cy = (g_app.crop.top + g_app.crop.bottom) * 0.5f * (float)rh;
        if (aspect > 0.0f) {
            if (w / h > aspect) w = h * aspect;
            else h = w / aspect;
        }
        if (w > (float)rw * 0.92f) {
            w = (float)rw * 0.92f;
            if (aspect > 0.0f) h = w / aspect;
        }
        if (h > (float)rh * 0.92f) {
            h = (float)rh * 0.92f;
            if (aspect > 0.0f) w = h * aspect;
        }
    }
    if (w < 12.0f) w = 12.0f;
    if (h < 12.0f) h = 12.0f;
    if (w > (float)rw) w = (float)rw;
    if (h > (float)rh) h = (float)rh;
    luma_crop_set_pixels(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f,
                         cy + h * 0.5f, rw, rh);
}

static void luma_enter_crop(int index) {
    float aspect;
    bool fresh;
    if (!g_app.has_image || g_app.loading) return;
    if (index < 0 || index >= 8) return;
    aspect = RATIOS[index].aspect;
    fresh = !g_app.crop_mode;
    if (fresh) {
        g_app.crop_before.rotation = g_app.rotation;
        g_app.crop_before.crop = g_app.crop;
        g_app.crop_before.mirror_h = g_app.mirror_h;
        g_app.crop_before.mirror_v = g_app.mirror_v;
        g_app.crop_before_valid = true;
        g_app.crop_mode = true;
        g_app.crop.left = 0.0f;
        g_app.crop.top = 0.0f;
        g_app.crop.right = 1.0f;
        g_app.crop.bottom = 1.0f;
    }
    g_app.crop_aspect = aspect;
    g_app.crop_preset = index;
    g_app.crop_drag = 0;
    luma_crop_fit_aspect(aspect, true);
    set_fit_view();
    layout_app();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_apply_crop(void) {
    if (!g_app.crop_mode || !g_app.has_image) return;
    if (g_app.crop_before_valid) luma_history_push_value(g_app.crop_before);
    else luma_history_push();
    g_app.crop_mode = false;
    g_app.crop_preset = -1;
    g_app.crop_aspect = 0.0f;
    g_app.crop_drag = 0;
    g_app.crop_before_valid = false;
    luma_mark_changed();
    set_fit_view();
    layout_app();
    show_toast(L"裁剪已应用", false);
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_cancel_crop(void) {
    if (!g_app.crop_mode) return;
    if (g_app.crop_before_valid) {
        g_app.rotation = g_app.crop_before.rotation;
        g_app.crop = g_app.crop_before.crop;
        g_app.mirror_h = g_app.crop_before.mirror_h;
        g_app.mirror_v = g_app.crop_before.mirror_v;
    }
    g_app.crop_mode = false;
    g_app.crop_preset = -1;
    g_app.crop_aspect = 0.0f;
    g_app.crop_drag = 0;
    g_app.crop_before_valid = false;
    if (GetCapture() == g_app.hwnd) ReleaseCapture();
    update_image_view();
    layout_app();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static int luma_crop_hit_test(float x, float y) {
    Rect cr;
    float t = 14.0f;
    bool nl, nr, nt, nb;
    if (!g_app.crop_mode || !g_app.image_visible) return 0;
    cr = luma_crop_screen_rect();
    nl = fabsf(x - cr.left) <= t;
    nr = fabsf(x - cr.right) <= t;
    nt = fabsf(y - cr.top) <= t;
    nb = fabsf(y - cr.bottom) <= t;
    if (nl && nt) return 6;
    if (nr && nt) return 7;
    if (nl && nb) return 8;
    if (nr && nb) return 9;
    if (nl && y >= cr.top - t && y <= cr.bottom + t) return 2;
    if (nr && y >= cr.top - t && y <= cr.bottom + t) return 3;
    if (nt && x >= cr.left - t && x <= cr.right + t) return 4;
    if (nb && x >= cr.left - t && x <= cr.right + t) return 5;
    if (x > cr.left && x < cr.right && y > cr.top && y < cr.bottom) return 1;
    return 0;
}

static int luma_crop_cursor(int handle) {
    switch (handle) {
    case 2:
    case 3: return 4;
    case 4:
    case 5: return 3;
    case 6:
    case 9: return 5;
    case 7:
    case 8: return 6;
    case 1: return 2;
    default: return 0;
    }
}

static void luma_crop_update_drag(float x, float y) {
    int rw, rh;
    int handle;
    float irw, irh;
    float sx, sy, mx, my, dx, dy;
    float L0, T0, R0, B0;
    float aspect;
    Rect ir;

    if (!g_app.crop_mode || g_app.crop_drag == 0) return;
    rotated_pixel_size(&rw, &rh);
    ir = g_app.drag_image_rect;
    irw = rect_w(ir);
    irh = rect_h(ir);
    if (rw < 2 || rh < 2 || irw < 2.0f || irh < 2.0f) return;
    sx = (g_app.drag_start.x - ir.left) * (float)rw / irw;
    sy = (g_app.drag_start.y - ir.top) * (float)rh / irh;
    mx = clampf((x - ir.left) * (float)rw / irw, 0.0f, (float)rw);
    my = clampf((y - ir.top) * (float)rh / irh, 0.0f, (float)rh);
    dx = mx - sx;
    dy = my - sy;
    handle = g_app.crop_drag;
    L0 = g_app.drag_start_crop.left * (float)rw;
    T0 = g_app.drag_start_crop.top * (float)rh;
    R0 = g_app.drag_start_crop.right * (float)rw;
    B0 = g_app.drag_start_crop.bottom * (float)rh;
    aspect = g_app.crop_aspect;

    if (handle == 1) {
        float w = R0 - L0;
        float h = B0 - T0;
        float l = clampf(L0 + dx, 0.0f, (float)rw - w);
        float t = clampf(T0 + dy, 0.0f, (float)rh - h);
        luma_crop_set_pixels(l, t, l + w, t + h, rw, rh);
        return;
    }

    if (aspect <= 0.0f) {
        float l = L0, t = T0, r = R0, b = B0;
        bool left = handle == 2 || handle == 6 || handle == 8;
        bool right = handle == 3 || handle == 7 || handle == 9;
        bool top = handle == 4 || handle == 6 || handle == 7;
        bool bottom = handle == 5 || handle == 8 || handle == 9;
        if (left) l = clampf(L0 + dx, 0.0f, R0 - 12.0f);
        if (right) r = clampf(R0 + dx, L0 + 12.0f, (float)rw);
        if (top) t = clampf(T0 + dy, 0.0f, B0 - 12.0f);
        if (bottom) b = clampf(B0 + dy, T0 + 12.0f, (float)rh);
        luma_crop_set_pixels(l, t, r, b, rw, rh);
        return;
    }

    if (handle == 2 || handle == 3) {
        float cy = (T0 + B0) * 0.5f;
        float l, r, w, h, t;
        if (handle == 2) {
            r = R0;
            l = clampf(L0 + dx, 0.0f, r - 24.0f);
            w = r - l;
        } else {
            l = L0;
            r = clampf(R0 + dx, l + 24.0f, (float)rw);
            w = r - l;
        }
        h = w / aspect;
        if (h > (float)rh) {
            h = (float)rh;
            w = h * aspect;
            if (handle == 2) l = r - w;
            else r = l + w;
        }
        if (l < 0.0f) {
            l = 0.0f;
            w = r;
            h = w / aspect;
        }
        if (r > (float)rw) {
            r = (float)rw;
            w = r - l;
            h = w / aspect;
        }
        t = clampf(cy - h * 0.5f, 0.0f, (float)rh - h);
        luma_crop_set_pixels(l, t, l + w, t + h, rw, rh);
        return;
    }

    if (handle == 4 || handle == 5) {
        float cx = (L0 + R0) * 0.5f;
        float t, b, w, h, l;
        if (handle == 4) {
            b = B0;
            t = clampf(T0 + dy, 0.0f, b - 24.0f);
            h = b - t;
        } else {
            t = T0;
            b = clampf(B0 + dy, t + 24.0f, (float)rh);
            h = b - t;
        }
        w = h * aspect;
        if (w > (float)rw) {
            w = (float)rw;
            h = w / aspect;
            if (handle == 4) t = b - h;
            else b = t + h;
        }
        if (t < 0.0f) {
            t = 0.0f;
            h = b;
            w = h * aspect;
        }
        if (b > (float)rh) {
            b = (float)rh;
            h = b - t;
            w = h * aspect;
        }
        l = clampf(cx - w * 0.5f, 0.0f, (float)rw - w);
        luma_crop_set_pixels(l, t, l + w, t + h, rw, rh);
        return;
    }

    {
        float ax = (handle == 6 || handle == 8) ? R0 : L0;
        float ay = (handle == 6 || handle == 7) ? B0 : T0;
        float dirx = mx < ax ? -1.0f : 1.0f;
        float diry = my < ay ? -1.0f : 1.0f;
        float raw_w = fabsf(mx - ax);
        float raw_h = fabsf(my - ay);
        float w = fmaxf(raw_w, raw_h * aspect);
        float max_w = dirx < 0.0f ? ax : (float)rw - ax;
        float max_h = diry < 0.0f ? ay : (float)rh - ay;
        float h, l, t;
        if (w < 24.0f) w = 24.0f;
        if (w > max_w) w = max_w;
        h = w / aspect;
        if (h > max_h) {
            h = max_h;
            w = h * aspect;
        }
        if (w < 1.0f || h < 1.0f) return;
        l = dirx < 0.0f ? ax - w : ax;
        t = diry < 0.0f ? ay - h : ay;
        luma_crop_set_pixels(l, t, l + w, t + h, rw, rh);
    }
}

static bool luma_crop_begin_drag(float x, float y) {
    int hit = luma_crop_hit_test(x, y);
    if (hit == 0) return false;
    g_app.crop_drag = hit;
    g_app.drag_start = point_make(x, y);
    g_app.drag_start_crop = g_app.crop;
    g_app.drag_image_rect = g_app.image_rect;
    SetCapture(g_app.hwnd);
    return true;
}

static void luma_crop_end_drag(void) {
    if (g_app.crop_drag != 0) {
        g_app.crop_drag = 0;
        if (GetCapture() == g_app.hwnd) ReleaseCapture();
        InvalidateRect(g_app.hwnd, NULL, FALSE);
    }
}

static DWORD WINAPI luma_load_worker(LPVOID param) {
    LumaLoadResult *res = (LumaLoadResult *)param;
    HRESULT co;
    co = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    res->ok = luma_image_load(res->path, &res->image, res->error,
                              sizeof(res->error) / sizeof(res->error[0]));
    if (SUCCEEDED(co)) CoUninitialize();
    if (g_app.hwnd != NULL) {
        PostMessageW(g_app.hwnd, WM_APP_LOAD_DONE, 0, (LPARAM)res);
    }
    return 0;
}

static void luma_finish_load(LumaLoadResult *res) {
    if (res == NULL) return;
    if (g_app.load_thread != NULL) {
        CloseHandle(g_app.load_thread);
        g_app.load_thread = NULL;
    }
    g_app.loading = false;
    if (res->id == g_app.load_id) {
        if (res->ok && res->image.pixels != NULL) {
            luma_image_free(&g_app.image);
            g_app.image = res->image;
            res->image.pixels = NULL;
            g_app.has_image = true;
            g_app.image_visible = true;
            copy_utf16(g_app.path, MAX_PATH, res->path);
            luma_update_meta(res->path);
            g_app.rotation = 0;
            g_app.crop.left = 0.0f;
            g_app.crop.top = 0.0f;
            g_app.crop.right = 1.0f;
            g_app.crop.bottom = 1.0f;
            g_app.mirror_h = false;
            g_app.mirror_v = false;
            g_app.initial_rotation = 0;
            g_app.initial_crop = g_app.crop;
            g_app.initial_mirror_h = false;
            g_app.initial_mirror_v = false;
            g_app.edited = false;
            g_app.exported = false;
            g_app.undo_count = 0;
            g_app.redo_count = 0;
            g_app.crop_mode = false;
            g_app.crop_preset = -1;
            g_app.crop_aspect = 0.0f;
            g_app.crop_before_valid = false;
            g_app.fit = true;
            g_app.zoom = 1.0f;
            g_app.pan = point_make(0.0f, 0.0f);
            g_app.reveal = 0.0f;
            set_fit_view();
            layout_app();
            upload_photo_bitmap();
            InvalidateRect(g_app.hwnd, NULL, FALSE);
        } else {
            if (res->error[0] != L'\0') show_toast(res->error, true);
            else show_toast(L"无法打开这张图片", true);
            if (g_app.has_image) {
                update_image_view();
                layout_app();
                InvalidateRect(g_app.hwnd, NULL, FALSE);
            }
        }
    }
    if (res->image.pixels != NULL) luma_image_free(&res->image);
    free(res->path);
    free(res);
}

static void luma_request_open(const wchar_t *path) {
    LumaLoadResult *res;
    HANDLE thread;
    if (!luma_path_exists(path)) {
        show_toast(L"找不到这个文件", true);
        return;
    }
    if (g_app.loading) {
        show_toast(L"正在载入上一张图片，请稍候", false);
        return;
    }
    res = (LumaLoadResult *)calloc(1, sizeof(*res));
    if (res == NULL) {
        show_toast(L"内存不足，无法载入图片", true);
        return;
    }
    res->path = (wchar_t *)calloc(MAX_PATH, sizeof(wchar_t));
    if (res->path == NULL) {
        free(res);
        show_toast(L"内存不足，无法载入图片", true);
        return;
    }
    copy_utf16(res->path, MAX_PATH, path);
    res->id = ++g_app.load_id;
    g_app.loading = true;
    copy_utf16(g_app.loading_name, 160, luma_file_name(path));
    thread = CreateThread(NULL, 0, luma_load_worker, res, 0, NULL);
    if (thread == NULL) {
        g_app.loading = false;
        free(res->path);
        free(res);
        show_toast(L"无法启动载入线程", true);
        return;
    }
    g_app.load_thread = thread;
    g_app.hover_id = CTRL_NONE;
    layout_app();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static bool luma_open_dialog(void) {
    IFileOpenDialog *dialog = NULL;
    IShellItem *item = NULL;
    wchar_t *path = NULL;
    DWORD options = 0;
    HRESULT hr;
    bool opened = false;
    const COMDLG_FILTERSPEC filters[] = {
        {L"图片文件", L"*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.webp"},
        {L"PNG 图片", L"*.png"},
        {L"JPEG 图片", L"*.jpg;*.jpeg"},
        {L"所有文件", L"*.*"},
    };
    if (g_app.loading) return false;
    hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IFileOpenDialog, (void **)&dialog);
    if (FAILED(hr) || dialog == NULL) {
        show_toast(L"无法打开文件选择器", true);
        return false;
    }
    dialog->lpVtbl->GetOptions(dialog, &options);
    dialog->lpVtbl->SetOptions(dialog, options | FOS_FORCEFILESYSTEM |
                                           FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
    dialog->lpVtbl->SetFileTypes(dialog, 4, filters);
    dialog->lpVtbl->SetTitle(dialog, L"打开图片");
    hr = dialog->lpVtbl->Show(dialog, g_app.hwnd);
    if (SUCCEEDED(hr) &&
        SUCCEEDED(dialog->lpVtbl->GetResult(dialog, &item)) && item != NULL &&
        SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH,
                                                &path)) &&
        path != NULL) {
        luma_request_open(path);
        opened = true;
    }
    if (path != NULL) CoTaskMemFree(path);
    LUMA_RELEASE(item);
    LUMA_RELEASE(dialog);
    if (!opened && FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        show_toast(L"打开文件失败", true);
    }
    return opened;
}

static bool luma_save_dialog(wchar_t *out, size_t cap) {
    IFileSaveDialog *dialog = NULL;
    IShellItem *item = NULL;
    wchar_t *path = NULL;
    wchar_t initial[MAX_PATH];
    const wchar_t *base;
    const wchar_t *dot;
    DWORD options = 0;
    HRESULT hr;
    bool saved = false;
    const COMDLG_FILTERSPEC filters[] = {
        {L"PNG 图片", L"*.png"},
        {L"JPEG 图片", L"*.jpg;*.jpeg"},
        {L"BMP 图片", L"*.bmp"},
        {L"TIFF 图片", L"*.tif;*.tiff"},
    };
    if (out == NULL || cap == 0) return false;
    out[0] = L'\0';
    base = g_app.path[0] ? luma_file_name(g_app.path) : L"fustimage";
    copy_utf16(initial, MAX_PATH, base);
    dot = wcsrchr(initial, L'.');
    if (dot != NULL) initial[dot - initial] = L'\0';
    if (wcslen(initial) + 6 >= MAX_PATH) initial[MAX_PATH - 7] = L'\0';
    wcscat(initial, L"_fustimage");
    hr = CoCreateInstance(&CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IFileSaveDialog, (void **)&dialog);
    if (FAILED(hr) || dialog == NULL) {
        show_toast(L"无法打开保存选择器", true);
        return false;
    }
    dialog->lpVtbl->GetOptions(dialog, &options);
    dialog->lpVtbl->SetOptions(dialog, options | FOS_FORCEFILESYSTEM |
                                           FOS_OVERWRITEPROMPT |
                                           FOS_PATHMUSTEXIST);
    dialog->lpVtbl->SetFileTypes(dialog, 4, filters);
    dialog->lpVtbl->SetFileTypeIndex(dialog, 1);
    dialog->lpVtbl->SetDefaultExtension(dialog, L"png");
    dialog->lpVtbl->SetFileName(dialog, initial);
    dialog->lpVtbl->SetTitle(dialog, L"导出图片");
    hr = dialog->lpVtbl->Show(dialog, g_app.hwnd);
    if (SUCCEEDED(hr) &&
        SUCCEEDED(dialog->lpVtbl->GetResult(dialog, &item)) && item != NULL &&
        SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH,
                                                &path)) &&
        path != NULL) {
        copy_utf16(out, cap, path);
        saved = true;
    }
    if (path != NULL) CoTaskMemFree(path);
    LUMA_RELEASE(item);
    LUMA_RELEASE(dialog);
    if (saved && wcsrchr(out, L'.') == NULL) {
        if (wcslen(out) + 5 < cap) wcscat(out, L".png");
    }
    if (!saved && FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        show_toast(L"保存文件失败", true);
    }
    return saved;
}

static void luma_copy_ext(const wchar_t *path, wchar_t *ext, size_t cap) {
    const wchar_t *name = luma_file_name(path);
    const wchar_t *dot = wcsrchr(name, L'.');
    if (dot != NULL && _wcsicmp(dot, L".jpg") == 0) {
        copy_utf16(ext, cap, L"jpg");
    } else if (dot != NULL && _wcsicmp(dot, L".jpeg") == 0) {
        copy_utf16(ext, cap, L"jpg");
    } else if (dot != NULL && _wcsicmp(dot, L".bmp") == 0) {
        copy_utf16(ext, cap, L"bmp");
    } else if (dot != NULL && _wcsicmp(dot, L".tif") == 0) {
        copy_utf16(ext, cap, L"tiff");
    } else if (dot != NULL && _wcsicmp(dot, L".tiff") == 0) {
        copy_utf16(ext, cap, L"tiff");
    } else if (dot != NULL && _wcsicmp(dot, L".png") == 0) {
        copy_utf16(ext, cap, L"png");
    } else {
        copy_utf16(ext, cap, L"png");
    }
}

static bool luma_copy_path(wchar_t *out, size_t cap) {
    wchar_t full[MAX_PATH];
    wchar_t stem[MAX_PATH];
    wchar_t prefix[MAX_PATH];
    wchar_t candidate[MAX_PATH];
    wchar_t ext[8];
    const wchar_t *base;
    const wchar_t *dot;
    const wchar_t *slash;
    const wchar_t *slash2;
    size_t dir_len;
    size_t stem_len;
    DWORD n;
    int i;

    if (out == NULL || cap == 0) return false;
    out[0] = L'\0';
    if (g_app.path[0] == L'\0') return false;

    n = GetFullPathNameW(g_app.path, MAX_PATH, full, NULL);
    if (n == 0 || n >= MAX_PATH) return false;

    slash = wcsrchr(full, L'\\');
    slash2 = wcsrchr(full, L'/');
    if (slash2 != NULL && (slash == NULL || slash2 > slash)) slash = slash2;
    base = slash != NULL ? slash + 1 : full;
    dir_len = slash != NULL ? (size_t)(slash - full + 1) : 0;

    dot = wcsrchr(base, L'.');
    stem_len = dot != NULL ? (size_t)(dot - base) : wcslen(base);
    if (stem_len == 0 || stem_len >= sizeof(stem) / sizeof(stem[0])) {
        return false;
    }
    wmemcpy(stem, base, stem_len);
    stem[stem_len] = L'\0';

    luma_copy_ext(full, ext, sizeof(ext) / sizeof(ext[0]));
    if (swprintf(prefix, MAX_PATH, L"%.*ls%ls", (int)dir_len, full, stem) < 0) {
        return false;
    }

    for (i = 1; i < 1000; ++i) {
        int used;
        if (i == 1) {
            used = swprintf(candidate, MAX_PATH, L"%ls_copy.%ls", prefix, ext);
        } else {
            used = swprintf(candidate, MAX_PATH, L"%ls_copy-%d.%ls", prefix,
                            i, ext);
        }
        if (used <= 0 || used >= MAX_PATH) continue;
        if (!luma_path_exists(candidate)) {
            copy_utf16(out, cap, candidate);
            return true;
        }
    }
    return false;
}

static void luma_export(void) {
    wchar_t path[MAX_PATH];
    wchar_t error[512];
    if (!g_app.has_image || g_app.loading) return;
    if (!luma_save_dialog(path, MAX_PATH)) return;
    error[0] = L'\0';
    if (luma_image_export(&g_app.image, g_app.rotation, g_app.mirror_h,
                          g_app.mirror_v, g_app.crop, path, error,
                          sizeof(error) / sizeof(error[0]))) {
        g_app.exported = true;
        luma_update_edited();
        show_toast(L"图片已导出", false);
        layout_app();
        InvalidateRect(g_app.hwnd, NULL, FALSE);
    } else {
        show_toast(error[0] ? error : L"导出失败", true);
    }
}

static void luma_copy_beside(void) {
    wchar_t path[MAX_PATH];
    wchar_t error[512];
    if (!g_app.has_image || g_app.loading) return;
    if (!luma_copy_path(path, MAX_PATH)) {
        show_toast(L"无法生成副本路径", true);
        return;
    }
    error[0] = L'\0';
    if (luma_image_export(&g_app.image, g_app.rotation, g_app.mirror_h,
                          g_app.mirror_v, g_app.crop, path, error,
                          sizeof(error) / sizeof(error[0]))) {
        g_app.exported = true;
        luma_update_edited();
        show_toast(L"副本已保存", false);
        layout_app();
        InvalidateRect(g_app.hwnd, NULL, FALSE);
    } else {
        show_toast(error[0] ? error : L"复制失败", true);
    }
}

static void luma_set_theme(bool dark) {
    g_app.dark = dark;
    apply_theme_to_window();
    layout_app();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_activate(int id) {
    LumaControl *c = find_control(id);
    if (c != NULL && !c->enabled) return;
    switch (id) {
    case CTRL_OPEN: luma_open_dialog(); break;
    case CTRL_UNDO: luma_undo(); break;
    case CTRL_REDO: luma_redo(); break;
    case CTRL_EXPORT: luma_export(); break;
    case CTRL_COPY: luma_copy_beside(); break;
    case CTRL_ROT_L90: luma_rotate(3); break;
    case CTRL_ROT_R90: luma_rotate(1); break;
    case CTRL_ROT_L180:
    case CTRL_ROT_R180: luma_rotate(2); break;
    case CTRL_FLIP_H: luma_mirror(true); break;
    case CTRL_FLIP_V: luma_mirror(false); break;
    case CTRL_APPLY_CROP: luma_apply_crop(); break;
    case CTRL_CANCEL_CROP: luma_cancel_crop(); break;
    case CTRL_THEME: luma_set_theme(!g_app.dark); break;
    default:
        if (id >= CTRL_RATIO_16_9 && id <= CTRL_RATIO_FREE) {
            luma_enter_crop(id - CTRL_RATIO_16_9);
        }
        break;
    }
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static LumaControl *luma_control_at(float x, float y) {
    int i;
    for (i = 0; i < g_app.control_count; ++i) {
        LumaControl *c = &g_app.controls[i];
        if (c->scrolled &&
            (y < g_app.tools_top || y > g_app.tools_bottom)) {
            continue;
        }
        if (rect_contains(c->rect, x, y)) return c;
    }
    return NULL;
}

static void luma_focus_ensure_visible(LumaControl *c) {
    float top, bottom, target;
    if (c == NULL || !c->scrolled) return;
    top = c->rect.top;
    bottom = c->rect.bottom;
    target = g_app.scroll_y;
    if (top < g_app.tools_top + 4.0f)
        target += top - (g_app.tools_top + 4.0f);
    else if (bottom > g_app.tools_bottom - 4.0f)
        target += bottom - (g_app.tools_bottom - 4.0f);
    target = clampf(target, 0.0f, g_app.scroll_max);
    if (fabsf(target - g_app.scroll_y) > 0.1f) {
        g_app.scroll_y = target;
        layout_app();
    }
}

static void luma_move_focus(int direction) {
    int order[MAX_CONTROLS];
    int count = 0;
    int current = -1;
    int next = 0;
    int i;
    LumaControl *c;
    for (i = 0; i < g_app.control_count; ++i) {
        if (g_app.controls[i].enabled) {
            if (g_app.controls[i].id == g_app.focus_id) current = count;
            order[count++] = g_app.controls[i].id;
        }
    }
    if (count == 0) return;
    if (current < 0) next = direction > 0 ? 0 : count - 1;
    else next = (current + direction + count) % count;
    g_app.focus_id = order[next];
    g_app.focus_visible = true;
    c = find_control(g_app.focus_id);
    luma_focus_ensure_visible(c);
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_activate_focused(void) {
    LumaControl *c;
    if (g_app.focus_id < 0) return;
    c = find_control(g_app.focus_id);
    if (c != NULL && c->enabled) luma_activate(c->id);
}

static void luma_set_cursor(int kind) {
    LPCWSTR name = (LPCWSTR)IDC_ARROW;
    g_app.cursor_kind = kind;
    switch (kind) {
    case 1: name = (LPCWSTR)IDC_HAND; break;
    case 2: name = (LPCWSTR)IDC_SIZEALL; break;
    case 3: name = (LPCWSTR)IDC_SIZENS; break;
    case 4: name = (LPCWSTR)IDC_SIZEWE; break;
    case 5: name = (LPCWSTR)IDC_SIZENWSE; break;
    case 6: name = (LPCWSTR)IDC_SIZENESW; break;
    default: break;
    }
    SetCursor(LoadCursorW(NULL, name));
}

static void luma_update_hover(float x, float y) {
    LumaControl *c = luma_control_at(x, y);
    int id = c ? c->id : CTRL_NONE;
    int kind = 0;
    if (id != g_app.hover_id) {
        g_app.hover_id = id;
        g_app.hover_since = GetTickCount64();
        if (id >= 0) SetTimer(g_app.hwnd, TIMER_TOOLTIP, 700, NULL);
        else KillTimer(g_app.hwnd, TIMER_TOOLTIP);
    }
    if (g_app.crop_mode) {
        int hit = g_app.crop_drag ? g_app.crop_drag
                                  : luma_crop_hit_test(x, y);
        kind = luma_crop_cursor(hit);
    }
    if (kind == 0 && id >= 0) kind = 1;
    if (kind == 0 && !g_app.crop_mode && g_app.image_visible &&
        rect_contains(g_app.image_rect, x, y)) {
        kind = 2;
    }
    luma_set_cursor(kind);
}
