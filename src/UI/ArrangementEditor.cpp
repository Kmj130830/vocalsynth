#include "UI/ArrangementEditor.h"
#include "UI/EditorFrame.h"

#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QWidget>

#include <algorithm>

namespace myvocal {

ArrangementEditor::ArrangementEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setMinimumHeight(190);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setViewportMargins(0, 38, 0, 0);
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("ArrangementHeader"));
    header->setStyleSheet(QStringLiteral("QWidget{background:#181a1e;border-bottom:1px solid #343840;} QLabel{color:#aeb5bf;} QDoubleSpinBox{background:#22252a;color:#e9edf2;border:1px solid #3d434b;padding:2px 5px;min-width:72px;}"));
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
    m_draggingPlayhead = false;
    m_draggingAudioIndex = -1;
    if (m_bpmSpin) {
        const QSignalBlocker blocker(m_bpmSpin);
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
void ArrangementEditor::setTrackHeight(int pixels) { m_trackHeight = std::clamp(pixels, 48, 100); updateScrollRanges(); viewport()->update(); }
void ArrangementEditor::setPixelsPerSecond(double pixels) { m_pixelsPerSecond = std::clamp(pixels, 20.0, 500.0); updateScrollRanges(); viewport()->update(); }
qint64 ArrangementEditor::msAtX(int x) const { return std::max<qint64>(0, qRound64((x + horizontalScrollBar()->value()) / m_pixelsPerSecond * 1000.0)); }
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
            for (const auto& note : track.notes()) maxMs = std::max(maxMs, qRound64(m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0));
        }
        for (const auto& clip : m_project->audioClips()) maxMs = std::max(maxMs, clip.startMs + std::max<qint64>(0, clip.durationMs));
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 600 - viewport()->width()));
}

void ArrangementEditor::updateHeaderGeometry()
{
    if (auto* header = findChild<QWidget*>(QStringLiteral("ArrangementHeader"))) header->setGeometry(0, 0, width(), 38);
}

void ArrangementEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    EditorFrame::drawBackground(p, viewport()->rect());

    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();
    const double bpm = m_project ? std::max(20.0, m_project->tempoMap().bpm()) : 120.0;
    const qint64 beatMs = std::max<qint64>(1, qRound64(60000.0 / bpm));
    const qint64 barMs = beatMs * 4;
    const qint64 firstVisibleMs = std::max<qint64>(0, qRound64(sx / m_pixelsPerSecond * 1000.0) - beatMs * 2);
    const qint64 endVisibleMs = msAtX(viewport()->width());

    for (qint64 ms = (firstVisibleMs / beatMs) * beatMs; ms <= endVisibleMs + beatMs; ms += beatMs) {
        const double x = ms / 1000.0 * m_pixelsPerSecond - sx;
        if (x < 0 || x > viewport()->width()) continue;
        const bool bar = ms % barMs == 0;
        p.setPen(QPen(bar ? QColor("#4b5058") : QColor("#24282e"), bar ? 1.3 : 1.0));
        p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
        if (bar) {
            const qint64 beatNumber = ms / barMs + 1;
            p.setPen(QColor("#777e89"));
            p.drawText(QPointF(x + 4, 13), QString::number(beatNumber));
        }
    }

    if (!m_project) return;
    const int trackCount = static_cast<int>(m_project->tracks().size());
    const qint64 maxGapTicks = static_cast<qint64>(m_project->ppq()) * 2;

    for (int ti = 0; ti < trackCount + 1; ++ti) {
        const int y = ti * m_trackHeight - sy;
        if (y + m_trackHeight < 0 || y > viewport()->height()) continue;
        const bool audioLane = ti == trackCount;
        p.fillRect(0, y, viewport()->width(), m_trackHeight, ti % 2 ? QColor("#141619") : QColor("#181b1f"));
        p.setPen(QColor("#30353c"));
        p.drawLine(0, y + m_trackHeight - 1, viewport()->width(), y + m_trackHeight - 1);
        p.setPen(QColor("#b9bec6"));
        p.drawText(8, y + 17, audioLane ? QStringLiteral("Audio") : m_project->tracks()[ti].name());

        if (audioLane) {
            for (int ci = 0; ci < m_project->audioClips().size(); ++ci) {
                const auto& clip = m_project->audioClips().at(ci);
                if (clip.muted || !QFileInfo(clip.path).isFile()) continue;
                const int x = qRound(clip.startMs / 1000.0 * m_pixelsPerSecond) - sx;
                const int w = std::max(12, qRound(clip.durationMs / 1000.0 * m_pixelsPerSecond));
                const QRect r(x, y + 7, w, m_trackHeight - 14);
                if (!r.intersects(viewport()->rect())) continue;
                EditorFrame::drawPanel(p, r, ci == m_draggingAudioIndex);
                p.setPen(QColor("#cfd5dc"));
                p.drawText(r.adjusted(7, 0, -7, 0), Qt::AlignLeft | Qt::AlignVCenter, QFileInfo(clip.path).fileName());
                p.setPen(QColor("#555c65"));
                const int midY = r.center().y();
                p.drawLine(r.left() + 6, midY, r.right() - 6, midY);
            }
            continue;
        }

        const auto& track = m_project->tracks()[ti];
        p.setPen(QColor("#747b85"));
        p.drawText(8, y + 34, track.singerPath().isEmpty() ? QStringLiteral("No singer") : QFileInfo(track.singerPath()).baseName());

        const auto& notes = track.notes();
        if (notes.isEmpty()) continue;

        int first = 0;
        while (first < notes.size()) {
            int last = first;
            while (last + 1 < notes.size() && notes[last + 1].getStartTick() - notes[last].getEndTick() <= maxGapTicks) ++last;

            const auto& firstNote = notes[first];
            const auto& lastNote = notes[last];
            const double startMs = m_project->tempoMap().tickToSeconds(firstNote.getStartTick(), m_project->ppq()) * 1000.0;
            const double endMs = m_project->tempoMap().tickToSeconds(lastNote.getEndTick(), m_project->ppq()) * 1000.0;
            const int x = qRound(startMs / 1000.0 * m_pixelsPerSecond) - sx;
            const int w = std::max(8, qRound((endMs - startMs) / 1000.0 * m_pixelsPerSecond));
            const QRect partRect(x, y + 5, w, m_trackHeight - 10);
            if (partRect.intersects(viewport()->rect())) {
                const bool selected = std::any_of(notes.cbegin() + first, notes.cbegin() + last + 1, [](const Note& n) { return n.isSelected(); });
                EditorFrame::drawPanel(p, partRect, selected);

                int minMidi = 127;
                int maxMidi = 0;
                for (int i = first; i <= last; ++i) {
                    minMidi = std::min(minMidi, notes[i].getMidiNote());
                    maxMidi = std::max(maxMidi, notes[i].getMidiNote());
                }
                const int pitchSpan = std::max(12, maxMidi - minMidi);
                const int top = partRect.top() + 6;
                const int bottom = partRect.bottom() - 6;
                const int usableHeight = std::max(1, bottom - top);
                for (int i = first; i <= last; ++i) {
                    const auto& note = notes[i];
                    const double ns = m_project->tempoMap().tickToSeconds(note.getStartTick(), m_project->ppq()) * 1000.0;
                    const double ne = m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0;
                    const int nx = qRound(ns / 1000.0 * m_pixelsPerSecond) - sx;
                    const int nex = qRound(ne / 1000.0 * m_pixelsPerSecond) - sx;
                    const int ny = bottom - static_cast<int>(std::llround((note.getMidiNote() - minMidi) * usableHeight / static_cast<double>(pitchSpan)));
                    const int lineY = std::clamp(ny, top, bottom);
                    p.setPen(QPen(note.isSelected() ? QColor("#f0ca68") : QColor("#d2d8df"), note.isSelected() ? 2.0 : 1.4));
                    p.drawLine(nx, lineY, std::max(nx + 2, nex), lineY);
                }

                p.setPen(QColor("#dbe1e8"));
                const QString partLabel = firstNote.getLyric().isEmpty() ? QStringLiteral("Part") : firstNote.getLyric();
                if (partRect.width() > 42) p.drawText(partRect.adjusted(7, 2, -7, -2), Qt::AlignTop | Qt::AlignLeft, partLabel);
            }
            first = last + 1;
        }
    }

    const int px = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond) - sx;
    p.setPen(QPen(QColor("#d75e6c"), 1.5));
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
    if (event->button() != Qt::LeftButton || !m_project) return;
    setFocus();
    const QPoint pos = event->pos();
    const int trackCount = m_project->tracks().size();
    const int audioLaneY = trackCount * m_trackHeight - verticalScrollBar()->value();
    if (pos.y() >= audioLaneY && pos.y() < audioLaneY + m_trackHeight) {
        const qint64 clickMs = msAtX(pos.x());
        for (int i = m_project->audioClips().size() - 1; i >= 0; --i) {
            const auto& clip = m_project->audioClips().at(i);
            if (clickMs >= clip.startMs && clickMs <= clip.startMs + std::max<qint64>(1, clip.durationMs)) {
                m_draggingAudioIndex = i;
                m_audioDragOffsetMs = clickMs - clip.startMs;
                viewport()->update();
                return;
            }
        }
    }
    if (const int track = trackAtY(pos.y()); track >= 0) emit trackClicked(track);
    m_playheadMs = msAtX(pos.x());
    emit positionClicked(m_playheadMs);
    m_draggingPlayhead = true;
    viewport()->update();
}

void ArrangementEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_project) return;
    const int x = qRound(event->position().x());
    if (m_draggingAudioIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        const qint64 newStart = std::max<qint64>(0, msAtX(x) - m_audioDragOffsetMs);
        auto& clip = m_project->audioClips()[m_draggingAudioIndex];
        if (clip.startMs != newStart) {
            clip.startMs = newStart;
            emit documentChanged();
            viewport()->update();
        }
        return;
    }
    if (!m_draggingPlayhead) return;
    m_playheadMs = msAtX(x);
    emit positionClicked(m_playheadMs);
    viewport()->update();
}

void ArrangementEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (m_draggingAudioIndex >= 0) {
        m_draggingAudioIndex = -1;
        emit documentChanged();
        viewport()->update();
    }
    m_draggingPlayhead = false;
}

}