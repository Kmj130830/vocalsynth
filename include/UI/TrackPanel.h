#pragma once
#include <QListWidget>
#include "Core/Project.h"
namespace myvocal { class TrackPanel : public QListWidget { Q_OBJECT public: explicit TrackPanel(Project*,QWidget*parent=nullptr); void refresh(); signals: void trackSelected(int); private: Project*m_project; }; }
