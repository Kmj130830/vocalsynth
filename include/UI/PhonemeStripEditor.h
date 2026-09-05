#pragma once

#include <QAbstractScrollArea>

#include "Core/Project.h"

class QMouseEvent;

namespace myvocal {

class PhonemeStripEditor final : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit PhonemeStripEditor(Project* project, QWidget* parent = nullptr);
    void setActiveTrack(int index);
    void setPlayheadMs(qint64 ms);
    void setPixelsPerSecond(double pixels);

signals:
    void positionClicked(qint64 ms);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    qint64 msAtX(int x) const;
    void updateScrollRanges();

    Project* m_project{nullptr};
    int m_activeTrack{0};
    qint64 m_playheadMs{0};
    double m_pixelsPerSecond{90.0};
    bool m_draggingPlayhead{false};
};

}