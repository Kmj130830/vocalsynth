#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace myvocal {

class SingerManager;

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QString* resampler,
                                SingerManager* singers = nullptr,
                                QWidget* parent = nullptr);

    QString voiceBanksPath() const;
    QString defaultSinger() const;
    QString defaultPhonemizer() const;
    int defaultSnap() const;
    int defaultGrid() const;

private:
    QString* m_resampler{nullptr};
    QLineEdit* m_resamplerEdit{nullptr};
    QLineEdit* m_voiceBanksEdit{nullptr};
    QComboBox* m_singerCombo{nullptr};
    QComboBox* m_phonemizerCombo{nullptr};
    QSpinBox* m_snapSpin{nullptr};
    QSpinBox* m_gridSpin{nullptr};
};

}