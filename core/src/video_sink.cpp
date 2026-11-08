// SPDX-License-Identifier: MIT
#include "gecko/video_sink.hpp"

#include <cstring>

namespace gecko {

bool convertToXrgb8888(const void* src, retro_pixel_format srcFormat, size_t srcStrideBytes,
                        unsigned width, unsigned height, uint32_t* dst) {
    if (!src || !dst || width == 0 || height == 0) return false;

    switch (srcFormat) {
        case RETRO_PIXEL_FORMAT_0RGB1555: {
            const uint8_t* base = static_cast<const uint8_t*>(src);
            for (unsigned y = 0; y < height; ++y) {
                const uint16_t* row = reinterpret_cast<const uint16_t*>(base + y * srcStrideBytes);
                uint32_t* out = dst + static_cast<size_t>(y) * width;
                for (unsigned x = 0; x < width; ++x) {
                    const uint16_t p = row[x];
                    const uint32_t r = (p >> 10) & 0x1F;
                    const uint32_t g = (p >> 5) & 0x1F;
                    const uint32_t b = p & 0x1F;
                    // 5-bit -> 8-bit bit replication.
                    out[x] = ((r << 3) | (r >> 2)) << 16 | ((g << 3) | (g >> 2)) << 8 | ((b << 3) | (b >> 2));
                }
            }
            return true;
        }
        case RETRO_PIXEL_FORMAT_RGB565: {
            const uint8_t* base = static_cast<const uint8_t*>(src);
            for (unsigned y = 0; y < height; ++y) {
                const uint16_t* row = reinterpret_cast<const uint16_t*>(base + y * srcStrideBytes);
                uint32_t* out = dst + static_cast<size_t>(y) * width;
                for (unsigned x = 0; x < width; ++x) {
                    const uint16_t p = row[x];
                    const uint32_t r = (p >> 11) & 0x1F;
                    const uint32_t g = (p >> 5) & 0x3F;
                    const uint32_t b = p & 0x1F;
                    out[x] = ((r << 3) | (r >> 2)) << 16 | ((g << 2) | (g >> 4)) << 8 | ((b << 3) | (b >> 2));
                }
            }
            return true;
        }
        case RETRO_PIXEL_FORMAT_XRGB8888: {
            const uint8_t* base = static_cast<const uint8_t*>(src);
            for (unsigned y = 0; y < height; ++y) {
                const uint32_t* row = reinterpret_cast<const uint32_t*>(base + y * srcStrideBytes);
                std::memcpy(dst + static_cast<size_t>(y) * width, row, static_cast<size_t>(width) * 4);
            }
            return true;
        }
        default:
            return false;
    }
}

} // namespace gecko
