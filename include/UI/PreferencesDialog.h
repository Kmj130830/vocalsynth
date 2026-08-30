#pragma once

#include <QDialog>
#include <QString>

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
    double defaultBpm() const;
    int defaultSnap() const;
    int defaultGrid() const;

private:
    QString* m_resampler;
    class QLineEdit* m_resamplerEdit{nullptr};
    class QLineEdit* m_voiceBanksEdit{nullptr};
    class QComboBox* m_singerCombo{nullptr};
    class QComboBox* m_phonemizerCombo{nullptr};
    class QDoubleSpinBox* m_bpmSpin{nullptr};
    class QSpinBox* m_snapSpin{nullptr};
    class QSpinBox* m_gridSpin{nullptr};
};

}
