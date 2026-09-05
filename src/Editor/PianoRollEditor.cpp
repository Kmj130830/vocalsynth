#include "Editor/PianoRollEditor.h"
#include "Core/SongTime.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <limits>

namespace myvocal {
namespace {
constexpr qint64 kDefaultNoteLength = 480;
constexpr int kResizeHandlePx = 9;
constexpr double kPitchSemitonePixels = 2.0;
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
    m_dragging = false;
    m_resizing = false;
    m_panning = false;
    m_playheadDragging = false;
    m_rightDrawing = false;
    m_pitchDrawing = false;
    m_dragIds.clear();
    m_dragOriginalStart.clear();
    m_dragOriginalEnd.clear();
    m_dragOriginalPitch.clear();
    viewport()->update();
}

int PianoRollEditor::activeTrack() const noexcept { return m_activeTrack; }

void PianoRollEditor::setTool(EditTool tool)
{
    m_tool = tool;
    viewport()->setCursor(tool == EditTool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

EditTool PianoRollEditor::tool() const noexcept { return m_tool; }

void PianoRollEditor::setPlayheadTick(qint64 tick)
{
    m_playhead = std::max<qint64>(0, tick);
    ensureVisibleTick(m_playhead);
    viewport()->update();
}

qint64 PianoRollEditor::playheadTick() const noexcept { return m_playhead; }

void PianoRollEditor::setSnapEnabled(bool enabled)
{
    m_snap = enabled;
    viewport()->update();
}

bool PianoRollEditor::snapEnabled() const noexcept { return m_snap; }

void PianoRollEditor::setShowGrid(bool value)
{
    m_showGrid = value;
    viewport()->update();
}

bool PianoRollEditor::showGrid() const noexcept { return m_showGrid; }

void PianoRollEditor::setGridTicks(qint64 ticks)
{
    m_gridTicks = std::clamp<qint64>(ticks, 1, 3840);
    viewport()->update();
}

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
    const int endX = qRound(SongTime::tickToPixel(note.getEndTick(), m_pxPerBeat)) - horizontalScrollBar()->value();
    const int y = (m_topMidi - note.getMidiNote()) * m_rowHeight - verticalScrollBar()->value();
    return QRect(x, y + 1, std::max(2, endX - x), std::max(2, m_rowHeight - 2));
}

Note* PianoRollEditor::noteAt(const QPoint& pos)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return nullptr;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
        if (noteRect(*it).contains(pos)) return &(*it);
    }
    return nullptr;
}

bool PianoRollEditor::isResizeHandle(const Note& note, const QPoint& pos) const
{
    const QRect r = noteRect(note);
    return r.contains(pos) && pos.x() >= r.right() - kResizeHandlePx;
}

bool PianoRollEditor::hasTimeOverlap(qint64 start, qint64 end, qint64 ignoreId) const
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return false;
    if (end <= start) return true;
    for (const auto& note : m_project->tracks()[m_activeTrack].notes()) {
        if (note.getId() == ignoreId) continue;
        if (start < note.getEndTick() && end > note.getStartTick()) return true;
    }
    return false;
}

qint64 PianoRollEditor::constrainedMoveDelta(qint64 desiredDelta) const
{
    if (!m_project || m_dragIds.isEmpty()) return desiredDelta;
    qint64 delta = desiredDelta;
    const auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (const auto& note : notes) {
        if (!m_dragOriginalStart.contains(note.getId())) continue;
        const qint64 start = m_dragOriginalStart.value(note.getId());
        const qint64 end = m_dragOriginalEnd.value(note.getId());
        if (start + delta < 0) delta = -start;
        for (const auto& other : notes) {
            if (other.getId() == note.getId() || m_dragOriginalStart.contains(other.getId())) continue;
            if (start + delta < other.getEndTick() && end + delta > other.getStartTick()) {
                if (desiredDelta >= 0) delta = other.getStartTick() - end;
                else delta = other.getEndTick() - start;
            }
        }
    }
    return delta;
}

qint64 PianoRollEditor::availableEndForNote(const Note& note, qint64 desiredEnd) const
{
    const qint64 minimum = note.getStartTick() + 1;
    qint64 end = std::max(minimum, desiredEnd);
    if (!m_project) return end;
    for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
        if (other.getId() == note.getId()) continue;
        if (other.getStartTick() > note.getStartTick() && other.getStartTick() < end) {
            end = other.getStartTick();
        }
    }
    return std::max(minimum, end);
}

void PianoRollEditor::ensureVisibleTick(qint64 tick)
{
    const int x = qRound(SongTime::tickToPixel(tick, m_pxPerBeat));
    const int left = horizontalScrollBar()->value();
    const int right = left + viewport()->width();
    if (x < left) horizontalScrollBar()->setValue(std::max(0, x - 40));
    else if (x > right - 40) horizontalScrollBar()->setValue(std::max(0, x - viewport()->width() + 40));
}

void PianoRollEditor::createNote(const QPoint& pos)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    const qint64 start = tickAtX(pos.x());
    qint64 end = start + kDefaultNoteLength;
    const int midi = midiAtY(pos.y());

    for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
        if (other.getStartTick() > start) end = std::min(end, other.getStartTick());
    }
    end = snapTick(end);
    if (end <= start || hasTimeOverlap(start, end)) return;

    for (auto& other : m_project->tracks()[m_activeTrack].notes()) other.setSelected(false);
    Note note;
    note.setStartTick(start);
    note.setDurationTick(end - start);
    note.setMidiNote(midi);
    note.setLyric(QStringLiteral("a"));
    note.setSelected(true);
    m_project->tracks()[m_activeTrack].addNote(std::move(note));
    sortNotes();
    emit keyboardPreviewRequested(midi);
    emit documentChanged();
    invalidate();
}

void PianoRollEditor::createDraggedNote(qint64 startTick, qint64 endTick, int midi)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    startTick = snapTick(startTick);
    endTick = snapTick(endTick);
    if (endTick < startTick) std::swap(startTick, endTick);
    if (endTick <= startTick) endTick = startTick + std::max<qint64>(1, m_gridTicks);
    if (hasTimeOverlap(startTick, endTick)) return;

    for (auto& other : m_project->tracks()[m_activeTrack].notes()) other.setSelected(false);
    Note note;
    note.setStartTick(startTick);
    note.setDurationTick(endTick - startTick);
    note.setMidiNote(midi);
    note.setLyric(QStringLiteral("a"));
    note.setSelected(true);
    m_project->tracks()[m_activeTrack].addNote(std::move(note));
    sortNotes();
    emit keyboardPreviewRequested(midi);
    emit documentChanged();
    invalidate();
}

void PianoRollEditor::deleteNoteAt(const QPoint& pos)
{
    if (!m_project) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (int i = notes.size() - 1; i >= 0; --i) {
        if (noteRect(notes[i]).contains(pos)) {
            notes.removeAt(i);
            emit documentChanged();
            invalidate();
            return;
        }
    }
}

void PianoRollEditor::splitNoteAt(const QPoint& pos)
{
    Note* note = noteAt(pos);
    if (!note || !m_project) return;
    const qint64 splitTick = tickAtX(pos.x());
    if (splitTick <= note->getStartTick() || splitTick >= note->getEndTick()) return;
    const qint64 oldEnd = note->getEndTick();
    const qint64 firstDuration = splitTick - note->getStartTick();
    note->setDurationTick(firstDuration);
    Note second = note->clone();
    second.setStartTick(splitTick);
    second.setDurationTick(oldEnd - splitTick);
    second.setLyric(note->getLyric());
    second.setSelected(false);
    m_project->tracks()[m_activeTrack].addNote(std::move(second));
    sortNotes();
    emit documentChanged();
    invalidate();
}

void PianoRollEditor::beginLyricEdit(Note& note)
{
    finishLyricEdit(false);
    m_editingLyricId = note.getId();
    m_lyricEditor = new QLineEdit(viewport());
    m_lyricEditor->setText(note.getLyric());
    m_lyricEditor->selectAll();
    const QRect r = noteRect(note).adjusted(2, 1, -2, -1);
    m_lyricEditor->setGeometry(r);
    m_lyricEditor->setFrame(true);
    m_lyricEditor->setFocus();
    connect(m_lyricEditor, &QLineEdit::editingFinished, this, [this] { finishLyricEdit(true); });
}

void PianoRollEditor::finishLyricEdit(bool accept)
{
    if (!m_lyricEditor) return;
    if (accept && m_project && m_activeTrack >= 0 && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_editingLyricId)) {
            note->setLyric(m_lyricEditor->text());
            emit documentChanged();
        }
    }
    delete m_lyricEditor;
    m_lyricEditor = nullptr;
    m_editingLyricId = -1;
    invalidate();
}

void PianoRollEditor::commitRightDragNote()
{
    if (!m_rightDrawing || m_rightDrawId < 0 || !m_project) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    Note* note = nullptr;
    for (auto& n : notes) if (n.getId() == m_rightDrawId) { note = &n; break; }
    if (!note) return;
    if (note->getDurationTick() <= 0) {
        notes.removeIf([this](const Note& n) { return n.getId() == m_rightDrawId; });
    }
    m_rightDrawId = -1;
    m_rightDrawing = false;
}

void PianoRollEditor::sortNotes()
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
        if (a.getStartTick() != b.getStartTick()) return a.getStartTick() < b.getStartTick();
        return a.getMidiNote() < b.getMidiNote();
    });
}

void PianoRollEditor::drawPitchPoint(Note& note, const QPoint& pos)
{
    const QRect r = noteRect(note);
    if (!r.contains(pos)) return;
    const double pxPerMs = std::max(0.0001, static_cast<double>(r.width()) / std::max<qint64>(1, note.getDurationTick()));
    const double localTick = std::clamp(static_cast<double>((pos.x() - r.left()) / pxPerMs), 0.0, static_cast<double>(note.getDurationTick()));
    const double semitoneOffset = (r.center().y() - pos.y()) / kPitchSemitonePixels;
    auto& points = note.getPitchCurve().points();
    auto it = std::find_if(points.begin(), points.end(), [localTick](const PitchPoint& p) {
        return std::abs(p.time - localTick) < 4.0;
    });
    PitchPoint point{localTick, semitoneOffset};
    if (it != points.end()) *it = point;
    else points.push_back(point);
    std::sort(points.begin(), points.end(), [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });
}

void PianoRollEditor::beginPitchDraw(const QPoint& pos)
{
    if (Note* note = noteAt(pos)) {
        m_pitchNoteId = note->getId();
        drawPitchPoint(*note, pos);
        m_pitchDrawing = true;
        emit documentChanged();
        invalidate();
    }
}

void PianoRollEditor::finishPitchDraw()
{
    if (!m_pitchDrawing) return;
    m_pitchDrawing = false;
    m_pitchNoteId = -1;
    emit documentChanged();
    invalidate();
}

void PianoRollEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.fillRect(viewport()->rect(), QColor("#101317"));
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= static_cast<int>(m_project->tracks().size())) return;

    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();
    const int firstMidi = std::clamp(m_topMidi - sy / std::max(1, m_rowHeight), 0, 127);
    const int lastMidi = std::clamp(firstMidi - viewport()->height() / std::max(1, m_rowHeight) - 1, 0, 127);

    if (m_showGrid) {
        for (int midi = lastMidi; midi <= firstMidi; ++midi) {
            const int y = (m_topMidi - midi) * m_rowHeight - sy;
            const bool black = (((midi % 12) == 1) || ((midi % 12) == 3) || ((midi % 12) == 6) || ((midi % 12) == 8) || ((midi % 12) == 10));
            p.fillRect(0, y, viewport()->width(), m_rowHeight, black ? QColor("#0d1014") : QColor("#15191f"));
            p.setPen(QColor("#242a31"));
            p.drawLine(0, y + m_rowHeight - 1, viewport()->width(), y + m_rowHeight - 1);
        }

        const qint64 maxTick = SongTime::pixelToTick(viewport()->width() + sx, m_pxPerBeat);
        for (qint64 tick = 0; tick <= maxTick + m_gridTicks; tick += m_gridTicks) {
            const int x = qRound(SongTime::tickToPixel(tick, m_pxPerBeat)) - sx;
            if (x < 0 || x > viewport()->width()) continue;
            p.setPen(QPen((tick % (m_gridTicks * 4) == 0) ? QColor("#3c4652") : QColor("#252c34"),
                          tick % (m_gridTicks * 4) == 0 ? 1.3 : 1.0));
            p.drawLine(x, 0, x, viewport()->height());
        }
    }

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
            const double noteTicks = static_cast<double>(std::max<qint64>(1, note.getDurationTick()));
            const int samples = std::max(2, r.width() / 3);
            for (int i = 0; i <= samples; ++i) {
                const double u = static_cast<double>(i) / samples;
                const double localTick = u * noteTicks;
                const double offset = note.getPitchCurve().evaluateSmooth(localTick);
                const double x = r.left() + u * r.width();
                const double y = r.center().y() - offset * kPitchSemitonePixels;
                if (i == 0) path.moveTo(x, y);
                else path.lineTo(x, y);
            }
            p.setPen(QPen(QColor("#ffd166"), 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
    }

    const int playX = qRound(SongTime::tickToPixel(m_playhead, m_pxPerBeat)) - sx;
    p.setPen(QPen(QColor("#ff5b6e"), 2));
    p.drawLine(playX, 0, playX, viewport()->height());
}

void PianoRollEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void PianoRollEditor::mousePressEvent(QMouseEvent* event)
{
    if (!m_project) return;
    const QPoint pos = event->pos();

    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && m_tool == EditTool::Pan)) {
        m_panning = true;
        m_lastPos = pos;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton && (m_tool == EditTool::Select || m_tool == EditTool::Pen || m_tool == EditTool::PenPlus || m_tool == EditTool::Pitch || m_tool == EditTool::PitchLine || m_tool == EditTool::Vibrato)) {
        if (m_tool == EditTool::Pitch || m_tool == EditTool::PitchLine) {
            beginPitchDraw(pos);
            return;
        }

        Note* note = noteAt(pos);
        if (!note) {
            if (m_tool == EditTool::Select) {
                m_playhead = tickAtX(pos.x());
                emit requestPlaybackTick(m_playhead);
                emit keyboardPreviewRequested(midiAtY(pos.y()));
                m_playheadDragging = true;
                viewport()->update();
            } else {
                createNote(pos);
            }
            return;
        }

        if (m_tool == EditTool::Pen || m_tool == EditTool::PenPlus) {
            beginLyricEdit(*note);
            return;
        }

        if (m_tool == EditTool::Select) {
            const bool additive = event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::ShiftModifier);
            if (!additive) {
                for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
            }
            note->setSelected(true);
            m_dragging = true;
            m_mouseMovedDuringDrag = false;
            m_lastPos = pos;
            m_dragAnchorTick = tickAtX(pos.x());
            m_dragAnchorPitch = midiAtY(pos.y());
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
            if (isResizeHandle(*note, pos)) {
                m_resizing = true;
                m_resizeId = note->getId();
                m_resizeOriginalEnd = note->getEndTick();
            }
            viewport()->update();
        }
    } else if (event->button() == Qt::RightButton) {
        const qint64 start = tickAtX(pos.x());
        const int midi = midiAtY(pos.y());
        const qint64 end = start + std::max<qint64>(1, m_gridTicks);
        if (!hasTimeOverlap(start, end)) {
            Note n;
            n.setStartTick(start);
            n.setDurationTick(end - start);
            n.setMidiNote(midi);
            n.setLyric(QStringLiteral("a"));
            n.setSelected(true);
            for (auto& other : m_project->tracks()[m_activeTrack].notes()) other.setSelected(false);
            m_rightDrawStart = start;
            m_rightDrawPitch = midi;
            m_project->tracks()[m_activeTrack].addNote(std::move(n));
            sortNotes();
            for (const auto& created : m_project->tracks()[m_activeTrack].notes()) {
                if (created.getStartTick() == start && created.getMidiNote() == midi && created.isSelected()) {
                    m_rightDrawId = created.getId();
                    break;
                }
            }
            m_rightDrawing = m_rightDrawId >= 0;
            emit keyboardPreviewRequested(midi);
            invalidate();
        }
    }
}

void PianoRollEditor::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->pos();
    if (m_panning) {
        const QPoint delta = pos - m_lastPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastPos = pos;
        viewport()->update();
        return;
    }

    if (m_pitchDrawing && m_project) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_pitchNoteId)) {
            drawPitchPoint(*note, pos);
            emit documentChanged();
            invalidate();
        }
        return;
    }

    if (m_playheadDragging && (event->buttons() & Qt::LeftButton)) {
        m_playhead = tickAtX(pos.x());
        emit requestPlaybackTick(m_playhead);
        emit keyboardPreviewRequested(midiAtY(pos.y()));
        viewport()->update();
        return;
    }

    if (m_rightDrawing && m_project && (event->buttons() & Qt::RightButton)) {
        Note* note = m_project->tracks()[m_activeTrack].findNote(m_rightDrawId);
        if (!note) return;
        const qint64 end = tickAtX(pos.x());
        const qint64 clampedEnd = availableEndForNote(*note, std::max(note->getStartTick() + 1, end));
        note->setDurationTick(clampedEnd - note->getStartTick());
        invalidate();
        return;
    }

    if (!m_dragging || !m_project || !(event->buttons() & Qt::LeftButton)) return;
    if ((pos - m_lastPos).manhattanLength() > 1) m_mouseMovedDuringDrag = true;

    const qint64 currentTick = tickAtX(pos.x());
    const qint64 desiredDelta = currentTick - m_dragAnchorTick;
    const int currentPitch = midiAtY(pos.y());
    const int pitchDelta = currentPitch - m_dragAnchorPitch;

    if (m_resizing) {
        Note* note = m_project->tracks()[m_activeTrack].findNote(m_resizeId);
        if (!note) return;
        const qint64 desiredEnd = snapTick(m_resizeOriginalEnd + desiredDelta);
        const qint64 end = availableEndForNote(*note, desiredEnd);
        if (end > note->getStartTick()) {
            note->setDurationTick(end - note->getStartTick());
            emit documentChanged();
            invalidate();
        }
        return;
    }

    const qint64 delta = constrainedMoveDelta(desiredDelta);
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (auto& note : notes) {
        if (!m_dragOriginalStart.contains(note.getId())) continue;
        const qint64 newStart = std::max<qint64>(0, m_dragOriginalStart.value(note.getId()) + delta);
        note.setStartTick(newStart);
        note.setMidiNote(std::clamp(m_dragOriginalPitch.value(note.getId()) + pitchDelta, 0, 127));
    }
    sortNotes();
    m_lastPos = pos;
    emit documentChanged();
    invalidate();
}

void PianoRollEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_pitchDrawing) finishPitchDraw();
        if (m_playheadDragging) {
            m_playheadDragging = false;
            emit requestPlaybackTick(m_playhead);
        }
        if (m_dragging) {
            m_dragging = false;
            m_resizing = false;
            m_resizeId = -1;
            m_dragIds.clear();
            m_dragOriginalStart.clear();
            m_dragOriginalEnd.clear();
            m_dragOriginalPitch.clear();
            if (m_mouseMovedDuringDrag) emit documentChanged();
        }
    } else if (event->button() == Qt::RightButton) {
        commitRightDragNote();
        emit documentChanged();
        invalidate();
    }
}

void PianoRollEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const double factor = event->angleDelta().y() > 0 ? 1.10 : 0.90;
        m_pxPerBeat = std::clamp(m_pxPerBeat * factor, 30.0, 600.0);
        updateScrollRanges();
    } else if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->angleDelta().y());
    } else {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - event->angleDelta().y());
    }
    viewport()->update();
}

void PianoRollEditor::keyPressEvent(QKeyEvent* event)
{
    if (m_lyricEditor) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (!m_project) return;
        auto& notes = m_project->tracks()[m_activeTrack].notes();
        notes.erase(std::remove_if(notes.begin(), notes.end(), [](const Note& n) { return n.isSelected(); }), notes.end());
        emit documentChanged();
        invalidate();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        emit requestPlaybackTick(m_playhead);
        event->accept();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void PianoRollEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_project) return;
    if (Note* note = noteAt(event->pos())) {
        beginLyricEdit(*note);
    } else {
        createNote(event->pos());
    }
}

void PianoRollEditor::updateScrollRanges()
{
    qint64 maxTick = 3840;
    if (m_project) {
        for (const auto& track : m_project->tracks()) {
            for (const auto& note : track.notes()) maxTick = std::max(maxTick, note.getEndTick() + 960);
        }
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(SongTime::tickToPixel(maxTick, m_pxPerBeat)) - viewport()->width()));
    verticalScrollBar()->setRange(0, std::max(0, 128 * m_rowHeight - viewport()->height()));
}

void PianoRollEditor::invalidate()
{
    updateScrollRanges();
    viewport()->update();
}

} // namespace myvocal
