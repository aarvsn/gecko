// SPDX-License-Identifier: MIT
// CoreManagerDialog — lists discovered libretro cores and lets the user map
// ROM extensions to cores. Also provides the "pick a core" flow used when a
// game has no core assigned yet.

#pragma once

#include "app_context.hpp"

#include <QDialog>
#include <QTableWidget>

namespace gecko {

class CoreManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit CoreManagerDialog(AppContext& context, QWidget* parent = nullptr);

    // Static helper: shows a core chooser for `romPath`; returns true and
    // fills `coreOut` when the user picked one (and opted to remember it).
    static bool pickCore(QWidget* parent, AppContext& context,
                         const std::filesystem::path& romPath,
                         std::filesystem::path* coreOut);

private slots:
    void refresh();
    void mapExtensionToCore();

private:
    void repopulate();

    AppContext& context_;
    QTableWidget* table_ = nullptr;
    QTableWidget* mapTable_ = nullptr;
    std::vector<CoreInfo> cores_;
};

} // namespace gecko
