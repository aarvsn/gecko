// SPDX-License-Identifier: MIT
#include "video_filters.hpp"

#include <QPainterPath>

namespace gecko {

void applyFilter(QPainter* painter, const QRectF& rect, VideoFilter filter, int coreHeight,
                 bool smoothing) {
    if (filter == VideoFilter::None || rect.width() <= 0 || rect.height() <= 0) return;

    painter->save();
    painter->setClipping(false);

    switch (filter) {
        case VideoFilter::Scanlines: {
            // One dark line per source row.
            const int lines = coreHeight > 0 ? coreHeight : 240;
            const double spacing = rect.height() / lines;
            if (spacing < 1.5) break; // not enough room; skip
            QColor line(0, 0, 0, 70);
            painter->setPen(Qt::NoPen);
            painter->setBrush(line);
            double y = rect.top();
            for (int i = 0; i < lines; ++i, y += spacing) {
                painter->drawRect(QRectF(rect.left(), y, rect.width(), spacing * 0.5));
            }
            break;
        }
        case VideoFilter::Crt: {
            // Scanlines + aperture-grille hint + vignette.
            const int lines = coreHeight > 0 ? coreHeight : 240;
            const double spacing = rect.height() / lines;
            if (spacing >= 1.5) {
                QColor line(0, 0, 0, 80);
                painter->setPen(Qt::NoPen);
                painter->setBrush(line);
                double y = rect.top();
                for (int i = 0; i < lines; ++i, y += spacing)
                    painter->drawRect(QRectF(rect.left(), y, rect.width(), spacing * 0.45));
            }
            // RGB stripe mask.
            const double stripe = qMax(3.0, rect.width() / 640.0);
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
            const QColor masks[3] = {QColor(255, 90, 90), QColor(90, 255, 90), QColor(110, 110, 255)};
            double x = rect.left();
            int i = 0;
            while (x < rect.right()) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(masks[i % 3]);
                painter->drawRect(QRectF(x, rect.top(), stripe, rect.height()));
                x += stripe;
                ++i;
            }
            painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
            // Vignette.
            QRadialGradient vignette(rect.center(), qMax(rect.width(), rect.height()) * 0.75);
            vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
            vignette.setColorAt(0.75, QColor(0, 0, 0, 0));
            vignette.setColorAt(1.0, QColor(0, 0, 0, 90));
            painter->setPen(Qt::NoPen);
            painter->setBrush(vignette);
            painter->drawRect(rect);
            break;
        }
        case VideoFilter::Lcd: {
            // Grid gaps like a handheld LCD panel.
            const int rows = coreHeight > 0 ? coreHeight : 240;
            const double cellY = rect.height() / rows;
            const int cols = qMax(1, static_cast<int>(rect.width() / (cellY * rect.width() / rect.width())));
            const double cellX = rect.width() / qMax(cols, static_cast<int>(rows * 4 / 3));
            if (cellY < 1.5 || cellX < 1.5) break;
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 50));
            double y = rect.top();
            while (y < rect.bottom()) {
                painter->drawRect(QRectF(rect.left(), y, rect.width(), 1.0));
                y += cellY;
            }
            double x = rect.left();
            while (x < rect.right()) {
                painter->drawRect(QRectF(x, rect.top(), 1.0, rect.height()));
                x += cellX;
            }
            // Subtle glass reflection.
            QLinearGradient glass(rect.topLeft(), rect.bottomRight());
            glass.setColorAt(0.0, QColor(255, 255, 255, 18));
            glass.setColorAt(0.35, QColor(255, 255, 255, 0));
            painter->setBrush(glass);
            painter->drawRect(rect);
            break;
        }
        case VideoFilter::None:
            break;
    }

    painter->restore();
    (void)smoothing;
}

} // namespace gecko
