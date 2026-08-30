#pragma once
#include <QToolBar>
#include "Editor/PianoRollEditor.h"
namespace myvocal { class MainToolBar : public QToolBar { Q_OBJECT public: explicit MainToolBar(QWidget* parent=nullptr); signals: void toolChanged(EditTool); void snapToggled(bool); void gridToggled(bool); private: QActionGroup* m_group; }; }
