#include "UI/ArrangementEditor.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>

namespace myvocal {

ArrangementEditor::ArrangementEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_project(project)
{
    setMinimumHeight(150);
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    updateScrollRanges();
}

void ArrangementEditor::setProject(Project* project)
{
    m_project = project;
    updateScrollRanges();
    update();
}

void ArrangementEditor::setPlayheadMs(qint64 ms)
{
    m_playheadMs = std::max<qint64>(0, ms);
    const int x = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond);
    if (x < horizontalScrollBar()->value()) {
        horizontalScrollBar()->setValue(std::max(0, x - width() / 5));
    } else if (x > horizontalScrollBar()->value() + viewport()->width() * 4 / 5) {
        horizontalScrollBar()->setValue(std::max(0, x - viewport()->width() / 5));
    }
    update();
}

qint64 ArrangementEditor::playheadMs() const noexcept { return m_playheadMs; }

void ArrangementEditor::setTrackHeight(int pixels)
{
    m_trackHeight = std::clamp(pixels, 28, 100);
    updateScrollRanges();
    update();
}

void ArrangementEditor::setPixelsPerSecond(double pixels)
{
    m_pixelsPerSecond = std::clamp(pixels, 20.0, 500.0);
    updateScrollRanges();
    update();
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
    const int trackCount = m_project ? std::max(1, m_project->tracks().size()) : 1;
    verticalScrollBar()->setRange(0,
        std::max(0, trackCount * m_trackHeight - viewport()->height()));

    qint64 maxMs = 60000;
    if (m_project) {
        for (const auto& track : m_project->tracks()) {
            for (const auto& note : track.notes()) {
                maxMs = std::max(maxMs, qRound64(
                    m_project->tempoMap().tickToSeconds(
                        note.getEndTick(), m_project->ppq()) * 1000.0));
            }
        }
        for (const auto& clip : m_project->audioClips()) {
            maxMs = std::max(maxMs, clip.startMs + 60000);
        }
    }

    const int contentWidth = qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 800;
    horizontalScrollBar()->setRange(0,
        std::max(0, contentWidth - viewport()->width()));
}

void ArrangementEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.fillRect(viewport()->rect(), QColor("#111316"));

    const int scrollX = horizontalScrollBar()->value();
    const int scrollY = verticalScrollBar()->value();
    const qint64 endMs = qRound64(
        (scrollX + viewport()->width()) / m_pixelsPerSecond * 1000.0);
    const qint64 beatMs = m_project
        ? qRound64(60000.0 / m_project->tempoMap().bpm()) : 500;
    const qint64 barMs = beatMs * 4;

    if (beatMs > 0) {
        const qint64 firstMs = std::max<qint64>(0,
            qRound64(scrollX / m_pixelsPerSecond * 1000.0) / beatMs - 2) * beatMs;
        for (qint64 ms = firstMs; ms <= endMs + beatMs; ms += beatMs) {
            const double x = ms / 1000.0 * m_pixelsPerSecond - scrollX;
            if (x < 0 || x > viewport()->width()) continue;
            const bool bar = barMs > 0 && ms % barMs == 0;
            p.setPen(QPen(bar ? QColor("#4d535c") : QColor("#292e34"),
                          bar ? 1.5 : 1));
            p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
        }
    }

    if (!m_project) return;

    for (int i = 0; i < m_project->tracks().size(); ++i) {
        const int y = yForTrack(i);
        p.fillRect(0, y, viewport()->width(), m_trackHeight,
                   i % 2 == 0 ? QColor("#171a1e") : QColor("#14171a"));
        p.setPen(QColor("#2b3036"));
        p.drawLine(0, y + m_trackHeight - 1,
                   viewport()->width(), y + m_trackHeight - 1);

        const auto& track = m_project->tracks()[i];
        for (const auto& note : track.notes()) {
            const double startMs = m_project->tempoMap().tickToSeconds(
                note.getStartTick(), m_project->ppq()) * 1000.0;
            const double endNoteMs = m_project->tempoMap().tickToSeconds(
                note.getEndTick(), m_project->ppq()) * 1000.0;
            const int x = qRound(startMs / 1000.0 * m_pixelsPerSecond) - scrollX;
            const int w = std::max(2, qRound(
                (endNoteMs - startMs) / 1000.0 * m_pixelsPerSecond));
            const QRect rect(x, y + 6, w, m_trackHeight - 14);
            p.setBrush(track.muted() ? QColor("#4b4e54") : QColor("#3c6fae"));
            p.setPen(QColor("#6b94c0"));
            p.drawRoundedRect(rect, 3, 3);
            p.setPen(Qt::white);
            p.drawText(rect.adjusted(5, 0, -5, 0),
                       Qt::AlignVCenter | Qt::AlignLeft, note.getLyric());
        }
    }

    const int audioY = m_project->tracks().size() * m_trackHeight - scrollY;
    p.setPen(QColor("#3a4048"));
    p.drawLine(0, audioY, viewport()->width(), audioY);
    p.setPen(QColor("#8c929b"));
    p.drawText(8, audioY + 18, QStringLiteral("Audio"));

    for (const auto& clip : m_project->audioClips()) {
        const int x = qRound(clip.startMs / 1000.0 * m_pixelsPerSecond) - scrollX;
        const int w = std::max(20, qRound(12.0 * m_pixelsPerSecond));
        const QRect rect(x, audioY + 4, w, std::max(24, m_trackHeight - 8));
        p.setBrush(clip.muted ? QColor("#45484d") : QColor("#4f6a59"));
        p.setPen(QColor("#7fa58a"));
        p.drawRoundedRect(rect, 3, 3);
        p.setPen(Qt::white);
        p.drawText(rect.adjusted(5, 0, -5, 0),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QFileInfo(clip.path).fileName());
    }

    const int playheadX = qRound(
        m_playheadMs / 1000.0 * m_pixelsPerSecond) - scrollX;
    p.setPen(QPen(QColor("#ff5b6e"), 2));
    p.drawLine(playheadX, 0, playheadX, viewport()->height());
}

void ArrangementEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void ArrangementEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    const qint64 ms = msAtX(event->pos().x());
    m_playheadMs = ms;
    emit positionClicked(ms);
    m_draggingPlayhead = true;
    update();
}

void ArrangementEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_draggingPlayhead) return;
    const qint64 ms = msAtX(event->pos().x());
    m_playheadMs = ms;
    emit positionClicked(ms);
    update();
}

}