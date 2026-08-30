#pragma once

#include <QListWidget>
#include "Core/Project.h"

namespace myvocal {

class SingerManager;

class TrackPanel : public QListWidget {
    Q_OBJECT
public:
    explicit TrackPanel(Project* project, SingerManager* singers, QWidget* parent = nullptr);

    void setSingerManager(SingerManager* singers);
    void refresh();

signals:
    void trackSelected(int index);
    void trackSettingsChanged(int index);

private:
    Project* m_project;
    SingerManager* m_singers;
};

}
