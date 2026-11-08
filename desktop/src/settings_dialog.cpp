// SPDX-License-Identifier: MIT
#include "settings_dialog.hpp"

#include "input_manager.hpp"
#include "input_mapper_dialog.hpp"
#include "video_filters.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace gecko {

namespace {
QLineEdit* pathEditor(QWidget* parent, QHBoxLayout** outLayout) {
    auto* editor = new QLineEdit(parent);
    auto* browse = new QPushButton(QObject::tr("…"), parent);
    browse->setFixedWidth(36);
    auto* row = new QHBoxLayout;
    row->addWidget(editor, 1);
    row->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, parent, [parent, editor] {
        const QString dir = QFileDialog::getExistingDirectory(parent, QObject::tr("Choose directory"));
        if (!dir.isEmpty()) editor->setText(dir);
    });
    *outLayout = row;
    return editor;
}
} // namespace

SettingsDialog::SettingsDialog(AppContext& context, QWidget* parent)
    : QDialog(parent), context_(context) {
    setWindowTitle(tr("Gecko — settings"));
    resize(720, 520);

    auto* outer = new QVBoxLayout(this);
    auto* rootLayout = new QHBoxLayout;
    categories_ = new QListWidget(this);
    categories_->setFixedWidth(150);
    categories_->addItem(tr("General"));
    categories_->addItem(tr("Video"));
    categories_->addItem(tr("Audio"));
    categories_->addItem(tr("Emulation"));
    categories_->addItem(tr("Input"));

    stack_ = new QStackedWidget(this);
    stack_->addWidget(buildGeneralPage());
    stack_->addWidget(buildVideoPage());
    stack_->addWidget(buildAudioPage());
    stack_->addWidget(buildEmulationPage());
    stack_->addWidget(buildInputPage());

    connect(categories_, &QListWidget::currentRowChanged, stack_, &QStackedWidget::setCurrentIndex);
    categories_->setCurrentRow(0);

    rootLayout->addWidget(categories_);
    rootLayout->addWidget(stack_, 1);
    outer->addLayout(rootLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        save();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    outer->addSpacing(8);
    outer->addWidget(buttons);
}

QWidget* SettingsDialog::buildGeneralPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    romDirs_ = new QListWidget(page);
    for (const auto& dir : context_.libraryDirectories())
        romDirs_->addItem(QString::fromStdString(dir));
    auto* romButtons = new QHBoxLayout;
    auto* addDir = new QPushButton(tr("Add…"), page);
    auto* removeDir = new QPushButton(tr("Remove"), page);
    connect(addDir, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Add ROM directory"));
        if (!dir.isEmpty() &&
            romDirs_->findItems(dir, Qt::MatchExactly).empty())
            romDirs_->addItem(dir);
    });
    connect(removeDir, &QPushButton::clicked, this, [this] {
        const auto selected = romDirs_->selectedItems();
        for (QListWidgetItem* item : selected) delete item;
    });
    romButtons->addWidget(addDir);
    romButtons->addWidget(removeDir);
    romButtons->addStretch();

    form->addRow(tr("ROM directories"), romDirs_);
    form->addRow(QString(), romButtons);

    QHBoxLayout* row = nullptr;
    coresDir_ = pathEditor(page, &row);
    coresDir_->setText(QString::fromStdString(context_.coresDirectory().string()));
    form->addRow(tr("Cores directory"), row);
    systemDir_ = pathEditor(page, &row);
    systemDir_->setText(QString::fromStdString(context_.systemDirectory().string()));
    form->addRow(tr("System directory"), row);
    savesDir_ = pathEditor(page, &row);
    savesDir_->setText(QString::fromStdString(context_.savesDirectory().string()));
    form->addRow(tr("Saves directory"), row);
    screenshotsDir_ = pathEditor(page, &row);
    screenshotsDir_->setText(QString::fromStdString(context_.screenshotsDirectory().string()));
    form->addRow(tr("Screenshots"), row);

    theme_ = new QComboBox(page);
    theme_->addItem(tr("System (native)"), "system");
    theme_->addItem(tr("Light"), "light");
    theme_->addItem(tr("Dark"), "dark");
    const QString current =
        QString::fromStdString(context_.config().getString("ui.theme", "system"));
    theme_->setCurrentIndex(qMax(0, theme_->findData(current)));
    form->addRow(tr("Theme"), theme_);

    return page;
}

QWidget* SettingsDialog::buildVideoPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    filter_ = new QComboBox(page);
    for (int i = 0; i <= 3; ++i)
        filter_->addItem(QString::fromLatin1(filterDisplayName(static_cast<VideoFilter>(i))),
                         static_cast<int>(i));
    filter_->setCurrentIndex(
        qMax(0, filter_->findData(static_cast<int>(
                    filterFromName(QString::fromStdString(
                        context_.config().getString("video.filter", "none")))))));
    form->addRow(tr("Video filter"), filter_);

    aspect_ = new QComboBox(page);
    aspect_->addItem(tr("Core aspect"), "core");
    aspect_->addItem(tr("4:3"), "4:3");
    aspect_->addItem(tr("16:9"), "16:9");
    aspect_->addItem(tr("Stretch to window"), "stretch");
    aspect_->setCurrentIndex(qMax(0, aspect_->findData(
        QString::fromStdString(context_.config().getString("video.aspect", "core")))));
    form->addRow(tr("Aspect ratio"), aspect_);

    smooth_ = new QCheckBox(tr("Smooth (bilinear) scaling"), page);
    smooth_->setChecked(context_.config().getBool("video.smooth", true));
    form->addRow(QString(), smooth_);

    hud_ = new QCheckBox(tr("Show performance HUD"), page);
    hud_->setChecked(context_.config().getBool("perf.hud", true));
    form->addRow(QString(), hud_);

    return page;
}

QWidget* SettingsDialog::buildAudioPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    audioEnabled_ = new QCheckBox(tr("Audio output enabled"), page);
    audioEnabled_->setChecked(context_.config().getBool("audio.enabled", true));
    form->addRow(QString(), audioEnabled_);

    volume_ = new QSlider(Qt::Horizontal, page);
    volume_->setRange(0, 100);
    volume_->setValue(static_cast<int>(context_.config().getInt("audio.volume", 100)));
    form->addRow(tr("Volume"), volume_);

    muteFf_ = new QCheckBox(tr("Mute audio while fast-forwarding"), page);
    muteFf_->setChecked(context_.config().getBool("audio.mute_ff", true));
    form->addRow(QString(), muteFf_);

    return page;
}

QWidget* SettingsDialog::buildEmulationPage() {
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    frameskip_ = new QSpinBox(page);
    frameskip_->setRange(0, 5);
    frameskip_->setSpecialValueText(tr("none"));
    frameskip_->setValue(static_cast<int>(context_.config().getInt("perf.frameskip", 0)));
    form->addRow(tr("Display frame skip"), frameskip_);

    ffSpeed_ = new QDoubleSpinBox(page);
    ffSpeed_->setRange(1.5, 10.0);
    ffSpeed_->setSingleStep(0.5);
    ffSpeed_->setSuffix(QStringLiteral("×"));
    ffSpeed_->setValue(context_.config().getDouble("perf.fastforward_speed", 3.0));
    form->addRow(tr("Fast-forward speed (hold Tab)"), ffSpeed_);

    return page;
}

QWidget* SettingsDialog::buildInputPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Keyboard bindings are used in the game window."), page));
    auto* openMapper = new QPushButton(tr("Open input mapping…"), page);
    // InputManager is constructed by the EmulatorView for sessions; the mapper
    // works on a temporary instance bound to the same config file.
    connect(openMapper, &QPushButton::clicked, this, [this] {
        InputManager manager(context_.config());
        InputMapperDialog dialog(manager, this);
        dialog.exec();
    });
    layout->addWidget(openMapper, 0, Qt::AlignLeft);
    layout->addStretch();
    return page;
}

void SettingsDialog::save() {
    Config& cfg = context_.config();

    QStringList dirs;
    for (int i = 0; i < romDirs_->count(); ++i)
        dirs << romDirs_->item(i)->text();
    std::vector<std::string> dirVector;
    for (const QString& dir : dirs) dirVector.push_back(dir.toStdString());
    context_.setLibraryDirectories(dirVector);

    if (!coresDir_->text().isEmpty()) context_.setCoresDirectory(coresDir_->text().toStdString());
    if (!systemDir_->text().isEmpty())
        context_.setSystemDirectory(systemDir_->text().toStdString());
    if (!savesDir_->text().isEmpty())
        context_.setSavesDirectory(savesDir_->text().toStdString());
    if (!screenshotsDir_->text().isEmpty())
        context_.setScreenshotsDirectory(screenshotsDir_->text().toStdString());

    cfg.set("ui.theme", theme_->currentData().toString().toStdString());
    cfg.set("video.filter", filterName(static_cast<VideoFilter>(filter_->currentData().toInt()))
                                .toStdString());
    cfg.set("video.aspect", aspect_->currentData().toString().toStdString());
    cfg.set("video.smooth", smooth_->isChecked());
    cfg.set("perf.hud", hud_->isChecked());
    cfg.set("audio.enabled", audioEnabled_->isChecked());
    cfg.set("audio.volume", static_cast<int64_t>(volume_->value()));
    cfg.set("audio.mute_ff", muteFf_->isChecked());
    cfg.set("perf.frameskip", static_cast<int64_t>(frameskip_->value()));
    cfg.set("perf.fastforward_speed", ffSpeed_->value());
    cfg.save();
}

} // namespace gecko
