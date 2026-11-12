#include "../src/image.h"
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, wchar_t **argv) {
    LumaImage image;
    wchar_t error[512];
    LumaCrop crop;
    int rotation;
    int mask;
    bool ok;
    if (argc < 6) {
        wprintf(L"usage: export_probe in out rotation mask l t r b\n");
        return 2;
    }
    memset(&image, 0, sizeof(image));
    error[0] = 0;
    if (!luma_image_load(argv[1], &image, error, 512)) {
        wprintf(L"load failed: %s\n", error);
        return 1;
    }
    rotation = _wtoi(argv[3]);
    mask = _wtoi(argv[4]);
    crop.left = (float)_wtof(argv[5]);
    crop.top = (float)_wtof(argv[6]);
    crop.right = (float)_wtof(argv[7]);
    crop.bottom = (float)_wtof(argv[8]);
    ok = luma_image_export(&image, rotation, (mask & 1) != 0,
                           (mask & 2) != 0, crop, argv[2], error, 512);
    wprintf(L"%s: %ux%u -> %s\n", ok ? L"OK" : L"FAIL", image.width,
            image.height, ok ? argv[2] : error);
    luma_image_free(&image);
    return ok ? 0 : 1;
}
