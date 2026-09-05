#pragma once

#include <QAbstractScrollArea>

#include "Core/Project.h"

class QDoubleSpinBox;
class QMouseEvent;
class QResizeEvent;

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
    void trackClicked(int index);
    void documentChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    qint64 msAtX(int x) const;
    int trackAtY(int y) const;
    void updateScrollRanges();
    void updateHeaderGeometry();

    Project* m_project{nullptr};
    qint64 m_playheadMs{0};
    int m_trackHeight{56};
    double m_pixelsPerSecond{90.0};
    bool m_draggingPlayhead{false};
    int m_draggingAudioIndex{-1};
    qint64 m_audioDragOffsetMs{0};
    QDoubleSpinBox* m_bpmSpin{nullptr};
};

}