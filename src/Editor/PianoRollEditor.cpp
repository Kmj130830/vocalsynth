#include "Editor/PianoRollEditor.h"
#include "Core/SongTime.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace myvocal {
namespace {
constexpr qint64 kDefaultNoteLength = 480;
constexpr int kResizeHandlePx = 9;
constexpr double kPitchSemitonePixels = 22.0;
constexpr double kPitchSampleMs = 5.0;
}

PianoRollEditor::PianoRollEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        emit verticalPitchChanged(m_topMidi - verticalScrollBar()->value() / std::max(1, m_rowHeight));
        viewport()->update();
    });
    updateScrollRanges();
}

Project* PianoRollEditor::project() const noexcept { return m_project; }

void PianoRollEditor::setActiveTrack(int index)
{
    const int count = m_project ? static_cast<int>(m_project->tracks().size()) : 0;
    m_activeTrack = count > 0 ? std::clamp(index, 0, count - 1) : 0;
    m_dragging = m_resizing = m_panning = m_playheadDragging = m_rightDrawing = m_pitchDrawing = false;
    m_dragIds.clear();
    m_dragOriginalStart.clear();
    m_dragOriginalEnd.clear();
    m_dragOriginalPitch.clear();
    m_resizeId = m_rightDrawId = m_pitchNoteId = -1;
    viewport()->update();
}

int PianoRollEditor::activeTrack() const noexcept { return m_activeTrack; }

void PianoRollEditor::setTool(EditTool tool)
{
    m_tool = tool;
    viewport()->setCursor(tool == EditTool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
    viewport()->update();
}

EditTool PianoRollEditor::tool() const noexcept { return m_tool; }

void PianoRollEditor::setPlayheadTick(qint64 tick)
{
    m_playhead = std::max<qint64>(0, tick);
    ensureVisibleTick(m_playhead);
    viewport()->update();
}

qint64 PianoRollEditor::playheadTick() const noexcept { return m_playhead; }
void PianoRollEditor::setSnapEnabled(bool enabled) { m_snap = enabled; viewport()->update(); }
bool PianoRollEditor::snapEnabled() const noexcept { return m_snap; }
void PianoRollEditor::setShowGrid(bool value) { m_showGrid = value; viewport()->update(); }
bool PianoRollEditor::showGrid() const noexcept { return m_showGrid; }
void PianoRollEditor::setGridTicks(qint64 ticks) { m_gridTicks = std::clamp<qint64>(ticks, 1, 3840); viewport()->update(); }
qint64 PianoRollEditor::gridTicks() const noexcept { return m_gridTicks; }

qint64 PianoRollEditor::snapTick(qint64 tick) const
{
    tick = std::max<qint64>(0, tick);
    if (!m_snap || m_gridTicks <= 1) return tick;
    return ((tick + m_gridTicks / 2) / m_gridTicks) * m_gridTicks;
}

qint64 PianoRollEditor::tickAtX(int x) const
{
    const double sceneX = static_cast<double>(x + horizontalScrollBar()->value());
    return snapTick(static_cast<qint64>(SongTime::pixelToTick(sceneX, m_pxPerBeat)));
}

int PianoRollEditor::midiAtY(int y) const
{
    return std::clamp(m_topMidi - (y + verticalScrollBar()->value()) / std::max(1, m_rowHeight), 0, 127);
}

QRect PianoRollEditor::noteRect(const Note& note) const
{
    const int x = qRound(SongTime::tickToPixel(note.getStartTick(), m_pxPerBeat)) - horizontalScrollBar()->value();
    const int w = std::max(4, qRound(SongTime::tickToPixel(note.getDurationTick(), m_pxPerBeat)));
    const int y = (m_topMidi - note.getMidiNote()) * m_rowHeight - verticalScrollBar()->value();
    return QRect(x, y, w, std::max(2, m_rowHeight - 2));
}

Note* PianoRollEditor::noteAt(const QPoint& pos)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return nullptr;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
        if (noteRect(*it).contains(pos)) return &*it;
    }
    return nullptr;
}

bool PianoRollEditor::isResizeHandle(const Note& note, const QPoint& pos) const
{
    const QRect r = noteRect(note);
    return r.contains(pos) && pos.x() >= r.right() - std::min(kResizeHandlePx, std::max(1, r.width() / 2));
}

bool PianoRollEditor::hasTimeOverlap(qint64 start, qint64 end, qint64 ignoreId) const
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size()) || end <= start) return true;
    for (const auto& n : m_project->tracks()[m_activeTrack].notes()) {
        if (n.getId() == ignoreId) continue;
        if (start < n.getEndTick() && end > n.getStartTick()) return true;
    }
    return false;
}

qint64 PianoRollEditor::constrainedMoveDelta(qint64 desiredDelta) const
{
    if (!m_project || m_dragIds.isEmpty()) return desiredDelta;

    // Compute the selection bounds from the immutable drag snapshot, not from
    // the notes that were already moved by previous mouseMove events. This is
    // what prevents the selection from oscillating left/right while dragging.
    qint64 minStart = std::numeric_limits<qint64>::max();
    qint64 maxEnd = std::numeric_limits<qint64>::min();
    for (qint64 id : m_dragIds) {
        if (!m_dragOriginalStart.contains(id)) continue;
        const qint64 start = m_dragOriginalStart.value(id);
        const qint64 end = m_dragOriginalEnd.value(id, start + 1);
        minStart = std::min(minStart, start);
        maxEnd = std::max(maxEnd, end);
    }
    if (minStart == std::numeric_limits<qint64>::max()) return desiredDelta;

    qint64 minDelta = -minStart;
    qint64 maxDelta = std::numeric_limits<qint64>::max() / 4;

    for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
        if (m_dragOriginalStart.contains(other.getId())) continue;
        if (desiredDelta >= 0) {
            maxDelta = std::min(maxDelta, other.getStartTick() - maxEnd);
        } else {
            minDelta = std::max(minDelta, other.getEndTick() - minStart);
        }
    }

    if (minDelta > maxDelta) return 0;
    return std::clamp(desiredDelta, minDelta, maxDelta);
}

qint64 PianoRollEditor::availableEndForNote(const Note& note, qint64 desiredEnd) const
{
    desiredEnd = std::max(note.getStartTick() + 1, desiredEnd);
    if (!m_project) return desiredEnd;
    for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
        if (other.getId() != note.getId() && other.getStartTick() > note.getStartTick()) {
            desiredEnd = std::min(desiredEnd, other.getStartTick());
        }
    }
    return std::max(note.getStartTick() + 1, desiredEnd);
}

void PianoRollEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(viewport()->rect(), QColor("#15171a"));

    const int sy = verticalScrollBar()->value();
    const int firstMidi = std::clamp(m_topMidi - (sy + viewport()->height()) / m_rowHeight - 1, 0, 127);
    const int lastMidi = std::clamp(m_topMidi - sy / m_rowHeight + 1, 0, 127);
    static const QStringList names = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

    for (int midi = firstMidi; midi <= lastMidi; ++midi) {
        const int y = (m_topMidi - midi) * m_rowHeight - sy;
        if (midi % 12 == 0) p.fillRect(0, y, viewport()->width(), m_rowHeight, QColor("#20242a"));
        else if (names.at(midi % 12).contains('#')) p.fillRect(0, y, viewport()->width(), m_rowHeight, QColor("#191b1f"));
        p.setPen(QColor(midi % 12 == 0 ? "#424953" : "#292d33"));
        p.drawLine(0, y, viewport()->width(), y);
    }

    if (m_showGrid && m_gridTicks > 0) {
        const qint64 start = static_cast<qint64>(SongTime::pixelToTick(horizontalScrollBar()->value(), m_pxPerBeat));
        const qint64 end = static_cast<qint64>(SongTime::pixelToTick(horizontalScrollBar()->value() + viewport()->width(), m_pxPerBeat));
        const qint64 first = std::max<qint64>(0, start / m_gridTicks - 1) * m_gridTicks;
        const qint64 barTicks = m_project ? static_cast<qint64>(m_project->ppq() * 4) : 1920;
        for (qint64 tick = first; tick <= end + m_gridTicks; tick += m_gridTicks) {
            const double x = SongTime::tickToPixel(tick, m_pxPerBeat) - horizontalScrollBar()->value();
            if (x < 0 || x > viewport()->width()) continue;
            const bool bar = barTicks > 0 && tick % barTicks == 0;
            p.setPen(QPen(bar ? QColor("#555c66") : QColor("#30353b"), bar ? 1.5 : 1.0));
            p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
        }
    }

    if (m_project && m_activeTrack >= 0 && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
        for (const auto& note : m_project->tracks()[m_activeTrack].notes()) {
            const QRect r = noteRect(note);
            if (!r.intersects(viewport()->rect())) continue;

            p.setBrush(note.isSelected() ? QColor("#4f8cf7") : QColor("#3c6fae"));
            p.setPen(note.isSelected() ? QColor("#c6dcff") : QColor("#668fbe"));
            p.drawRoundedRect(r, 3, 3);
            p.setPen(Qt::white);
            p.drawText(r.adjusted(5, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter, note.getLyric());

            if (note.isSelected()) {
                p.setPen(QColor("#e0ebff"));
                p.drawLine(r.right() - 3, r.top() + 3, r.right() - 3, r.bottom() - 3);
            }

            const auto& points = note.getPitchCurve().points();
            if (!points.empty()) {
                QPainterPath path;
                const double noteMs = m_project->tempoMap().tickToSeconds(note.getDurationTick(), m_project->ppq()) * 1000.0;
                const double safeMs = std::max(1.0, noteMs);
                const int samples = std::max(2, r.width() / 3);
                for (int i = 0; i <= samples; ++i) {
                    const double u = static_cast<double>(i) / samples;
                    const double localMs = u * safeMs;
                    const double offset = note.getPitchCurve().evaluateSmooth(localMs);
                    const double x = r.left() + u * r.width();
                    const double y = r.center().y() - offset * kPitchSemitonePixels;
                    if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
                }
                p.setPen(QPen(QColor("#ffd166"), 1.6));
                p.setBrush(Qt::NoBrush);
                p.drawPath(path);
            }
        }

        if (m_rightDrawing && m_rightDrawId >= 0) {
            if (const Note* draft = m_project->tracks()[m_activeTrack].findNote(m_rightDrawId)) {
                p.setBrush(QColor(79, 140, 247, 90));
                p.setPen(QPen(QColor("#bcd5ff"), 1, Qt::DashLine));
                p.drawRect(noteRect(*draft));
            }
        }
    }

    const int px = qRound(SongTime::tickToPixel(m_playhead, m_pxPerBeat)) - horizontalScrollBar()->value();
    p.setPen(QPen(QColor("#ff586b"), 2));
    p.drawLine(px, 0, px, viewport()->height());
}

void PianoRollEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void PianoRollEditor::mousePressEvent(QMouseEvent* event)
{
    if (!m_project || m_project->tracks().isEmpty()) return;
    setFocus();
    const QPoint pressPos = event->pos();
    m_lastPos = pressPos;
    m_mouseMovedDuringDrag = false;

    if (m_lyricEditor) finishLyricEdit(true);
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && m_tool == EditTool::Pan)) {
        m_panning = true;
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }

    Note* hit = noteAt(pressPos);
    const int pressedMidi = midiAtY(pressPos.y());
    emit keyboardPreviewRequested(pressedMidi);

    if (event->button() == Qt::LeftButton) {
        if (m_tool == EditTool::Pitch || m_tool == EditTool::PitchLine) {
            if (hit) beginPitchDraw(pressPos);
            return;
        }

        const int phx = qRound(SongTime::tickToPixel(m_playhead, m_pxPerBeat)) - horizontalScrollBar()->value();
        if (!hit && std::abs(pressPos.x() - phx) <= 8) {
            m_playheadDragging = true;
            m_playhead = tickAtX(pressPos.x());
            emit requestPlaybackTick(m_playhead);
            viewport()->update();
            return;
        }

        if (hit && isResizeHandle(*hit, pressPos)) {
            for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
            hit->setSelected(true);
            m_dragging = true;
            m_resizing = true;
            m_resizeId = hit->getId();
            m_dragIds = {hit->getId()};
            m_dragOriginalStart.insert(hit->getId(), hit->getStartTick());
            m_dragOriginalEnd.insert(hit->getId(), hit->getEndTick());
            m_dragOriginalPitch.insert(hit->getId(), hit->getMidiNote());
            m_resizeOriginalEnd = hit->getEndTick();
            m_dragAnchorTick = tickAtX(pressPos.x());
            viewport()->update();
            return;
        }

        if (m_tool == EditTool::Eraser) { deleteNoteAt(pressPos); return; }
        if (m_tool == EditTool::Knife) { splitNoteAt(pressPos); return; }

        if (m_tool == EditTool::Select) {
            if (!hit) {
                for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
                m_playhead = tickAtX(pressPos.x());
                emit requestPlaybackTick(m_playhead);
                viewport()->update();
                return;
            }
            const bool ctrl = event->modifiers() & Qt::ControlModifier;
            if (!ctrl) {
                for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
            }
            hit->setSelected(ctrl ? !hit->isSelected() : true);

            m_dragIds.clear();
            m_dragOriginalStart.clear();
            m_dragOriginalEnd.clear();
            m_dragOriginalPitch.clear();
            for (const auto& n : m_project->tracks()[m_activeTrack].notes()) {
                if (!n.isSelected()) continue;
                m_dragIds.push_back(n.getId());
                m_dragOriginalStart.insert(n.getId(), n.getStartTick());
                m_dragOriginalEnd.insert(n.getId(), n.getEndTick());
                m_dragOriginalPitch.insert(n.getId(), n.getMidiNote());
            }

            m_dragging = !m_dragIds.isEmpty();
            m_dragAnchorTick = tickAtX(pressPos.x());
            m_dragAnchorPitch = midiAtY(pressPos.y());
            viewport()->update();
            return;
        }

        if ((m_tool == EditTool::Pen || m_tool == EditTool::PenPlus) && !hit) {
            createNote(pressPos);
            return;
        }
    }

    if (event->button() == Qt::RightButton && m_tool != EditTool::Pitch && m_tool != EditTool::PitchLine) {
        if (hit) return;
        const qint64 start = tickAtX(pressPos.x());
        const qint64 initialEnd = start + std::max<qint64>(1, m_gridTicks);
        if (hasTimeOverlap(start, initialEnd)) return;

        Note note(QDateTime::currentMSecsSinceEpoch());
        note.setStartTick(start);
        note.setDurationTick(std::max<qint64>(1, m_gridTicks));
        note.setMidiNote(pressedMidi);
        note.setLyric(QStringLiteral("a"));
        note.setSelected(true);
        m_project->tracks()[m_activeTrack].addNote(std::move(note));
        sortNotes();
        auto& notes = m_project->tracks()[m_activeTrack].notes();
        for (auto it = notes.begin(); it != notes.end(); ++it) {
            if (it->getStartTick() == start && it->getMidiNote() == pressedMidi) {
                m_rightDrawId = it->getId();
                break;
            }
        }
        m_rightDrawStart = start;
        m_rightDrawPitch = pressedMidi;
        m_rightDrawing = m_rightDrawId >= 0;
        emit documentChanged();
        viewport()->update();
    }
}

void PianoRollEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_project) return;
    const QPoint current = event->pos();

    if (m_panning) {
        const QPoint delta = current - m_lastPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastPos = current;
        return;
    }

    if (m_playheadDragging) {
        m_playhead = tickAtX(current.x());
        emit requestPlaybackTick(m_playhead);
        viewport()->update();
        return;
    }

    if (m_pitchDrawing) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_pitchNoteId)) {
            drawPitchPoint(*note, current);
            emit documentChanged();
            viewport()->update();
        }
        return;
    }

    if (m_rightDrawing && m_rightDrawId >= 0) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_rightDrawId)) {
            qint64 end = snapTick(static_cast<qint64>(SongTime::pixelToTick(current.x() + horizontalScrollBar()->value(), m_pxPerBeat)));
            if (end <= m_rightDrawStart) end = m_rightDrawStart + std::max<qint64>(1, m_gridTicks);
            for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
                if (other.getId() != note->getId() && other.getStartTick() > m_rightDrawStart) {
                    end = std::min(end, other.getStartTick());
                }
            }
            note->setDurationTick(std::max<qint64>(1, end - note->getStartTick()));
            emit documentChanged();
            viewport()->update();
        }
        return;
    }

    if (!m_dragging) return;
    m_mouseMovedDuringDrag = true;

    if (m_resizing) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_resizeId)) {
            const qint64 currentTick = tickAtX(current.x());
            const qint64 desired = m_resizeOriginalEnd + currentTick - m_dragAnchorTick;
            const qint64 end = availableEndForNote(*note, desired);
            note->setDurationTick(std::max<qint64>(1, end - note->getStartTick()));
            emit documentChanged();
            viewport()->update();
        }
        return;
    }

    const qint64 desiredDelta = tickAtX(current.x()) - m_dragAnchorTick;
    const qint64 delta = constrainedMoveDelta(desiredDelta);
    const int pitchDelta = midiAtY(current.y()) - m_dragAnchorPitch;

    for (qint64 id : m_dragIds) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(id)) {
            note->setStartTick(std::max<qint64>(0, m_dragOriginalStart.value(id) + delta));
            note->setMidiNote(std::clamp(m_dragOriginalPitch.value(id) + pitchDelta, 0, 127));
        }
    }
    sortNotes();
    emit documentChanged();
    viewport()->update();
}

void PianoRollEditor::mouseReleaseEvent(QMouseEvent*)
{
    if (m_rightDrawing) commitRightDragNote();
    if (m_pitchDrawing) finishPitchDraw();
    m_dragging = m_resizing = m_panning = m_playheadDragging = m_rightDrawing = m_pitchDrawing = false;
    m_resizeId = m_rightDrawId = m_pitchNoteId = -1;
    m_dragIds.clear();
    m_dragOriginalStart.clear();
    m_dragOriginalEnd.clear();
    m_dragOriginalPitch.clear();
    viewport()->setCursor(m_tool == EditTool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
    sortNotes();
    viewport()->update();
}

void PianoRollEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? 1.12 : 0.89;
        const qint64 anchor = static_cast<qint64>(SongTime::pixelToTick(event->position().x() + horizontalScrollBar()->value(), m_pxPerBeat));
        m_pxPerBeat = std::clamp(m_pxPerBeat * factor, 30.0, 600.0);
        const int x = qRound(SongTime::tickToPixel(anchor, m_pxPerBeat));
        horizontalScrollBar()->setValue(std::max(0, x - qRound(event->position().x())));
        updateScrollRanges();
        viewport()->update();
        return;
    }
    if (event->modifiers() & Qt::ShiftModifier)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().y() / 2);
    else
        verticalScrollBar()->setValue(verticalScrollBar()->value() - event->angleDelta().y() / 2);
}

void PianoRollEditor::keyPressEvent(QKeyEvent* event)
{
    if (m_lyricEditor) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Escape && m_lyricEditor) {
        finishLyricEdit(false);
        return;
    }
    if (event->key() == Qt::Key_F2 || event->key() == Qt::Key_Enter) {
        if (m_project && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
            for (auto& n : m_project->tracks()[m_activeTrack].notes()) {
                if (n.isSelected()) {
                    beginLyricEdit(n);
                    return;
                }
            }
        }
    }
    if (event->matches(QKeySequence::Delete)) {
        if (m_project && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
            auto& notes = m_project->tracks()[m_activeTrack].notes();
            bool changed = false;
            for (int i = notes.size() - 1; i >= 0; --i) {
                if (!notes[i].isSelected()) continue;
                notes.removeAt(i);
                changed = true;
            }
            if (changed) {
                emit documentChanged();
                viewport()->update();
            }
        }
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void PianoRollEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!m_project || event->button() != Qt::LeftButton) return;
    if (Note* note = noteAt(event->pos())) {
        beginLyricEdit(*note);
        emit noteDoubleClicked(note->getId());
    } else {
        createNote(event->pos());
    }
}

void PianoRollEditor::createNote(const QPoint& pos)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    const qint64 start = tickAtX(pos.x());
    qint64 end = start + kDefaultNoteLength;

    for (const auto& n : m_project->tracks()[m_activeTrack].notes()) {
        if (n.getStartTick() > start) end = std::min(end, n.getStartTick());
    }
    end = snapTick(end);
    if (end <= start) end = start + std::max<qint64>(1, m_gridTicks);
    end = availableEndForNote(Note(QDateTime::currentMSecsSinceEpoch()), end);
    if (hasTimeOverlap(start, end) || end <= start) return;

    for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
    Note note(QDateTime::currentMSecsSinceEpoch());
    note.setStartTick(start);
    note.setDurationTick(end - start);
    note.setMidiNote(midiAtY(pos.y()));
    note.setLyric(QStringLiteral("a"));
    note.setSelected(true);
    m_project->tracks()[m_activeTrack].addNote(std::move(note));
    sortNotes();
    updateScrollRanges();
    emit documentChanged();
    viewport()->update();
}

void PianoRollEditor::createDraggedNote(qint64 startTick, qint64 endTick, int midi)
{
    startTick = snapTick(startTick);
    endTick = snapTick(endTick);
    if (!m_project || endTick <= startTick || hasTimeOverlap(startTick, endTick)) return;
    Note note(QDateTime::currentMSecsSinceEpoch());
    note.setStartTick(startTick);
    note.setDurationTick(std::max<qint64>(1, endTick - startTick));
    note.setMidiNote(std::clamp(midi, 0, 127));
    note.setLyric(QStringLiteral("a"));
    note.setSelected(true);
    m_project->tracks()[m_activeTrack].addNote(std::move(note));
    sortNotes();
    emit documentChanged();
    viewport()->update();
}

void PianoRollEditor::deleteNoteAt(const QPoint& pos)
{
    if (!m_project) return;
    if (Note* n = noteAt(pos)) {
        m_project->tracks()[m_activeTrack].removeNoteById(n->getId());
        emit documentChanged();
        viewport()->update();
    }
}

void PianoRollEditor::splitNoteAt(const QPoint& pos)
{
    Note* note = noteAt(pos);
    if (!note) return;
    const qint64 tick = tickAtX(pos.x());
    if (tick <= note->getStartTick() || tick >= note->getEndTick()) return;

    Note right = note->clone();
    right.setStartTick(tick);
    right.setDurationTick(note->getEndTick() - tick);
    right.setSelected(false);
    note->setDurationTick(tick - note->getStartTick());
    m_project->tracks()[m_activeTrack].addNote(std::move(right));
    sortNotes();
    emit documentChanged();
    viewport()->update();
}

void PianoRollEditor::beginLyricEdit(Note& note)
{
    if (m_lyricEditor) finishLyricEdit(true);
    m_editingLyricId = note.getId();
    m_lyricEditor = new QLineEdit(viewport());
    m_lyricEditor->setText(note.getLyric());
    m_lyricEditor->selectAll();
    m_lyricEditor->setFrame(false);
    m_lyricEditor->setGeometry(noteRect(note).adjusted(2, 2, -2, -2));
    m_lyricEditor->setStyleSheet(QStringLiteral("QLineEdit { background:#20252c; color:white; border:1px solid #79a9ff; padding:0 3px; }"));
    connect(m_lyricEditor, &QLineEdit::editingFinished, this, [this] { finishLyricEdit(true); });
    m_lyricEditor->show();
    m_lyricEditor->setFocus();
}

void PianoRollEditor::finishLyricEdit(bool accept)
{
    if (!m_lyricEditor) return;
    const QString text = m_lyricEditor->text();
    const qint64 id = m_editingLyricId;
    QLineEdit* edit = m_lyricEditor;
    m_lyricEditor = nullptr;
    m_editingLyricId = -1;
    if (accept && m_project && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
        if (Note* n = m_project->tracks()[m_activeTrack].findNote(id)) {
            if (n->getLyric() != text) {
                n->setLyric(text);
                emit documentChanged();
            }
        }
    }
    edit->deleteLater();
    viewport()->update();
}

void PianoRollEditor::commitRightDragNote()
{
    if (!m_project || m_rightDrawId < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    if (Note* n = m_project->tracks()[m_activeTrack].findNote(m_rightDrawId)) {
        const qint64 end = snapTick(n->getEndTick());
        n->setDurationTick(std::max<qint64>(1, end - n->getStartTick()));
    }
    emit documentChanged();
    sortNotes();
}

void PianoRollEditor::drawPitchPoint(Note& note, const QPoint& pos)
{
    const QRect r = noteRect(note);
    if (r.width() <= 0) return;

    const int clampedX = std::clamp(pos.x(), r.left(), r.right());
    const int clampedY = std::clamp(pos.y(), r.top() - m_rowHeight * 12, r.bottom() + m_rowHeight * 12);
    const double u = r.width() > 0 ? static_cast<double>(clampedX - r.left()) / static_cast<double>(r.width()) : 0.0;
    const double durationMs = std::max(1.0, m_project->tempoMap().tickToSeconds(note.getDurationTick(), m_project->ppq()) * 1000.0);
    const double timeMs = std::clamp(u * durationMs, 0.0, durationMs);
    const double offset = static_cast<double>(r.center().y() - clampedY) / kPitchSemitonePixels;

    auto& points = note.getPitchCurve().points();
    if (!points.empty() && std::abs(points.back().time - timeMs) < kPitchSampleMs * 0.5) {
        points.back().semitoneOffset = offset;
    } else {
        note.getPitchCurve().addPoint({timeMs, offset});
    }
}

void PianoRollEditor::beginPitchDraw(const QPoint& pos)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    Note* note = noteAt(pos);
    if (!note) return;
    for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
    note->setSelected(true);
    note->getPitchCurve().clear();
    m_pitchNoteId = note->getId();
    m_pitchDrawing = true;
    drawPitchPoint(*note, pos);
    emit documentChanged();
    viewport()->update();
}

void PianoRollEditor::finishPitchDraw()
{
    if (!m_pitchDrawing) return;
    emit documentChanged();
    viewport()->update();
}

void PianoRollEditor::sortNotes()
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
        return a.getStartTick() != b.getStartTick() ? a.getStartTick() < b.getStartTick() : a.getId() < b.getId();
    });
}

void PianoRollEditor::ensureVisibleTick(qint64 tick)
{
    const int x = qRound(SongTime::tickToPixel(tick, m_pxPerBeat));
    const int left = horizontalScrollBar()->value();
    const int right = left + viewport()->width();
    if (x < left) horizontalScrollBar()->setValue(std::max(0, x - 32));
    else if (x > right) horizontalScrollBar()->setValue(std::max(0, x - viewport()->width() + 32));
}

void PianoRollEditor::updateScrollRanges()
{
    const int vw = std::max(1, viewport()->width());
    const int vh = std::max(1, viewport()->height());
    const int sceneWidth = std::max(20000, qRound(SongTime::tickToPixel(480 * 2048, m_pxPerBeat)));
    const int sceneHeight = 128 * m_rowHeight;
    horizontalScrollBar()->setPageStep(vw);
    horizontalScrollBar()->setRange(0, std::max(0, sceneWidth - vw));
    verticalScrollBar()->setPageStep(vh);
    verticalScrollBar()->setRange(0, std::max(0, sceneHeight - vh));
}

void PianoRollEditor::invalidate()
{
    updateScrollRanges();
    viewport()->update();
}

}