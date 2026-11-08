// SPDX-License-Identifier: MIT
// VideoFilters — software post-processing applied over the scaled frame in
// the emulator view: scanlines, a CRT-ish mask and an LCD grid. Pure QPainter,
// no shaders, so behavior is identical on all platforms.

#pragma once

#include <QString>

#include <QPainter>

namespace gecko {

enum class VideoFilter {
    None = 0,
    Scanlines,
    Crt,
    Lcd,
};

inline VideoFilter filterFromName(const QString& name) {
    if (name == QStringLiteral("scanlines")) return VideoFilter::Scanlines;
    if (name == QStringLiteral("crt")) return VideoFilter::Crt;
    if (name == QStringLiteral("lcd")) return VideoFilter::Lcd;
    return VideoFilter::None;
}

inline QString filterName(VideoFilter filter) {
    switch (filter) {
        case VideoFilter::Scanlines: return QStringLiteral("scanlines");
        case VideoFilter::Crt: return QStringLiteral("crt");
        case VideoFilter::Lcd: return QStringLiteral("lcd");
        case VideoFilter::None: break;
    }
    return QStringLiteral("none");
}

inline const char* filterDisplayName(VideoFilter filter) {
    switch (filter) {
        case VideoFilter::None: return "None";
        case VideoFilter::Scanlines: return "Scanlines";
        case VideoFilter::Crt: return "CRT";
        case VideoFilter::Lcd: return "LCD Grid";
    }
    return "None";
}

// Applies `filter` over the region `rect` of `painter`. Called after the
// frame has been drawn. `coreHeight` is the source height so line spacing
// can follow the integer scale factor.
void applyFilter(QPainter* painter, const QRectF& rect, VideoFilter filter, int coreHeight,
                 bool smoothing);

} // namespace gecko
