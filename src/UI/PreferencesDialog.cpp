#include "UI/PreferencesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace myvocal {

namespace {
const QStringList kPhonemizers = {
    QStringLiteral("Default CV"), QStringLiteral("Japanese VCV"),
    QStringLiteral("Japanese CVVC"), QStringLiteral("Hybrid Japanese"),
    QStringLiteral("Korean VCV"), QStringLiteral("Korean CBNN")
};
}

PreferencesDialog::PreferencesDialog(QString* resampler, QWidget* parent)
    : QDialog(parent), m_resampler(resampler)
{
    setWindowTitle(QStringLiteral("Preferences"));
    setMinimumWidth(560);

    QSettings settings;
    auto* form = new QFormLayout;

    m_resamplerEdit = new QLineEdit(
        settings.value("renderer/moresampler").toString(), this);
    if (m_resampler && !m_resampler->isEmpty()) {
        m_resamplerEdit->setText(*m_resampler);
    }
    auto* resamplerBrowse = new QPushButton(QStringLiteral("Browse..."), this);
    auto* resamplerRow = new QWidget(this);
    auto* resamplerLayout = new QHBoxLayout(resamplerRow);
    resamplerLayout->setContentsMargins(0, 0, 0, 0);
    resamplerLayout->addWidget(m_resamplerEdit, 1);
    resamplerLayout->addWidget(resamplerBrowse);
    form->addRow(QStringLiteral("Moresampler executable"), resamplerRow);

    m_voiceBanksEdit = new QLineEdit(
        settings.value("voicebanks/path").toString(), this);
    auto* voiceBrowse = new QPushButton(QStringLiteral("Browse..."), this);
    auto* voiceRow = new QWidget(this);
    auto* voiceLayout = new QHBoxLayout(voiceRow);
    voiceLayout->setContentsMargins(0, 0, 0, 0);
    voiceLayout->addWidget(m_voiceBanksEdit, 1);
    voiceLayout->addWidget(voiceBrowse);
    form->addRow(QStringLiteral("VoiceBanks path"), voiceRow);

    m_singerCombo = new QComboBox(this);
    m_singerCombo->setEditable(true);
    m_singerCombo->setCurrentText(settings.value("defaults/singer").toString());
    form->addRow(QStringLiteral("Default Singer"), m_singerCombo);

    m_phonemizerCombo = new QComboBox(this);
    m_phonemizerCombo->addItems(kPhonemizers);
    const QString defaultPhonemizer =
        settings.value("defaults/phonemizer", "Default CV").toString();
    const int phonIndex = m_phonemizerCombo->findText(defaultPhonemizer);
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

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(resamplerBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Select Moresampler"), {},
            QStringLiteral("Executable (*.exe);;All Files (*)"));
        if (!path.isEmpty()) {
            m_resamplerEdit->setText(path);
        }
    });

    connect(voiceBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Select VoiceBanks directory"));
        if (!path.isEmpty()) {
            m_voiceBanksEdit->setText(path);
        }
    });

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QSettings s;
        s.setValue("renderer/moresampler", m_resamplerEdit->text());
        s.setValue("voicebanks/path", m_voiceBanksEdit->text());
        s.setValue("defaults/singer", m_singerCombo->currentText());
        s.setValue("defaults/phonemizer", m_phonemizerCombo->currentText());
        s.setValue("defaults/bpm", m_bpmSpin->value());
        s.setValue("defaults/snap", m_snapSpin->value());
        s.setValue("defaults/grid", m_gridSpin->value());
        if (m_resampler) {
            *m_resampler = m_resamplerEdit->text();
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString PreferencesDialog::voiceBanksPath() const
{
    return m_voiceBanksEdit ? m_voiceBanksEdit->text() : QString();
}

QString PreferencesDialog::defaultSinger() const
{
    return m_singerCombo ? m_singerCombo->currentText() : QString();
}

QString PreferencesDialog::defaultPhonemizer() const
{
    return m_phonemizerCombo ? m_phonemizerCombo->currentText()
                             : QStringLiteral("Default CV");
}

double PreferencesDialog::defaultBpm() const
{
    return m_bpmSpin ? m_bpmSpin->value() : 120.0;
}

int PreferencesDialog::defaultSnap() const
{
    return m_snapSpin ? m_snapSpin->value() : 60;
}

int PreferencesDialog::defaultGrid() const
{
    return m_gridSpin ? m_gridSpin->value() : 120;
}

}
