// SPDX-License-Identifier: MIT
// LibraryModel — the game grid model with artwork or generated cover tiles.

#pragma once

#include "gecko/library.hpp"

#include <QAbstractListModel>
#include <QPixmap>
#include <QHash>

namespace gecko {

class LibraryModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        PathRole,
        ExtensionRole,
        CorePathRole,
    };

    explicit LibraryModel(QObject* parent = nullptr);

    void setGames(std::vector<GameEntry> games);
    const GameEntry* gameAt(const QModelIndex& index) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QPixmap tileFor(const GameEntry& game) const;

    std::vector<GameEntry> games_;
    mutable QHash<QString, QPixmap> artworkCache_;
};

} // namespace gecko
