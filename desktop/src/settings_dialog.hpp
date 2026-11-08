// SPDX-License-Identifier: MIT
// SettingsDialog — directories, video, audio, emulation and input settings.

#pragma once

#include "app_context.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace gecko {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(AppContext& context, QWidget* parent = nullptr);

private slots:
    void save();

private:
    QWidget* buildGeneralPage();
    QWidget* buildVideoPage();
    QWidget* buildAudioPage();
    QWidget* buildEmulationPage();
    QWidget* buildInputPage();

    AppContext& context_;
    QStackedWidget* stack_ = nullptr;
    QListWidget* categories_ = nullptr;

    // general
    QListWidget* romDirs_ = nullptr;
    QLineEdit* coresDir_ = nullptr;
    QLineEdit* systemDir_ = nullptr;
    QLineEdit* savesDir_ = nullptr;
    QLineEdit* screenshotsDir_ = nullptr;
    QComboBox* theme_ = nullptr;

    // video
    QComboBox* filter_ = nullptr;
    QComboBox* aspect_ = nullptr;
    QCheckBox* smooth_ = nullptr;
    QCheckBox* hud_ = nullptr;

    // audio
    QCheckBox* audioEnabled_ = nullptr;
    QSlider* volume_ = nullptr;
    QCheckBox* muteFf_ = nullptr;

    // emulation
    QSpinBox* frameskip_ = nullptr;
    QDoubleSpinBox* ffSpeed_ = nullptr;
};

} // namespace gecko
