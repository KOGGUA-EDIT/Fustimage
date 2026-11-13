/* Luma window, input handling and application startup.
   Included at the end of main.c after ui_actions.inc. */

static bool g_luma_tracking_mouse;

static const IID kLumaDWriteFactoryIID = {
    0xb859ee5a, 0xd838, 0x4b5b,
    {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}
};

static void luma_tick_now(void) {
    ULONGLONG tick = GetTickCount64();
    if (g_app.last_tick == 0) g_app.last_tick = tick;
    g_app.now += (double)(tick - g_app.last_tick) / 1000.0;
    g_app.last_tick = tick;
}

static void luma_read_size(void) {
    RECT rc;
    UINT dpi;
    if (g_app.hwnd == NULL) return;
    GetClientRect(g_app.hwnd, &rc);
    dpi = GetDpiForWindow(g_app.hwnd);
    if (dpi == 0) dpi = 96;
    g_app.dpi = (float)dpi;
    g_app.width = (float)rc.right * 96.0f / (float)dpi;
    g_app.height = (float)rc.bottom * 96.0f / (float)dpi;
}

static void luma_resize_target(void) {
    RECT rc;
    D2D1_SIZE_U size;
    if (g_app.hwnd_rt == NULL || g_app.hwnd == NULL) return;
    GetClientRect(g_app.hwnd, &rc);
    size.width = (UINT)(rc.right > 0 ? rc.right : 1);
    size.height = (UINT)(rc.bottom > 0 ? rc.bottom : 1);
    g_app.hwnd_rt->lpVtbl->Resize(g_app.hwnd_rt, &size);
}

static Point luma_mouse_dip(LPARAM lp) {
    float scale = g_app.dpi > 0.0f ? 96.0f / g_app.dpi : 1.0f;
    return point_make((float)GET_X_LPARAM(lp) * scale,
                      (float)GET_Y_LPARAM(lp) * scale);
}

static void luma_mouse_move(float x, float y) {
    g_app.mouse_x = x;
    g_app.mouse_y = y;
    if (g_app.panning) {
        g_app.pan.x = g_app.pan_origin.x + (x - g_app.pan_start.x);
        g_app.pan.y = g_app.pan_origin.y + (y - g_app.pan_start.y);
        update_image_view();
        luma_set_cursor(2);
    } else if (g_app.crop_drag != 0) {
        luma_crop_update_drag(x, y);
        luma_set_cursor(luma_crop_cursor(g_app.crop_drag));
    } else {
        luma_update_hover(x, y);
    }
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_mouse_down(float x, float y) {
    LumaControl *c;
    SetFocus(g_app.hwnd);
    g_app.mouse_down = true;
    g_app.pressed_id = CTRL_NONE;
    c = luma_control_at(x, y);
    if (c != NULL) {
        g_app.pressed_id = c->id;
        g_app.focus_id = c->id;
        g_app.focus_visible = false;
        SetCapture(g_app.hwnd);
    } else if (g_app.crop_mode) {
        luma_crop_begin_drag(x, y);
    } else if (g_app.has_image && g_app.image_visible &&
               rect_contains(g_app.image_rect, x, y)) {
        g_app.panning = true;
        g_app.pan_start = point_make(x, y);
        g_app.pan_origin = g_app.pan;
        SetCapture(g_app.hwnd);
        luma_set_cursor(2);
    }
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static void luma_mouse_up(float x, float y) {
    int pressed = g_app.pressed_id;
    g_app.mouse_down = false;
    g_app.pressed_id = CTRL_NONE;
    if (GetCapture() == g_app.hwnd) ReleaseCapture();
    if (pressed != CTRL_NONE) {
        LumaControl *c = luma_control_at(x, y);
        if (c != NULL && c->id == pressed) luma_activate(pressed);
    }
    g_app.panning = false;
    if (g_app.crop_drag != 0) luma_crop_end_drag();
    luma_update_hover(x, y);
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

static bool luma_handle_key(WPARAM key, LPARAM lp) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    float cx = rect_cx(g_app.canvas);
    float cy = rect_cy(g_app.canvas);
    (void)lp;

    if (ctrl) {
        switch (key) {
        case 'O': luma_open_dialog(); return true;
        case 'S': luma_export(); return true;
        case 'Z':
            if (shift) luma_redo();
            else luma_undo();
            return true;
        case 'Y': luma_redo(); return true;
        case '0': set_fit_view(); InvalidateRect(g_app.hwnd, NULL, FALSE); return true;
        case VK_OEM_PLUS:
        case VK_ADD:
            zoom_view(1.15f, cx, cy);
            InvalidateRect(g_app.hwnd, NULL, FALSE);
            return true;
        case VK_OEM_MINUS:
        case VK_SUBTRACT:
            zoom_view(1.0f / 1.15f, cx, cy);
            InvalidateRect(g_app.hwnd, NULL, FALSE);
            return true;
        default: break;
        }
    }
    switch (key) {
    case VK_TAB:
        luma_move_focus(shift ? -1 : 1);
        return true;
    case VK_RETURN:
        if (g_app.crop_mode) luma_apply_crop();
        else luma_activate_focused();
        return true;
    case VK_SPACE:
        luma_activate_focused();
        return true;
    case VK_ESCAPE:
        if (g_app.crop_mode) luma_cancel_crop();
        else {
            g_app.focus_id = CTRL_NONE;
            g_app.focus_visible = false;
            InvalidateRect(g_app.hwnd, NULL, FALSE);
        }
        return true;
    case 'R':
        if (g_app.has_image && !g_app.loading) {
            luma_rotate(shift ? 3 : 1);
            InvalidateRect(g_app.hwnd, NULL, FALSE);
        }
        return true;
    case 'H':
        luma_mirror(!shift);
        return true;
    case 'T':
        luma_set_theme(!g_app.dark);
        return true;
    case 'F':
        set_fit_view();
        InvalidateRect(g_app.hwnd, NULL, FALSE);
        return true;
    default: break;
    }
    return false;
}

static void luma_wheel(float x, float y, short delta) {
    float steps = (float)delta / (float)WHEEL_DELTA;
    if (x < g_app.sidebar_w) {
        g_app.scroll_y = clampf(g_app.scroll_y - steps * 52.0f, 0.0f,
                                g_app.scroll_max);
        layout_app();
        luma_update_hover(x, y);
        InvalidateRect(g_app.hwnd, NULL, FALSE);
    } else if (g_app.has_image && g_app.image_visible) {
        zoom_view(steps > 0.0f ? 1.12f : 1.0f / 1.12f, x, y);
        InvalidateRect(g_app.hwnd, NULL, FALSE);
    }
}

static void luma_initial_rect(RECT *out) {
    RECT work;
    RECT wr;
    int client_w;
    int client_h;
    int width;
    int height;
    float scale = g_app.dpi > 0.0f ? g_app.dpi / 96.0f : 1.0f;
    if (scale < 1.0f) scale = 1.0f;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    client_w = (int)(1320.0f * scale);
    client_h = (int)(820.0f * scale);
    if (client_w > (work.right - work.left) * 92 / 100)
        client_w = (work.right - work.left) * 92 / 100;
    if (client_h > (work.bottom - work.top) * 92 / 100)
        client_h = (work.bottom - work.top) * 92 / 100;
    wr.left = 0;
    wr.top = 0;
    wr.right = client_w;
    wr.bottom = client_h;
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
    width = wr.right - wr.left;
    height = wr.bottom - wr.top;
    out->left = work.left + ((work.right - work.left) - width) / 2;
    out->top = work.top + ((work.bottom - work.top) - height) / 2;
    out->right = out->left + width;
    out->bottom = out->top + height;
}

static void luma_parse_command_line(void) {
    int argc = 0;
    int i;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) return;
    for (i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--dark") == 0) {
            g_app.startup_dark = true;
        } else if (wcscmp(argv[i], L"--light") == 0) {
            g_app.startup_light = true;
        } else if (wcsncmp(argv[i], L"--crop=", 7) == 0) {
            copy_utf16(g_app.startup_crop, 16, argv[i] + 7);
        } else if (argv[i][0] != L'-' && g_app.startup_path[0] == L'\0') {
            wchar_t resolved[MAX_PATH];
            DWORD n = GetFullPathNameW(argv[i], MAX_PATH, resolved, NULL);
            if (n > 0 && n < MAX_PATH)
                copy_utf16(g_app.startup_path, MAX_PATH, resolved);
            else
                copy_utf16(g_app.startup_path, MAX_PATH, argv[i]);
        }
    }
    LocalFree(argv);
}

static void luma_release_formats(void) {
    LUMA_RELEASE(g_app.f_brand);
    LUMA_RELEASE(g_app.f_h1);
    LUMA_RELEASE(g_app.f_h2);
    LUMA_RELEASE(g_app.f_title);
    LUMA_RELEASE(g_app.f_body);
    LUMA_RELEASE(g_app.f_body_strong);
    LUMA_RELEASE(g_app.f_label);
    LUMA_RELEASE(g_app.f_button);
    LUMA_RELEASE(g_app.f_caption);
    LUMA_RELEASE(g_app.f_tiny);
}

static bool luma_create_factories(void) {
    HRESULT hr;
    D2D1_FACTORY_OPTIONS options;
    memset(&options, 0, sizeof(options));
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           &IID_ID2D1Factory, &options,
                           (void **)&g_app.d2d);
    if (FAILED(hr) || g_app.d2d == NULL) return false;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             &kLumaDWriteFactoryIID,
                             (IUnknown **)&g_app.dwrite);
    if (FAILED(hr) || g_app.dwrite == NULL) return false;
    return create_text_formats();
}

static LRESULT CALLBACK luma_wndproc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_app.hwnd = hwnd;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        (void)hdc;
        if (g_app.rt == NULL) create_device_resources();
        luma_tick_now();
        paint_app();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        luma_read_size();
        luma_resize_target();
        if (g_app.width >= 1.0f && g_app.height >= 1.0f) {
            layout_app();
            update_image_view();
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lParam;
        g_app.dpi = (float)HIWORD(wParam);
        if (g_app.hwnd != NULL && suggested != NULL) {
            SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
        create_device_resources();
        luma_read_size();
        layout_app();
        update_image_view();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lParam;
        UINT dpi = g_app.hwnd ? GetDpiForWindow(g_app.hwnd) : 96;
        float scale = (float)dpi / 96.0f;
        mmi->ptMinTrackSize.x = (LONG)(720.0f * scale) + 16;
        mmi->ptMinTrackSize.y = (LONG)(560.0f * scale) + 39;
        return 0;
    }
    case WM_MOUSEMOVE:
        if (!g_luma_tracking_mouse) {
            TRACKMOUSEEVENT tme;
            memset(&tme, 0, sizeof(tme));
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            g_luma_tracking_mouse = true;
        }
        {
            Point p = luma_mouse_dip(lParam);
            luma_mouse_move(p.x, p.y);
        }
        return 0;
    case WM_MOUSELEAVE:
        g_luma_tracking_mouse = false;
        g_app.hover_id = CTRL_NONE;
        KillTimer(hwnd, TIMER_TOOLTIP);
        luma_set_cursor(0);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        Point p = luma_mouse_dip(lParam);
        luma_mouse_down(p.x, p.y);
        return 0;
    }
    case WM_LBUTTONUP: {
        Point p = luma_mouse_dip(lParam);
        luma_mouse_up(p.x, p.y);
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        if (!g_app.crop_mode && g_app.has_image) {
            set_fit_view();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ScreenToClient(hwnd, &pt);
        luma_wheel((float)pt.x * 96.0f / g_app.dpi,
                   (float)pt.y * 96.0f / g_app.dpi,
                   GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            luma_set_cursor(g_app.cursor_kind);
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
        if (luma_handle_key(wParam, lParam)) return 0;
        break;
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wParam;
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(drop, 0, path, MAX_PATH) > 0)
            luma_request_open(path);
        DragFinish(drop);
        return 0;
    }
    case WM_TIMER:
        if (wParam == TIMER_TOOLTIP) {
            KillTimer(hwnd, TIMER_TOOLTIP);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == TIMER_ANIM) {
            bool repaint = false;
            luma_tick_now();
            if (g_app.reveal < 1.0f) {
                g_app.reveal += 0.09f;
                if (g_app.reveal > 1.0f) g_app.reveal = 1.0f;
                repaint = true;
            }
            if (g_app.toast_active && g_app.now >= g_app.toast_until) {
                g_app.toast_active = false;
                repaint = true;
            }
            if (g_app.loading) repaint = true;
            if (repaint) InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        break;
    case WM_APP_LOAD_DONE:
        luma_finish_load((LumaLoadResult *)lParam);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ANIM);
        KillTimer(hwnd, TIMER_TOOLTIP);
        PostQuitMessage(0);
        return 0;
    case WM_SETFOCUS:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_KILLFOCUS:
        g_app.focus_visible = false;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line,
                    int show) {
    WNDCLASSEXW wc;
    RECT initial;
    HWND hwnd;
    MSG msg;
    HRESULT co;
    UINT dpi;
    (void)previous;
    (void)command_line;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    (void)co;
    memset(&g_app, 0, sizeof(g_app));
    g_app.instance = instance;
    g_app.focus_id = CTRL_NONE;
    g_app.hover_id = CTRL_NONE;
    g_app.pressed_id = CTRL_NONE;
    g_app.toast_active = false;
    g_app.last_tick = GetTickCount64();
    g_app.now = 0.0;
    g_app.dpi = (float)GetDpiForSystem();
    if (g_app.dpi < 96.0f) g_app.dpi = 96.0f;
    theme_light(&g_app.theme);
    luma_parse_command_line();
    if (g_app.startup_dark && !g_app.startup_light) g_app.dark = true;
    if (g_app.dark) theme_dark(&g_app.theme);

    if (!luma_create_factories()) {
        MessageBoxW(NULL, L"无法初始化 Direct2D / DirectWrite。",
                    L"Fustimage", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = luma_wndproc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (wc.hIcon == NULL) wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"无法注册窗口类。", L"Fustimage",
                    MB_ICONERROR | MB_OK);
        luma_release_formats();
        LUMA_RELEASE(g_app.dwrite);
        LUMA_RELEASE(g_app.d2d);
        CoUninitialize();
        return 1;
    }

    dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;
    g_app.dpi = (float)dpi;
    luma_initial_rect(&initial);
    hwnd = CreateWindowExW(
        0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        initial.left, initial.top, initial.right - initial.left,
        initial.bottom - initial.top, NULL, NULL, instance, NULL);
    if (hwnd == NULL) {
        MessageBoxW(NULL, L"无法创建应用窗口。", L"Fustimage",
                    MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }
    g_app.hwnd = hwnd;
    DragAcceptFiles(hwnd, TRUE);
    luma_read_size();
    apply_theme_to_window();
    create_device_resources();
    layout_app();
    update_image_view();
    SetTimer(hwnd, TIMER_ANIM, 16, NULL);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    if (g_app.startup_path[0] != L'\0') luma_request_open(g_app.startup_path);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_app.load_thread != NULL) {
        WaitForSingleObject(g_app.load_thread, 3000);
        CloseHandle(g_app.load_thread);
        g_app.load_thread = NULL;
    }
    discard_device_resources();
    luma_release_formats();
    LUMA_RELEASE(g_app.d2d);
    LUMA_RELEASE(g_app.dwrite);
    luma_image_free(&g_app.image);
    CoUninitialize();
    return (int)msg.wParam;
}
