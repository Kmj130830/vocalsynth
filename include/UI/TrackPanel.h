#pragma once

#include <QListWidget>
#include "Core/Project.h"

class QContextMenuEvent;
class QResizeEvent;
class QPushButton;

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
    void addTrackRequested();
    void removeTrackRequested(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    Project* m_project;
    SingerManager* m_singers;
    QPushButton* m_addButton{nullptr};
};

}