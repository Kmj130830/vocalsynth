#pragma once

#include <QToolBar>

#include "Editor/PianoRollEditor.h"

class QComboBox;

namespace myvocal {

class MainToolBar final : public QToolBar {
    Q_OBJECT
public:
    explicit MainToolBar(QWidget* parent = nullptr);

signals:
    void toolChanged(EditTool tool);
    void snapToggled(bool enabled);
    void gridToggled(bool enabled);
    void gridResolutionChanged(int denominator);

private:
    QActionGroup* m_group{nullptr};
    QComboBox* m_gridCombo{nullptr};
};

}