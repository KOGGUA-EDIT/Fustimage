#include "image.h"

#include <math.h>
#include <objbase.h>
#include <propvarutil.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wincodec.h>

#define LUMA_RELEASE(p)                                                        \
    do {                                                                       \
        if ((p) != NULL) {                                                     \
            (p)->lpVtbl->Release(p);                                           \
            (p) = NULL;                                                        \
        }                                                                      \
    } while (0)

static void set_error(wchar_t *error, size_t cap, const wchar_t *fmt, ...) {
    va_list ap;
    if (error == NULL || cap == 0) {
        return;
    }
    va_start(ap, fmt);
    vswprintf(error, cap, fmt, ap);
    va_end(ap);
    error[cap - 1] = L'\0';
}

static void set_hresult_error(wchar_t *error, size_t cap, const wchar_t *what,
                              HRESULT hr) {
    set_error(error, cap, L"%ls（错误码 0x%08lX）", what,
              (unsigned long)hr);
}

static int exif_orientation(IWICBitmapFrameDecode *frame) {
    IWICMetadataQueryReader *reader = NULL;
    PROPVARIANT value;
    int orientation = 1;

    PropVariantInit(&value);
    if (SUCCEEDED(frame->lpVtbl->GetMetadataQueryReader(frame, &reader)) &&
        reader != NULL) {
        HRESULT hr = reader->lpVtbl->GetMetadataByName(
            reader, L"System.Photo.Orientation", &value);
        if (FAILED(hr) || value.vt == VT_EMPTY) {
            PropVariantClear(&value);
            PropVariantInit(&value);
            reader->lpVtbl->GetMetadataByName(
                reader, L"/app1/ifd/{ushort=274}", &value);
        }
        if (value.vt == VT_UI2) {
            orientation = value.uiVal;
        } else if (value.vt == VT_UI4) {
            orientation = (int)value.ulVal;
        } else if (value.vt == VT_I2) {
            orientation = value.iVal;
        } else if (value.vt == VT_I4) {
            orientation = (int)value.lVal;
        }
        PropVariantClear(&value);
        LUMA_RELEASE(reader);
    }
    if (orientation < 1 || orientation > 8) {
        orientation = 1;
    }
    return orientation;
}

/* Keep this table next to the reader: EXIF 5..8 transpose the image. */
static void apply_orientation_in_place(LumaImage *image, int orientation) {
    UINT w = image->width;
    UINT h = image->height;
    UINT nw = (orientation >= 5) ? h : w;
    UINT nh = (orientation >= 5) ? w : h;
    UINT32 *src = (UINT32 *)image->pixels;
    UINT32 *dst;
    UINT x, y;

    if (orientation <= 1) {
        return;
    }
    dst = (UINT32 *)malloc((size_t)nw * nh * sizeof(UINT32));
    if (dst == NULL) {
        return;
    }

    for (y = 0; y < nh; ++y) {
        for (x = 0; x < nw; ++x) {
            UINT sx = x;
            UINT sy = y;
            switch (orientation) {
            case 2:
                sx = w - 1 - x;
                break;
            case 3:
                sx = w - 1 - x;
                sy = h - 1 - y;
                break;
            case 4:
                sy = h - 1 - y;
                break;
            case 5:
                sx = y;
                sy = x;
                break;
            case 6:
                sx = y;
                sy = h - 1 - x;
                break;
            case 7:
                sx = w - 1 - y;
                sy = h - 1 - x;
                break;
            case 8:
                sx = w - 1 - y;
                sy = x;
                break;
            default:
                break;
            }
            dst[(size_t)y * nw + x] = src[(size_t)sy * w + sx];
        }
    }

    free(image->pixels);
    image->pixels = (uint8_t *)dst;
    image->width = nw;
    image->height = nh;
    image->stride = nw * 4;
}

static bool load_any(const wchar_t *path, UINT max_side, LumaImage *out,
                     wchar_t *error, size_t error_cap) {
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICBitmapScaler *scaler = NULL;
    IWICFormatConverter *converter = NULL;
    UINT width = 0;
    UINT height = 0;
    UINT stride = 0;
    UINT buffer_size = 0;
    uint8_t *pixels = NULL;
    HRESULT hr;
    bool com_ready = false;
    int orientation = 1;

    if (path == NULL || out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        com_ready = true;
    } else if (hr == RPC_E_CHANGED_MODE) {
        com_ready = false;
    } else {
        set_hresult_error(error, error_cap, L"无法初始化图像组件", hr);
        return false;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法初始化图像解码器", hr);
        goto fail;
    }

    hr = factory->lpVtbl->CreateDecoderFromFilename(
        factory, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr)) {
        set_error(error, error_cap,
                  L"无法打开这张图片，文件可能损坏或格式不受支持");
        goto fail;
    }

    hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法读取图片画面", hr);
        goto fail;
    }

    orientation = exif_orientation(frame);

    {
        IWICBitmapSource *source = (IWICBitmapSource *)frame;
        UINT fw = 0, fh = 0;
        frame->lpVtbl->GetSize(frame, &fw, &fh);
        if (max_side > 0 && fw > 0 && fh > 0 &&
            (fw > max_side || fh > max_side)) {
            double scale = (double)max_side / (double)(fw > fh ? fw : fh);
            UINT dw = (UINT)((double)fw * scale + 0.5);
            UINT dh = (UINT)((double)fh * scale + 0.5);
            if (dw == 0) dw = 1;
            if (dh == 0) dh = 1;
            if (SUCCEEDED(factory->lpVtbl->CreateBitmapScaler(factory,
                                                             &scaler)) &&
                SUCCEEDED(scaler->lpVtbl->Initialize(
                    scaler, (IWICBitmapSource *)frame, dw, dh,
                    WICBitmapInterpolationModeFant))) {
                source = (IWICBitmapSource *)scaler;
            }
        }
        hr = factory->lpVtbl->CreateFormatConverter(factory, &converter);
        if (FAILED(hr)) {
            set_hresult_error(error, error_cap, L"无法准备图像像素", hr);
            goto fail;
        }
        hr = converter->lpVtbl->Initialize(
            converter, source, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            set_hresult_error(error, error_cap, L"无法转换图片格式", hr);
            goto fail;
        }
    }

    hr = converter->lpVtbl->GetSize(converter, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        set_error(error, error_cap, L"图片尺寸无效");
        goto fail;
    }
    if (width > 65535 || height > 65535 ||
        (size_t)width * height > SIZE_MAX / 4u) {
        set_error(error, error_cap, L"图片尺寸过大，无法载入");
        goto fail;
    }

    stride = width * 4;
    buffer_size = (UINT)((size_t)width * height * 4u);
    pixels = (uint8_t *)malloc(buffer_size);
    if (pixels == NULL) {
        set_error(error, error_cap, L"内存不足，无法载入这张图片");
        goto fail;
    }
    hr = converter->lpVtbl->CopyPixels(converter, NULL, stride, buffer_size,
                                       pixels);
    if (FAILED(hr)) {
        set_error(error, error_cap, L"读取图片像素失败");
        goto fail;
    }

    out->pixels = pixels;
    out->width = width;
    out->height = height;
    out->stride = stride;
    apply_orientation_in_place(out, orientation);

    LUMA_RELEASE(converter);
    LUMA_RELEASE(scaler);
    LUMA_RELEASE(frame);
    LUMA_RELEASE(decoder);
    LUMA_RELEASE(factory);
    if (com_ready) {
        CoUninitialize();
    }
    return true;

fail:
    free(pixels);
    LUMA_RELEASE(converter);
    LUMA_RELEASE(scaler);
    LUMA_RELEASE(frame);
    LUMA_RELEASE(decoder);
    LUMA_RELEASE(factory);
    if (com_ready) {
        CoUninitialize();
    }
    return false;
}

void luma_image_free(LumaImage *image) {
    if (image == NULL) {
        return;
    }
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}

static bool write_encoded_image(const uint8_t *bgra, UINT width, UINT height,
                                const wchar_t *path, wchar_t *error,
                                size_t error_cap) {
    IWICImagingFactory *factory = NULL;
    IWICStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IPropertyBag2 *properties = NULL;
    const wchar_t *dot;
    wchar_t ext[16] = L"";
    wchar_t temp_path[MAX_PATH + 32];
    const GUID *container = NULL;
    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
    WICPixelFormatGUID expected_format;
    uint8_t *packed = NULL;
    const uint8_t *source = bgra;
    UINT source_stride = width * 4;
    UINT packed_stride = 0;
    UINT packed_size = 0;
    BOOL is_jpeg = FALSE;
    HRESULT hr;
    HANDLE temp_handle;
    bool moved = false;

    dot = wcsrchr(path, L'.');
    if (dot != NULL && dot[1] != L'\0') {
        size_t i;
        for (i = 0; i < 15 && dot[1 + i] != L'\0'; ++i) {
            wchar_t c = dot[1 + i];
            if (c >= L'A' && c <= L'Z') {
                c = (wchar_t)(c - L'A' + L'a');
            }
            ext[i] = c;
        }
        ext[i] = L'\0';
    }

    if (wcscmp(ext, L"png") == 0) {
        container = &GUID_ContainerFormatPng;
        pixel_format = GUID_WICPixelFormat32bppBGRA;
    } else if (wcscmp(ext, L"jpg") == 0 || wcscmp(ext, L"jpeg") == 0) {
        container = &GUID_ContainerFormatJpeg;
        pixel_format = GUID_WICPixelFormat24bppBGR;
        is_jpeg = TRUE;
    } else if (wcscmp(ext, L"bmp") == 0) {
        container = &GUID_ContainerFormatBmp;
        pixel_format = GUID_WICPixelFormat24bppBGR;
    } else if (wcscmp(ext, L"tif") == 0 || wcscmp(ext, L"tiff") == 0) {
        container = &GUID_ContainerFormatTiff;
        pixel_format = GUID_WICPixelFormat32bppBGRA;
    } else {
        set_error(error, error_cap, L"请使用 PNG、JPEG、BMP 或 TIFF 格式导出");
        return false;
    }

    if (IsEqualGUID(&pixel_format, &GUID_WICPixelFormat24bppBGR)) {
        UINT x, y;
        uint64_t total;
        packed_stride = width * 3;
        total = (uint64_t)packed_stride * height;
        if (total > 0xFFFFFFFFu) {
            set_error(error, error_cap, L"图片尺寸过大，无法导出");
            return false;
        }
        packed_size = (UINT)total;
        packed = (uint8_t *)malloc(packed_size);
        if (packed == NULL) {
            set_error(error, error_cap, L"内存不足，无法导出图片");
            return false;
        }
        for (y = 0; y < height; ++y) {
            const uint8_t *src_row = bgra + (size_t)y * source_stride;
            uint8_t *dst_row = packed + (size_t)y * packed_stride;
            for (x = 0; x < width; ++x) {
                unsigned alpha = src_row[x * 4 + 3];
                unsigned b = src_row[x * 4 + 0];
                unsigned g = src_row[x * 4 + 1];
                unsigned r = src_row[x * 4 + 2];
                if (alpha < 255) {
                    /* JPEG/BMP have no alpha: composite over white. */
                    r = (r * alpha + 255u * (255u - alpha)) / 255u;
                    g = (g * alpha + 255u * (255u - alpha)) / 255u;
                    b = (b * alpha + 255u * (255u - alpha)) / 255u;
                }
                dst_row[x * 3 + 0] = (uint8_t)b;
                dst_row[x * 3 + 1] = (uint8_t)g;
                dst_row[x * 3 + 2] = (uint8_t)r;
            }
        }
        source = packed;
        source_stride = packed_stride;
    } else {
        uint64_t total = (uint64_t)source_stride * height;
        if (total > 0xFFFFFFFFu) {
            set_error(error, error_cap, L"图片尺寸过大，无法导出");
            return false;
        }
        packed_size = (UINT)total;
    }

    if (swprintf(temp_path, MAX_PATH + 32, L"%ls.luma-tmp", path) < 0) {
        set_error(error, error_cap, L"导出路径过长");
        free(packed);
        return false;
    }
    DeleteFileW(temp_path);
    temp_handle = CreateFileW(temp_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (temp_handle == INVALID_HANDLE_VALUE) {
        set_error(error, error_cap, L"无法在目标位置创建文件");
        free(packed);
        return false;
    }
    CloseHandle(temp_handle);

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) {
        set_error(error, error_cap, L"无法初始化图片编码器");
        goto fail;
    }
    hr = factory->lpVtbl->CreateStream(factory, &stream);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法创建导出文件流", hr);
        goto fail;
    }
    hr = stream->lpVtbl->InitializeFromFilename(stream, temp_path,
                                                GENERIC_WRITE);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法写入导出文件", hr);
        goto fail;
    }
    hr = factory->lpVtbl->CreateEncoder(factory, container, NULL, &encoder);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法创建图片编码器", hr);
        goto fail;
    }
    hr = encoder->lpVtbl->Initialize(encoder, (IStream *)stream,
                                     WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法初始化图片编码器", hr);
        goto fail;
    }
    hr = encoder->lpVtbl->CreateNewFrame(encoder, &frame, &properties);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法准备导出画面", hr);
        goto fail;
    }
    if (is_jpeg && properties != NULL) {
        PROPBAG2 option;
        VARIANT quality;
        memset(&option, 0, sizeof(option));
        option.pstrName = (LPOLESTR)L"ImageQuality";
        VariantInit(&quality);
        quality.vt = VT_R4;
        quality.fltVal = 0.92f;
        properties->lpVtbl->Write(properties, 1, &option, &quality);
        VariantClear(&quality);
    }
    hr = frame->lpVtbl->Initialize(frame, properties);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法初始化导出画面", hr);
        goto fail;
    }
    hr = frame->lpVtbl->SetSize(frame, width, height);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法设置导出尺寸", hr);
        goto fail;
    }
    hr = frame->lpVtbl->SetResolution(frame, 96.0, 96.0);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法设置导出分辨率", hr);
        goto fail;
    }
    expected_format = pixel_format;
    hr = frame->lpVtbl->SetPixelFormat(frame, &pixel_format);
    if (FAILED(hr) || !IsEqualGUID(&pixel_format, &expected_format)) {
        set_hresult_error(error, error_cap, L"编码器不支持此像素格式", hr);
        goto fail;
    }
    hr = frame->lpVtbl->WritePixels(frame, height, source_stride,
                                    (UINT)packed_size, (BYTE *)source);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"写入图片像素失败", hr);
        goto fail;
    }
    hr = frame->lpVtbl->Commit(frame);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法完成导出画面", hr);
        goto fail;
    }
    hr = encoder->lpVtbl->Commit(encoder);
    if (FAILED(hr)) {
        set_hresult_error(error, error_cap, L"无法完成导出文件", hr);
        goto fail;
    }

    LUMA_RELEASE(properties);
    LUMA_RELEASE(frame);
    LUMA_RELEASE(encoder);
    LUMA_RELEASE(stream);
    LUMA_RELEASE(factory);

    if (!MoveFileExW(temp_path, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        set_error(error, error_cap, L"无法保存到目标位置，请检查文件权限");
        DeleteFileW(temp_path);
        free(packed);
        return false;
    }
    moved = true;
    free(packed);
    return moved;

fail:
    LUMA_RELEASE(properties);
    LUMA_RELEASE(frame);
    LUMA_RELEASE(encoder);
    LUMA_RELEASE(stream);
    LUMA_RELEASE(factory);
    DeleteFileW(temp_path);
    free(packed);
    return false;
}

bool luma_image_export(const LumaImage *image, int rotation, bool mirror_h,
                       bool mirror_v, LumaCrop crop, const wchar_t *path,
                       wchar_t *error, size_t error_cap) {
    UINT base_w, base_h, oriented_w, oriented_h;
    UINT x0, y0, x1, y1, out_w, out_h;
    UINT32 *out = NULL;
    uint8_t *bgra = NULL;
    UINT x, y;
    int r;
    bool ok;
    HRESULT hr;
    bool com_ready = false;

    if (image == NULL || image->pixels == NULL || image->width == 0 ||
        image->height == 0) {
        set_error(error, error_cap, L"没有可导出的图片");
        return false;
    }

    r = ((rotation % 4) + 4) % 4;
    base_w = image->width;
    base_h = image->height;
    oriented_w = (r & 1) ? base_h : base_w;
    oriented_h = (r & 1) ? base_w : base_h;

    if (crop.left < 0.0f) crop.left = 0.0f;
    if (crop.top < 0.0f) crop.top = 0.0f;
    if (crop.right > 1.0f) crop.right = 1.0f;
    if (crop.bottom > 1.0f) crop.bottom = 1.0f;
    if (crop.right <= crop.left || crop.bottom <= crop.top) {
        set_error(error, error_cap, L"裁剪范围无效");
        return false;
    }

    {
        double fw = (crop.right - crop.left) * (double)oriented_w;
        double fh = (crop.bottom - crop.top) * (double)oriented_h;
        double fcx = (crop.left + crop.right) * 0.5 * (double)oriented_w;
        double fcy = (crop.top + crop.bottom) * 0.5 * (double)oriented_h;
        UINT cw = (UINT)llround(fw);
        UINT ch = (UINT)llround(fh);
        UINT cx = (UINT)llround(fcx);
        UINT cy = (UINT)llround(fcy);

        if (cw == 0) cw = 1;
        if (ch == 0) ch = 1;
        if (cw > oriented_w) cw = oriented_w;
        if (ch > oriented_h) ch = oriented_h;
        x0 = cx > cw / 2 ? cx - cw / 2 : 0;
        y0 = cy > ch / 2 ? cy - ch / 2 : 0;
        if (x0 + cw > oriented_w) x0 = oriented_w - cw;
        if (y0 + ch > oriented_h) y0 = oriented_h - ch;
        x1 = x0 + cw;
        y1 = y0 + ch;
    }
    if (x1 <= x0 || y1 <= y0) {
        set_error(error, error_cap, L"裁剪范围太小");
        return false;
    }
    out_w = x1 - x0;
    out_h = y1 - y0;
    if ((size_t)out_w * out_h > SIZE_MAX / 4u) {
        set_error(error, error_cap, L"导出图片过大");
        return false;
    }

    out = (UINT32 *)malloc((size_t)out_w * out_h * sizeof(UINT32));
    bgra = (uint8_t *)malloc((size_t)out_w * out_h * 4u);
    if (out == NULL || bgra == NULL) {
        set_error(error, error_cap, L"内存不足，无法导出图片");
        free(out);
        free(bgra);
        return false;
    }

    for (y = 0; y < out_h; ++y) {
        for (x = 0; x < out_w; ++x) {
            UINT ox = x0 + x;
            UINT oy = y0 + y;
            UINT bx, by;
            UINT32 px;
            if (mirror_h) ox = oriented_w - 1 - ox;
            if (mirror_v) oy = oriented_h - 1 - oy;
            switch (r) {
            case 1:
                bx = oy;
                by = base_h - 1 - ox;
                break;
            case 2:
                bx = base_w - 1 - ox;
                by = base_h - 1 - oy;
                break;
            case 3:
                bx = base_w - 1 - oy;
                by = ox;
                break;
            default:
                bx = ox;
                by = oy;
                break;
            }
            px = ((const UINT32 *)image->pixels)[(size_t)by * base_w + bx];
            out[(size_t)y * out_w + x] = px;
        }
    }

    for (y = 0; y < out_h; ++y) {
        for (x = 0; x < out_w; ++x) {
            UINT32 px = out[(size_t)y * out_w + x];
            UINT a = (px >> 24) & 0xffu;
            UINT b = (px >> 0) & 0xffu;
            UINT g = (px >> 8) & 0xffu;
            UINT rr = (px >> 16) & 0xffu;
            size_t index = ((size_t)y * out_w + x) * 4u;
            if (a != 0 && a != 255) {
                b = (b * 255u) / a;
                g = (g * 255u) / a;
                rr = (rr * 255u) / a;
                if (b > 255u) b = 255u;
                if (g > 255u) g = 255u;
                if (rr > 255u) rr = 255u;
            }
            bgra[index + 0] = (uint8_t)b;
            bgra[index + 1] = (uint8_t)g;
            bgra[index + 2] = (uint8_t)rr;
            bgra[index + 3] = (uint8_t)a;
        }
    }

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        com_ready = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        set_hresult_error(error, error_cap, L"无法初始化导出组件", hr);
        free(out);
        free(bgra);
        return false;
    }

    ok = write_encoded_image(bgra, out_w, out_h, path, error, error_cap);
    if (com_ready) {
        CoUninitialize();
    }
    free(out);
    free(bgra);
    return ok;
}

/* ---------------------------------------------------------------- public API */

bool luma_image_load(const wchar_t *path, LumaImage *out, wchar_t *error,
                     size_t error_cap) {
    return load_any(path, 0, out, error, error_cap);
}

bool image_load_scaled(const wchar_t *path, UINT max_side, LumaImage *out,
                       wchar_t *error, size_t error_cap) {
    return load_any(path, max_side, out, error, error_cap);
}

/* Filters run on premultiplied BGRA: grey is a weighted sum (linear, so the
   alpha factor carries through), invert becomes a - c, which keeps edges
   clean on transparent pixels. */
void image_apply_filters(LumaImage *image, bool grayscale, bool invert) {
    UINT x, y;
    if (image == NULL || image->pixels == NULL) {
        return;
    }
    for (y = 0; y < image->height; ++y) {
        uint8_t *row = image->pixels + (size_t)y * image->stride;
        for (x = 0; x < image->width; ++x) {
            uint8_t *px = row + (size_t)x * 4u;
            if (grayscale) {
                unsigned l = (unsigned)(0.2126 * px[2] + 0.7152 * px[1] +
                                        0.0722 * px[0] + 0.5);
                if (l > 255u) l = 255u;
                px[0] = (uint8_t)l;
                px[1] = (uint8_t)l;
                px[2] = (uint8_t)l;
            }
            if (invert) {
                unsigned a = px[3];
                px[0] = (uint8_t)(a - px[0]);
                px[1] = (uint8_t)(a - px[1]);
                px[2] = (uint8_t)(a - px[2]);
            }
        }
    }
}
