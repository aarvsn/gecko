// SPDX-License-Identifier: MIT
// InputMapperDialog — click a joypad button row, press a key: persisted to
// the config through InputManager.

#pragma once

#include <QDialog>

class QTableWidget;

namespace gecko {

class InputManager;

class InputMapperDialog : public QDialog {
    Q_OBJECT

public:
    explicit InputMapperDialog(InputManager& input, QWidget* parent = nullptr);

protected:
    bool event(QEvent* event) override;

private slots:
    void captureKey();
    void resetToDefaults();

private:
    void repopulate();

    InputManager& input_;
    QTableWidget* table_ = nullptr;
};

} // namespace gecko
