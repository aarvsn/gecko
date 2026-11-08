// SPDX-License-Identifier: MIT
#include "library_model.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace gecko {

namespace {
// Stable pastel colors derived from the title, for generated tiles.
QColor tileColor(const QString& title) {
    if (title.isEmpty()) return QColor(90, 130, 95);
    quint32 h = 2166136261u;
    for (QChar c : title) {
        h ^= c.unicode();
        h *= 16777619u;
    }
    const int hue = static_cast<int>(h % 360);
    return QColor::fromHsl(hue, 120, 118);
}
} // namespace

LibraryModel::LibraryModel(QObject* parent) : QAbstractListModel(parent) {}

void LibraryModel::setGames(std::vector<GameEntry> games) {
    beginResetModel();
    games_ = std::move(games);
    endResetModel();
}

const GameEntry* LibraryModel::gameAt(const QModelIndex& index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(games_.size()))
        return nullptr;
    return &games_[static_cast<size_t>(index.row())];
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(games_.size());
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
    const GameEntry* game = gameAt(index);
    if (!game) return {};

    switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return QString::fromStdString(game->title);
        case Qt::DecorationRole:
            return tileFor(*game);
        case Qt::ToolTipRole: {
            QString tip = QString::fromStdString(game->path.string());
            if (!game->corePath.empty())
                tip += QStringLiteral("\n%1: %2")
                           .arg(tr("Core"))
                           .arg(QString::fromStdString(game->corePath.filename().string()));
            else
                tip += QStringLiteral("\n%1").arg(tr("No core assigned — pick one when launching"));
            return tip;
        }
        case PathRole:
            return QString::fromStdString(game->path.string());
        case ExtensionRole:
            return QString::fromStdString(game->extension);
        case CorePathRole:
            return QString::fromStdString(game->corePath.string());
        default:
            return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[TitleRole] = "title";
    roles[PathRole] = "path";
    roles[ExtensionRole] = "extension";
    roles[CorePathRole] = "corePath";
    return roles;
}

QPixmap LibraryModel::tileFor(const GameEntry& game) const {
    const QString key = QString::fromStdString(game.path.string());
    auto it = artworkCache_.find(key);
    if (it != artworkCache_.end()) return it.value();

    const QSize size(200, 150);
    QPixmap pixmap;

    if (!game.artworkPath.empty()) {
        QImage image(QString::fromStdString(game.artworkPath.string()));
        if (!image.isNull()) {
            pixmap = QPixmap::fromImage(
                image.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        }
    }

    if (pixmap.isNull()) {
        // Generated cover tile: gradient + platform tag + initials.
        const QString title = QString::fromStdString(game.title);
        QColor base = tileColor(title);
        pixmap = QPixmap(size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        QPainterPath rounded;
        rounded.addRoundedRect(QRectF(0, 0, size.width(), size.height()), 10, 10);
        painter.setClipPath(rounded);

        QLinearGradient gradient(0, 0, 0, size.height());
        gradient.setColorAt(0.0, base.lighter(125));
        gradient.setColorAt(1.0, base.darker(140));
        painter.fillRect(0, 0, size.width(), size.height(), gradient);

        // Initials.
        QString initials;
        const QStringList words = title.split(' ', Qt::SkipEmptyParts);
        for (const QString& word : words.mid(0, 2))
            if (!word.isEmpty()) initials += word.at(0).toUpper();
        if (initials.isEmpty()) initials = QStringLiteral("?");
        QFont font = painter.font();
        font.setPixelSize(64);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 230));
        painter.drawText(QRect(0, 10, size.width(), size.height() - 40), Qt::AlignCenter,
                         initials);

        // Extension badge.
        painter.setFont(QFont(painter.font().family(), 9, QFont::Medium));
        painter.setPen(QColor(255, 255, 255, 190));
        painter.drawText(QRect(0, size.height() - 34, size.width(), 24), Qt::AlignCenter,
                         QString::fromStdString(game.extension).toUpper());

        painter.end();
    }

    artworkCache_.insert(key, pixmap);
    return pixmap;
}

} // namespace gecko
