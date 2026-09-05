#include "UI/PreferencesDialog.h"
#include "Singer/SingerManager.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace myvocal {

namespace {
const QStringList kPhonemizers = {
    QStringLiteral("Default CV"), QStringLiteral("Japanese VCV"),
    QStringLiteral("Japanese CVVC"), QStringLiteral("Hybrid Japanese"),
    QStringLiteral("Korean VCV"), QStringLiteral("Korean CBNN")};
}

PreferencesDialog::PreferencesDialog(QString* resampler,
                                     SingerManager* singers,
                                     QWidget* parent)
    : QDialog(parent), m_resampler(resampler)
{
    setWindowTitle(QStringLiteral("Preferences"));
    setMinimumWidth(600);

    const QSettings settings;
    auto* form = new QFormLayout;

    m_resamplerEdit = new QLineEdit(settings.value("renderer/moresampler").toString(), this);
    if (m_resampler && !m_resampler->isEmpty()) m_resamplerEdit->setText(*m_resampler);
    auto* resamplerBrowse = new QPushButton(QStringLiteral("Browse..."), this);
    auto* resamplerRow = new QWidget(this);
    auto* resamplerLayout = new QHBoxLayout(resamplerRow);
    resamplerLayout->setContentsMargins(0, 0, 0, 0);
    resamplerLayout->addWidget(m_resamplerEdit, 1);
    resamplerLayout->addWidget(resamplerBrowse);
    form->addRow(QStringLiteral("Moresampler executable"), resamplerRow);

    m_voiceBanksEdit = new QLineEdit(settings.value("voicebanks/path").toString(), this);
    auto* voiceBrowse = new QPushButton(QStringLiteral("Browse..."), this);
    auto* voiceRow = new QWidget(this);
    auto* voiceLayout = new QHBoxLayout(voiceRow);
    voiceLayout->setContentsMargins(0, 0, 0, 0);
    voiceLayout->addWidget(m_voiceBanksEdit, 1);
    voiceLayout->addWidget(voiceBrowse);
    form->addRow(QStringLiteral("VoiceBanks path"), voiceRow);

    m_singerCombo = new QComboBox(this);
    m_singerCombo->setEditable(true);
    if (singers) {
        for (const auto& singer : singers->singers()) {
            m_singerCombo->addItem(singer->info().name,
                                   QString::fromStdWString(singer->path().wstring()));
        }
    }
    const QString defaultSinger = settings.value("defaults/singer").toString();
    const int singerIndex = m_singerCombo->findText(defaultSinger);
    if (singerIndex >= 0) m_singerCombo->setCurrentIndex(singerIndex);
    else m_singerCombo->setCurrentText(defaultSinger);
    form->addRow(QStringLiteral("Default Singer"), m_singerCombo);

    m_phonemizerCombo = new QComboBox(this);
    m_phonemizerCombo->addItems(kPhonemizers);
    const int phonIndex = m_phonemizerCombo->findText(settings.value("defaults/phonemizer", "Default CV").toString());
    m_phonemizerCombo->setCurrentIndex(phonIndex < 0 ? 0 : phonIndex);
    form->addRow(QStringLiteral("Default Phonemizer"), m_phonemizerCombo);

    m_bpmSpin = new QDoubleSpinBox(this);
    m_bpmSpin->setRange(20.0, 999.0);
    m_bpmSpin->setDecimals(2);
    m_bpmSpin->setValue(settings.value("defaults/bpm", 120.0).toDouble());
    form->addRow(QStringLiteral("Default BPM"), m_bpmSpin);

    m_snapSpin = new QSpinBox(this);
    m_snapSpin->setRange(1, 3840);
    m_snapSpin->setValue(settings.value("defaults/snap", 60).toInt());
    form->addRow(QStringLiteral("Default Snap (ticks)"), m_snapSpin);

    m_gridSpin = new QSpinBox(this);
    m_gridSpin->setRange(1, 3840);
    m_gridSpin->setValue(settings.value("defaults/grid", 120).toInt());
    form->addRow(QStringLiteral("Default Grid (ticks)"), m_gridSpin);

    auto* stopWidget = new QWidget(this);
    auto* stopLayout = new QVBoxLayout(stopWidget);
    stopLayout->setContentsMargins(0, 0, 0, 0);
    auto* returnStart = new QRadioButton(QStringLiteral("Return to playback start"), stopWidget);
    auto* stayCurrent = new QRadioButton(QStringLiteral("Stop at current position"), stopWidget);
    if (settings.value("playback/stopBehavior", 0).toInt() == 0) returnStart->setChecked(true);
    else stayCurrent->setChecked(true);
    stopLayout->addWidget(returnStart);
    stopLayout->addWidget(stayCurrent);
    form->addRow(QStringLiteral("Playback Stop Behavior"), stopWidget);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(resamplerBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select Moresampler"), {}, QStringLiteral("Executable (*.exe);;All Files (*)"));
        if (!path.isEmpty()) m_resamplerEdit->setText(path);
    });
    connect(voiceBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Select VoiceBanks directory"));
        if (!path.isEmpty()) m_voiceBanksEdit->setText(path);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this, returnStart] {
        QSettings settings;
        settings.setValue("renderer/moresampler", m_resamplerEdit->text());
        settings.setValue("voicebanks/path", m_voiceBanksEdit->text());
        settings.setValue("defaults/singer", m_singerCombo->currentText());
        settings.setValue("defaults/phonemizer", m_phonemizerCombo->currentText());
        settings.setValue("defaults/bpm", m_bpmSpin->value());
        settings.setValue("defaults/snap", m_snapSpin->value());
        settings.setValue("defaults/grid", m_gridSpin->value());
        settings.setValue("playback/stopBehavior", returnStart->isChecked() ? 0 : 1);
        if (m_resampler) *m_resampler = m_resamplerEdit->text();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString PreferencesDialog::voiceBanksPath() const { return m_voiceBanksEdit ? m_voiceBanksEdit->text() : QString(); }
QString PreferencesDialog::defaultSinger() const { return m_singerCombo ? m_singerCombo->currentText() : QString(); }
QString PreferencesDialog::defaultPhonemizer() const { return m_phonemizerCombo ? m_phonemizerCombo->currentText() : QStringLiteral("Default CV"); }
double PreferencesDialog::defaultBpm() const { return m_bpmSpin ? m_bpmSpin->value() : 120.0; }
int PreferencesDialog::defaultSnap() const { return m_snapSpin ? m_snapSpin->value() : 60; }
int PreferencesDialog::defaultGrid() const { return m_gridSpin ? m_gridSpin->value() : 120; }

}