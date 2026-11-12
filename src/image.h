#ifndef LUMA_IMAGE_H
#define LUMA_IMAGE_H

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float left;
    float top;
    float right;
    float bottom;
} LumaCrop;

/* Decoded image owned by the caller. Pixels are premultiplied BGRA. */
typedef struct {
    uint8_t *pixels;
    UINT width;
    UINT height;
    UINT stride;
} LumaImage;

bool luma_image_load(const wchar_t *path, LumaImage *out,
                     wchar_t *error, size_t error_cap);
void luma_image_free(LumaImage *image);

/* Rotation is clockwise quarter turns (0..3). mirror_h/mirror_v are applied
   after rotation. Crop is normalized in the final rotated and mirrored
   coordinate space. The file extension selects PNG/JPEG/BMP/TIFF. */
bool luma_image_export(const LumaImage *image, int rotation, bool mirror_h,
                       bool mirror_v, LumaCrop crop, const wchar_t *path,
                       wchar_t *error, size_t error_cap);

#endif
