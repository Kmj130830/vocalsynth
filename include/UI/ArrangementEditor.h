#pragma once

#include <QAbstractScrollArea>

#include "Core/Project.h"

namespace myvocal {

class ArrangementEditor final : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit ArrangementEditor(Project* project, QWidget* parent = nullptr);

    void setProject(Project* project);
    void setPlayheadMs(qint64 ms);
    qint64 playheadMs() const noexcept;
    void setTrackHeight(int pixels);
    void setPixelsPerSecond(double pixels);

signals:
    void positionClicked(qint64 ms);
    void documentChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    qint64 msAtX(int x) const;
    int yForTrack(int trackIndex) const;
    void updateScrollRanges();

    Project* m_project{nullptr};
    qint64 m_playheadMs{0};
    int m_trackHeight{48};
    double m_pixelsPerSecond{90.0};
    bool m_draggingPlayhead{false};
};

}