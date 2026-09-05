#include "UI/ArrangementEditor.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>

namespace myvocal {

ArrangementEditor::ArrangementEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setMinimumHeight(170);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    updateScrollRanges();
}

void ArrangementEditor::setProject(Project* project)
{
    m_project = project;
    updateScrollRanges();
    viewport()->update();
}

void ArrangementEditor::setPlayheadMs(qint64 ms)
{
    m_playheadMs = std::max<qint64>(0, ms);
    const int x = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond);
    const int left = horizontalScrollBar()->value();
    const int view = viewport()->width();
    if (x < left) horizontalScrollBar()->setValue(std::max(0, x - view / 5));
    else if (x > left + view * 4 / 5) horizontalScrollBar()->setValue(std::max(0, x - view / 5));
    viewport()->update();
}

qint64 ArrangementEditor::playheadMs() const noexcept { return m_playheadMs; }

void ArrangementEditor::setTrackHeight(int pixels)
{
    m_trackHeight = std::clamp(pixels, 28, 100);
    updateScrollRanges();
    viewport()->update();
}

void ArrangementEditor::setPixelsPerSecond(double pixels)
{
    m_pixelsPerSecond = std::clamp(pixels, 20.0, 500.0);
    updateScrollRanges();
    viewport()->update();
}

qint64 ArrangementEditor::msAtX(int x) const
{
    const double sceneX = x + horizontalScrollBar()->value();
    return std::max<qint64>(0, qRound64(sceneX / m_pixelsPerSecond * 1000.0));
}

int ArrangementEditor::yForTrack(int trackIndex) const
{
    return trackIndex * m_trackHeight - verticalScrollBar()->value();
}

void ArrangementEditor::updateScrollRanges()
{
    const int trackCount = m_project
        ? std::max(1, static_cast<int>(m_project->tracks().size()))
        : 1;
    const int rows = trackCount + 1;
    verticalScrollBar()->setRange(0, std::max(0, rows * m_trackHeight - viewport()->height()));

    qint64 maxMs = 60000;
    if (m_project) {
        for (const auto& track : m_project->tracks()) {
            for (const auto& note : track.notes()) {
                maxMs = std::max(maxMs,
                    qRound64(m_project->tempoMap().tickToSeconds(
                        note.getEndTick(), m_project->ppq()) * 1000.0));
            }
        }
        for (const auto& clip : m_project->audioClips()) {
            const qint64 length = clip.durationMs > 0 ? clip.durationMs : 1000;
            maxMs = std::max(maxMs, clip.startMs + length);
        }
    }
    const int contentWidth = qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 500;
    horizontalScrollBar()->setRange(0, std::max(0, contentWidth - viewport()->width()));
}

void ArrangementEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.fillRect(viewport()->rect(), QColor("#101215"));

    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();
    const qint64 endMs = qRound64((sx + viewport()->width()) / m_pixelsPerSecond * 1000.0);
    const qint64 beatMs = m_project ? qRound64(60000.0 / std::max(1.0, m_project->tempoMap().bpm())) : 500;
    const qint64 barMs = beatMs * 4;
    qint64 firstMs = qRound64((sx / m_pixelsPerSecond * 1000.0));
    firstMs = std::max<qint64>(0, (firstMs / std::max<qint64>(1, beatMs) - 2) * beatMs);
    for (qint64 ms = firstMs; ms <= endMs + beatMs; ms += beatMs) {
        const double x = ms / 1000.0 * m_pixelsPerSecond - sx;
        if (x < 0 || x > viewport()->width()) continue;
        const bool bar = barMs > 0 && ms % barMs == 0;
        p.setPen(QPen(bar ? QColor("#555b64") : QColor("#292e34"), bar ? 1.5 : 1.0));
        p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
    }

    if (!m_project) return;

    for (int i = 0; i < m_project->tracks().size(); ++i) {
        const int y = yForTrack(i);
        p.fillRect(0, y, viewport()->width(), m_trackHeight,
                   i % 2 ? QColor("#14171a") : QColor("#181b1f"));
        p.setPen(QColor("#2a2f35"));
        p.drawLine(0, y + m_trackHeight - 1, viewport()->width(), y + m_trackHeight - 1);

        const auto& track = m_project->tracks()[i];
        for (const auto& note : track.notes()) {
            const double start = m_project->tempoMap().tickToSeconds(
                note.getStartTick(), m_project->ppq()) * 1000.0;
            const double finish = m_project->tempoMap().tickToSeconds(
                note.getEndTick(), m_project->ppq()) * 1000.0;
            const int x = qRound(start / 1000.0 * m_pixelsPerSecond) - sx;
            const int w = std::max(2, qRound((finish - start) / 1000.0 * m_pixelsPerSecond));
            const QRect r(x, y + 7, w, m_trackHeight - 15);
            p.setBrush(track.muted() ? QColor("#4a4d52") : QColor("#365f93"));
            p.setPen(QColor("#6389b6"));
            p.drawRoundedRect(r, 3, 3);
            p.setPen(Qt::white);
            p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, note.getLyric());
        }
    }

    const int audioY = m_project->tracks().size() * m_trackHeight - sy;
    p.setPen(QColor("#3a4048"));
    p.drawLine(0, audioY, viewport()->width(), audioY);
    p.setPen(QColor("#a2a7af"));
    p.drawText(8, audioY + 18, QStringLiteral("Audio"));
    for (const auto& clip : m_project->audioClips()) {
        const qint64 lengthMs = clip.durationMs > 0 ? clip.durationMs : 1000;
        const int x = qRound(clip.startMs / 1000.0 * m_pixelsPerSecond) - sx;
        const int w = std::max(20, qRound(lengthMs / 1000.0 * m_pixelsPerSecond));
        const QRect r(x, audioY + 4, w, std::max(24, m_trackHeight - 8));
        p.setBrush(clip.muted ? QColor("#45484d") : QColor("#486b55"));
        p.setPen(QColor("#82a88c"));
        p.drawRoundedRect(r, 3, 3);
        p.setPen(Qt::white);
        p.drawText(r.adjusted(5, 0, -5, 0), Qt::AlignVCenter | Qt::AlignLeft, QFileInfo(clip.path).fileName());
    }

    const int ph = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond) - sx;
    p.setPen(QPen(QColor("#ff5b6e"), 2));
    p.drawLine(ph, 0, ph, viewport()->height());
}

void ArrangementEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void ArrangementEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    setFocus();
    m_playheadMs = msAtX(qRound(event->position().x()));
    emit positionClicked(m_playheadMs);
    m_draggingPlayhead = true;
    viewport()->update();
}

void ArrangementEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_draggingPlayhead) return;
    m_playheadMs = msAtX(qRound(event->position().x()));
    emit positionClicked(m_playheadMs);
    viewport()->update();
}

void ArrangementEditor::mouseReleaseEvent(QMouseEvent*)
{
    m_draggingPlayhead = false;
}

}
