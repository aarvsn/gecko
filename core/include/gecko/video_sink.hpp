// SPDX-License-Identifier: MIT
// Video output abstraction. The emulator converts every incoming frame to
// 32-bit XRGB8888 (one uint32_t per pixel, 0x00RRGGBB, row-major) before
// handing it to the sink, so UI layers only deal with one format.

#pragma once

#include "libretro.h"

#include <cstddef>
#include <cstdint>

namespace gecko {

struct VideoFrame {
    unsigned width = 0;
    unsigned height = 0;
    // Row stride in *bytes* (width * 4 for XRGB8888).
    size_t stride = 0;
    // Always RETRO_PIXEL_FORMAT_XRGB8888 after emulator-side conversion.
    retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
    // width * height uint32_t values, valid until the sink returns.
    const uint32_t* pixels = nullptr;
};

class VideoSink {
public:
    virtual ~VideoSink() = default;

    // Called from the emulation thread for each frame produced by the core.
    // `frame.pixels` stays valid only for the duration of the call.
    virtual void frame(const VideoFrame& frame) = 0;

    // The core asked to re-use the previous frame (retro_video_refresh_t with
    // a NULL data pointer).
    virtual void duplicateFrame() {}
};

// Converts one row of pixels in `srcFormat` to XRGB8888. `src` must hold
// width pixels with the stride the core reported. Returns false when the
// format is unknown (dst is left untouched).
bool convertToXrgb8888(const void* src, retro_pixel_format srcFormat, size_t srcStrideBytes,
                        unsigned width, unsigned height, uint32_t* dst);

} // namespace gecko
