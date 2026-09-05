#include "UI/ArrangementEditor.h"

#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QWidget>

#include <algorithm>

namespace myvocal {

ArrangementEditor::ArrangementEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setMinimumHeight(210);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setViewportMargins(0, 38, 0, 0);

    auto* header = new QWidget(this);
    header->setStyleSheet(QStringLiteral("QWidget { background:#181b20; border-bottom:1px solid #343a42; } QLabel { color:#c7cdd5; } QDoubleSpinBox { background:#242931; color:#edf2f7; border:1px solid #414954; padding:2px 5px; min-width:72px; }"));
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(10, 4, 10, 4);
    layout->setSpacing(8);
    layout->addWidget(new QLabel(QStringLiteral("Arrangement"), header));
    layout->addSpacing(18);
    layout->addWidget(new QLabel(QStringLiteral("BPM"), header));
    m_bpmSpin = new QDoubleSpinBox(header);
    m_bpmSpin->setRange(20.0, 999.0);
    m_bpmSpin->setDecimals(2);
    m_bpmSpin->setSingleStep(1.0);
    m_bpmSpin->setValue(m_project ? m_project->tempoMap().bpm() : 120.0);
    layout->addWidget(m_bpmSpin);
    layout->addStretch(1);
    header->setObjectName(QStringLiteral("ArrangementHeader"));

    connect(m_bpmSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double bpm) {
        if (!m_project) return;
        m_project->tempoMap().setBpm(bpm);
        updateScrollRanges();
        viewport()->update();
        emit documentChanged();
    });

    updateHeaderGeometry();
    updateScrollRanges();
}

void ArrangementEditor::setProject(Project* project)
{
    m_project = project;
    if (m_bpmSpin) {
        QSignalBlocker blocker(m_bpmSpin);
        m_bpmSpin->setValue(m_project ? m_project->tempoMap().bpm() : 120.0);
    }
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
    else if (x > left + view * 4 / 5) horizontalScrollBar()->setValue(std::max(0, x - view * 2 / 5));
    viewport()->update();
}

qint64 ArrangementEditor::playheadMs() const noexcept { return m_playheadMs; }

void ArrangementEditor::setTrackHeight(int pixels)
{
    m_trackHeight = std::clamp(pixels, 44, 100);
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

int ArrangementEditor::trackAtY(int y) const
{
    const int index = (y + verticalScrollBar()->value()) / m_trackHeight;
    const int count = m_project ? static_cast<int>(m_project->tracks().size()) : 0;
    return index >= 0 && index < count ? index : -1;
}

void ArrangementEditor::updateScrollRanges()
{
    const int trackCount = m_project ? std::max(1, static_cast<int>(m_project->tracks().size()) + 1) : 1;
    verticalScrollBar()->setRange(0, std::max(0, trackCount * m_trackHeight - viewport()->height()));

    qint64 maxMs = 30000;
    if (m_project) {
        for (const auto& track : m_project->tracks()) {
            for (const auto& note : track.notes()) {
                maxMs = std::max(maxMs, qRound64(m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0));
            }
        }
        for (const auto& clip : m_project->audioClips()) {
            maxMs = std::max(maxMs, clip.startMs + std::max<qint64>(0, clip.durationMs));
        }
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 600 - viewport()->width()));
}

void ArrangementEditor::updateHeaderGeometry()
{
    if (auto* header = findChild<QWidget*>(QStringLiteral("ArrangementHeader"))) {
        header->setGeometry(0, 0, width(), 38);
    }
}

void ArrangementEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(viewport()->rect(), QColor("#101215"));

    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();
    const double bpm = m_project ? std::max(20.0, m_project->tempoMap().bpm()) : 120.0;
    const qint64 beatMs = std::max<qint64>(1, qRound64(60000.0 / bpm));
    const qint64 barMs = beatMs * 4;
    const qint64 firstVisibleMs = std::max<qint64>(0, qRound64(sx / m_pixelsPerSecond * 1000.0) - beatMs * 2);
    const qint64 endVisibleMs = msAtX(viewport()->width());

    const qint64 firstGrid = (firstVisibleMs / beatMs) * beatMs;
    for (qint64 ms = firstGrid; ms <= endVisibleMs + beatMs; ms += beatMs) {
        const double x = ms / 1000.0 * m_pixelsPerSecond - sx;
        if (x < 0 || x > viewport()->width()) continue;
        const bool bar = barMs > 0 && ms % barMs == 0;
        p.setPen(QPen(bar ? QColor("#555d68") : QColor("#272c33"), bar ? 1.5 : 1.0));
        p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
    }

    if (!m_project) return;
    const int trackCount = static_cast<int>(m_project->tracks().size());
    for (int ti = 0; ti < trackCount + 1; ++ti) {
        const int y = ti * m_trackHeight - sy;
        if (y + m_trackHeight < 0 || y > viewport()->height()) continue;
        const bool audioLane = ti == trackCount;
        p.fillRect(0, y, viewport()->width(), m_trackHeight, ti % 2 ? QColor("#14171b") : QColor("#181b20"));
        p.setPen(QColor("#303640"));
        p.drawLine(0, y + m_trackHeight - 1, viewport()->width(), y + m_trackHeight - 1);

        p.setPen(QColor("#b6bdc7"));
        p.drawText(8, y + 18, audioLane ? QStringLiteral("Audio") : m_project->tracks()[ti].name());
        if (!audioLane) {
            const auto& track = m_project->tracks()[ti];
            const QString singer = QFileInfo(track.singerPath()).baseName();
            p.setPen(QColor("#717b87"));
            p.drawText(8, y + 36, track.singerPath().isEmpty() ? QStringLiteral("No singer") : singer);

            for (const auto& note : track.notes()) {
                const double startMs = m_project->tempoMap().tickToSeconds(note.getStartTick(), m_project->ppq()) * 1000.0;
                const double endMs = m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0;
                const int x = qRound(startMs / 1000.0 * m_pixelsPerSecond) - sx;
                const int w = std::max(2, qRound((endMs - startMs) / 1000.0 * m_pixelsPerSecond));
                const QRect r(x, y + 6, w, m_trackHeight - 12);
                if (!r.intersects(viewport()->rect())) continue;
                p.setBrush(track.muted() ? QColor("#33363b") : QColor("#385a7e"));
                p.setPen(track.solo() ? QColor("#f1c75b") : QColor("#5f84a9"));
                p.drawRoundedRect(r, 3, 3);
                if (w > 18) {
                    p.setPen(QColor("#edf3f9"));
                    p.drawText(r.adjusted(5, 0, -5, 0), Qt::AlignCenter, note.getLyric());
                }
            }
        } else {
            for (const auto& clip : m_project->audioClips()) {
                if (clip.muted) continue;
                const int x = qRound(clip.startMs / 1000.0 * m_pixelsPerSecond) - sx;
                const int w = std::max(12, qRound(clip.durationMs / 1000.0 * m_pixelsPerSecond));
                const QRect r(x, y + 8, w, m_trackHeight - 16);
                if (!r.intersects(viewport()->rect())) continue;
                p.setBrush(QColor("#3e4148"));
                p.setPen(QColor("#808791"));
                p.drawRoundedRect(r, 4, 4);
                p.setPen(QColor("#d4d8de"));
                p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, QFileInfo(clip.path).fileName());
            }
        }
    }

    const int px = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond) - sx;
    p.setPen(QPen(QColor("#ff5b6e"), 2));
    p.drawLine(px, 0, px, viewport()->height());
}

void ArrangementEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateHeaderGeometry();
    updateScrollRanges();
}

void ArrangementEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    setFocus();
    const QPoint pos = event->pos();
    if (const int track = trackAtY(pos.y()); track >= 0) emit trackClicked(track);
    m_playheadMs = msAtX(qRound(pos.x()));
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