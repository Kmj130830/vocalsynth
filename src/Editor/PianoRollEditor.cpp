#include "Editor/PianoRollEditor.h"
#include "Core/SongTime.h"
#include "UI/EditorFrame.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

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
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
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
    m_resizeId = -1;
    m_pitchNoteId = -1;
    clearNoteRenderCache();
    viewport()->update();
}

int PianoRollEditor::activeTrack() const noexcept { return m_activeTrack; }

void PianoRollEditor::setTool(EditTool tool)
{
    m_tool = tool;
    if (tool == EditTool::Pan) viewport()->setCursor(Qt::OpenHandCursor);
    else viewport()->setCursor(Qt::ArrowCursor);
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
    const int endX = qRound(SongTime::tickToPixel(note.getEndTick(), m_pxPerBeat)) - horizontalScrollBar()->value();
    const int y = (m_topMidi - note.getMidiNote()) * m_rowHeight - verticalScrollBar()->value();
    return QRect(x, y + 1, std::max(2, endX - x), std::max(2, m_rowHeight - 2));
}

Note* PianoRollEditor::noteAt(const QPoint& pos)
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return nullptr;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
        if (noteRect(*it).contains(pos)) return &(*it);
    }
    return nullptr;
}

bool PianoRollEditor::isResizeHandle(const Note& note, const QPoint& pos) const
{
    const QRect r = noteRect(note);
    return r.width() >= kResizeHandlePx && r.contains(pos) && pos.x() >= r.right() - kResizeHandlePx;
}

bool PianoRollEditor::hasTimeOverlap(qint64 start, qint64 end, qint64 ignoreId) const
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return false;
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
    const auto& notes = m_project->tracks()[m_activeTrack].notes();
    qint64 delta = desiredDelta;
    for (const auto& note : notes) {
        if (!m_dragOriginalStart.contains(note.getId())) continue;
        const qint64 start = m_dragOriginalStart.value(note.getId());
        const qint64 end = m_dragOriginalEnd.value(note.getId());
        if (start + delta < 0) delta = -start;
        for (const auto& other : notes) {
            if (m_dragOriginalStart.contains(other.getId()) || other.getId() == note.getId()) continue;
            if (start + delta < other.getEndTick() && end + delta > other.getStartTick()) {
                delta = desiredDelta >= 0 ? other.getStartTick() - end : other.getEndTick() - start;
            }
        }
    }
    return delta;
}

qint64 PianoRollEditor::availableEndForNote(const Note& note, qint64 desiredEnd) const
{
    const qint64 minimum = note.getStartTick() + 1;
    qint64 end = std::max(minimum, desiredEnd);
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return end;
    for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
        if (other.getId() == note.getId()) continue;
        if (other.getStartTick() > note.getStartTick() && other.getStartTick() < end) end = other.getStartTick();
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
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return;
    const qint64 start = tickAtX(pos.x());
    const int midi = midiAtY(pos.y());
    qint64 end = start + kDefaultNoteLength;
    for (const auto& other : m_project->tracks()[m_activeTrack].notes()) {
        if (other.getStartTick() > start) end = std::min(end, other.getStartTick());
    }
    if (m_snap) end = snapTick(end);
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
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return;
    startTick = snapTick(startTick);
    endTick = snapTick(endTick);
    if (endTick < startTick) std::swap(startTick, endTick);
    if (endTick <= startTick) endTick = startTick + std::max<qint64>(1, m_gridTicks);
    if (hasTimeOverlap(startTick, endTick)) return;
    for (auto& other : m_project->tracks()[m_activeTrack].notes()) other.setSelected(false);
    Note note;
    note.setStartTick(startTick);
    note.setDurationTick(endTick - startTick);
    note.setMidiNote(std::clamp(midi, 0, 127));
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
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (int i = notes.size() - 1; i >= 0; --i) {
        if (!noteRect(notes.at(i)).contains(pos)) continue;
        notes.removeAt(i);
        emit documentChanged();
        invalidate();
        return;
    }
}

void PianoRollEditor::splitNoteAt(const QPoint& pos)
{
    if (!m_project) return;
    Note* note = noteAt(pos);
    if (!note) return;
    const qint64 splitTick = tickAtX(pos.x());
    if (splitTick <= note->getStartTick() || splitTick >= note->getEndTick()) return;
    const qint64 end = note->getEndTick();
    note->setDurationTick(splitTick - note->getStartTick());
    Note second = note->clone();
    second.setStartTick(splitTick);
    second.setDurationTick(end - splitTick);
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
    m_lyricEditor->setGeometry(noteRect(note).adjusted(2, 1, -2, -1));
    m_lyricEditor->setFocus();
    connect(m_lyricEditor, &QLineEdit::editingFinished, this, [this] { finishLyricEdit(true); });
}

void PianoRollEditor::finishLyricEdit(bool accept)
{
    if (!m_lyricEditor) return;
    if (accept && m_project && m_activeTrack >= 0 && m_activeTrack < m_project->tracks().size()) {
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
    if (!m_rightDrawing || !m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    bool found = false;
    for (const auto& note : notes) if (note.getId() == m_rightDrawId) { found = true; break; }
    if (!found) m_rightDrawId = -1;
    m_rightDrawing = false;
}

void PianoRollEditor::sortNotes()
{
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return;
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
        return a.getStartTick() == b.getStartTick() ? a.getMidiNote() < b.getMidiNote() : a.getStartTick() < b.getStartTick();
    });
}

void PianoRollEditor::drawPitchPoint(Note& note, const QPoint& pos)
{
    const QRect r = noteRect(note);
    if (!r.contains(pos) || !m_project) return;
    const double u = std::clamp(static_cast<double>(pos.x() - r.left()) / std::max(1, r.width()), 0.0, 1.0);
    const qint64 localTick = static_cast<qint64>(std::llround(u * note.getDurationTick()));
    const double startMs = m_project->tempoMap().tickToSeconds(static_cast<double>(note.getStartTick()), m_project->ppq()) * 1000.0;
    const double pointMs = m_project->tempoMap().tickToSeconds(static_cast<double>(note.getStartTick() + localTick), m_project->ppq()) * 1000.0;
    const double localMs = std::max(0.0, pointMs - startMs);
    const double semitoneOffset = (r.center().y() - pos.y()) / kPitchSemitonePixels;
    auto& points = note.getPitchCurve().points();
    auto it = std::find_if(points.begin(), points.end(), [localMs](const PitchPoint& point) { return std::abs(point.time - localMs) < 8.0; });
    const PitchPoint point{localMs, std::clamp(semitoneOffset, -24.0, 24.0)};
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
        viewport()->update();
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

QByteArray PianoRollEditor::noteVisualKey(const Note& note) const
{
    QByteArray key;
    key.reserve(96);
    key.append(QByteArray::number(note.getStartTick()));
    key.append(':');
    key.append(QByteArray::number(note.getDurationTick()));
    key.append(':');
    key.append(QByteArray::number(note.getMidiNote()));
    key.append(':');
    key.append(note.isSelected() ? '1' : '0');
    key.append(':');
    key.append(note.getLyric().toUtf8());
    key.append(':');
    for (const auto& point : note.getPitchCurve().points()) {
        key.append(QByteArray::number(point.time, 'f', 3));
        key.append(',');
        key.append(QByteArray::number(point.semitoneOffset, 'f', 3));
        key.append(';');
    }
    return key;
}

QPixmap PianoRollEditor::renderNotePixmap(const Note& note) const
{
    const QRect scene = QRect(0, 0, std::max(2, qRound(SongTime::tickToPixel(note.getEndTick() - note.getStartTick(), m_pxPerBeat))), std::max(2, m_rowHeight - 2));
    QPixmap pixmap(scene.size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = pixmap.rect();
    painter.setBrush(note.isSelected() ? QColor("#284a68") : QColor("#242833"));
    painter.setPen(note.isSelected() ? QColor("#6fa8e6") : QColor("#4b5665"));
    painter.drawRoundedRect(r.adjusted(0, 0, -1, -1), 3, 3);

    painter.setPen(note.isSelected() ? QColor("#d8ebff") : QColor("#aeb6c1"));
    if (r.width() > 14) painter.drawText(r.adjusted(5, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter, note.getLyric());

    if (note.isSelected()) {
        painter.setPen(QColor("#cfe7ff"));
        painter.drawLine(r.right() - 2, r.top() + 3, r.right() - 2, r.bottom() - 3);
    }

    const auto& points = note.getPitchCurve().points();
    if (!points.empty()) {
        const double durationMs = std::max(0.001, m_project->tempoMap().tickToSeconds(static_cast<double>(note.getDurationTick()), m_project->ppq()) * 1000.0);
        const int samples = std::max(2, r.width() / 3);
        QPainterPath path;
        for (int i = 0; i <= samples; ++i) {
            const double u = static_cast<double>(i) / samples;
            const double localMs = u * durationMs;
            const double offset = note.getPitchCurve().evaluateSmooth(localMs);
            const double x = u * r.width();
            const double y = r.center().y() - offset * kPitchSemitonePixels;
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        painter.setPen(QPen(QColor("#d5a34d"), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
    return pixmap;
}

void PianoRollEditor::clearNoteRenderCache()
{
    m_noteCacheKeys.clear();
    m_noteCachePixmaps.clear();
}

void PianoRollEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, false);
    EditorFrame::drawBackground(p, viewport()->rect());
    if (!m_project || m_activeTrack < 0 || m_activeTrack >= m_project->tracks().size()) return;

    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();
    const int topMidi = std::clamp(m_topMidi - sy / std::max(1, m_rowHeight), 0, 127);
    const int bottomMidi = std::clamp(m_topMidi - (sy + viewport()->height()) / std::max(1, m_rowHeight), 0, 127);

    for (int midi = bottomMidi; midi <= topMidi; ++midi) {
        const int y = (m_topMidi - midi) * m_rowHeight - sy;
        const bool black = ((midi % 12) == 1 || (midi % 12) == 3 || (midi % 12) == 6 || (midi % 12) == 8 || (midi % 12) == 10);
        p.fillRect(0, y, viewport()->width(), m_rowHeight, black ? QColor("#14161a") : QColor("#1b1e23"));
        p.setPen(QColor("#292e35"));
        p.drawLine(0, y + m_rowHeight - 1, viewport()->width(), y + m_rowHeight - 1);
    }

    if (m_showGrid) {
        const qint64 maxTick = SongTime::pixelToTick(viewport()->width() + sx, m_pxPerBeat);
        for (qint64 tick = 0; tick <= maxTick + m_gridTicks; tick += m_gridTicks) {
            const int x = qRound(SongTime::tickToPixel(tick, m_pxPerBeat)) - sx;
            if (x < 0 || x > viewport()->width()) continue;
            const bool bar = tick % std::max<qint64>(1, m_gridTicks * 4) == 0;
            p.setPen(QPen(bar ? QColor("#454b54") : QColor("#2c3239"), bar ? 1.1 : 1.0));
            p.drawLine(x, 0, x, viewport()->height());
        }
    }

    const auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (const auto& note : notes) {
        const QRect r = noteRect(note);
        if (!r.intersects(viewport()->rect())) continue;
        const QByteArray key = noteVisualKey(note);
        auto keyIt = m_noteCacheKeys.constFind(note.getId());
        auto pixIt = m_noteCachePixmaps.constFind(note.getId());
        if (keyIt == m_noteCacheKeys.cend() || pixIt == m_noteCachePixmaps.cend() || keyIt.value() != key || pixIt.value().size() != QSize(r.width(), r.height())) {
            m_noteCacheKeys.insert(note.getId(), key);
            m_noteCachePixmaps.insert(note.getId(), renderNotePixmap(note));
            pixIt = m_noteCachePixmaps.constFind(note.getId());
        }
        p.drawPixmap(r.topLeft(), pixIt.value());
    }

    if (m_noteCacheKeys.size() > notes.size() + 64) {
        QHash<qint64, QByteArray> keys;
        QHash<qint64, QPixmap> pixmaps;
        keys.reserve(notes.size());
        pixmaps.reserve(notes.size());
        for (const auto& note : notes) {
            if (!m_noteCacheKeys.contains(note.getId())) continue;
            keys.insert(note.getId(), m_noteCacheKeys.value(note.getId()));
            pixmaps.insert(note.getId(), m_noteCachePixmaps.value(note.getId()));
        }
        m_noteCacheKeys.swap(keys);
        m_noteCachePixmaps.swap(pixmaps);
    }

    const int playX = qRound(SongTime::tickToPixel(m_playhead, m_pxPerBeat)) - sx;
    p.setPen(QPen(QColor("#dd6172"), 1.5));
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

    if (event->button() == Qt::LeftButton && (m_tool == EditTool::Pitch || m_tool == EditTool::PitchLine)) {
        beginPitchDraw(pos);
        return;
    }

    if (event->button() == Qt::LeftButton && (m_tool == EditTool::Select || m_tool == EditTool::Pen || m_tool == EditTool::PenPlus || m_tool == EditTool::Eraser || m_tool == EditTool::Knife || m_tool == EditTool::Vibrato)) {
        Note* note = noteAt(pos);
        if (!note) {
            if (m_tool == EditTool::Select) {
                m_playhead = tickAtX(pos.x());
                emit requestPlaybackTick(m_playhead);
                m_playheadDragging = true;
                viewport()->update();
            } else if (m_tool == EditTool::Eraser) {
            } else {
                createNote(pos);
            }
            return;
        }

        if ((m_tool == EditTool::Select || m_tool == EditTool::Pen || m_tool == EditTool::PenPlus) && isResizeHandle(*note, pos)) {
            for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
            note->setSelected(true);
            m_dragging = true;
            m_resizing = true;
            m_resizeId = note->getId();
            m_resizeOriginalEnd = note->getEndTick();
            m_mouseMovedDuringDrag = false;
            m_dragAnchorTick = tickAtX(pos.x());
            m_lastPos = pos;
            return;
        }

        if (m_tool == EditTool::Eraser) {
            deleteNoteAt(pos);
            return;
        }
        if (m_tool == EditTool::Knife) {
            splitNoteAt(pos);
            return;
        }
        if (m_tool == EditTool::Pen || m_tool == EditTool::PenPlus) {
            beginLyricEdit(*note);
            return;
        }
        if (m_tool == EditTool::Vibrato) {
            note->setVibrato(Vibrato{});
            emit documentChanged();
            invalidate();
            return;
        }

        const bool additive = event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::ShiftModifier);
        if (!additive) for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
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
        viewport()->update();
    } else if (event->button() == Qt::RightButton) {
        const qint64 start = tickAtX(pos.x());
        const int midi = midiAtY(pos.y());
        const qint64 end = start + std::max<qint64>(1, m_gridTicks);
        if (!hasTimeOverlap(start, end)) {
            for (auto& n : m_project->tracks()[m_activeTrack].notes()) n.setSelected(false);
            Note note;
            note.setStartTick(start);
            note.setDurationTick(end - start);
            note.setMidiNote(midi);
            note.setLyric(QStringLiteral("a"));
            note.setSelected(true);
            m_project->tracks()[m_activeTrack].addNote(std::move(note));
            sortNotes();
            for (const auto& n : m_project->tracks()[m_activeTrack].notes()) {
                if (n.getStartTick() == start && n.getMidiNote() == midi && n.isSelected()) {
                    m_rightDrawId = n.getId();
                    break;
                }
            }
            m_rightDrawStart = start;
            m_rightDrawPitch = midi;
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
        return;
    }

    if (m_pitchDrawing && m_project) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_pitchNoteId)) {
            drawPitchPoint(*note, pos);
            viewport()->update();
        }
        return;
    }

    if (m_playheadDragging && (event->buttons() & Qt::LeftButton)) {
        m_playhead = tickAtX(pos.x());
        emit requestPlaybackTick(m_playhead);
        viewport()->update();
        return;
    }

    if (m_rightDrawing && m_project && (event->buttons() & Qt::RightButton)) {
        Note* note = m_project->tracks()[m_activeTrack].findNote(m_rightDrawId);
        if (!note) return;
        const qint64 end = tickAtX(pos.x());
        const qint64 clampedEnd = availableEndForNote(*note, std::max(note->getStartTick() + 1, end));
        note->setDurationTick(clampedEnd - note->getStartTick());
        viewport()->update();
        return;
    }

    if (!m_dragging || !m_project || !(event->buttons() & Qt::LeftButton)) return;
    if ((pos - m_lastPos).manhattanLength() > 1) m_mouseMovedDuringDrag = true;

    const qint64 currentTick = tickAtX(pos.x());
    const qint64 desiredDelta = currentTick - m_dragAnchorTick;
    if (m_resizing) {
        if (Note* note = m_project->tracks()[m_activeTrack].findNote(m_resizeId)) {
            const qint64 desiredEnd = snapTick(m_resizeOriginalEnd + desiredDelta);
            const qint64 end = availableEndForNote(*note, desiredEnd);
            if (end > note->getStartTick()) note->setDurationTick(end - note->getStartTick());
        }
        viewport()->update();
        return;
    }

    const int pitchDelta = midiAtY(pos.y()) - m_dragAnchorPitch;
    const qint64 delta = constrainedMoveDelta(desiredDelta);
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (auto& note : notes) {
        if (!m_dragOriginalStart.contains(note.getId())) continue;
        note.setStartTick(std::max<qint64>(0, m_dragOriginalStart.value(note.getId()) + delta));
        note.setMidiNote(std::clamp(m_dragOriginalPitch.value(note.getId()) + pitchDelta, 0, 127));
    }
    sortNotes();
    viewport()->update();
}

void PianoRollEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || m_panning) {
        m_panning = false;
        setTool(m_tool);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_pitchDrawing) finishPitchDraw();
        if (m_playheadDragging) {
            m_playheadDragging = false;
            emit requestPlaybackTick(m_playhead);
        }
        if (m_dragging) {
            if (m_mouseMovedDuringDrag) emit documentChanged();
            m_dragging = false;
            m_resizing = false;
            m_resizeId = -1;
            m_dragIds.clear();
            m_dragOriginalStart.clear();
            m_dragOriginalEnd.clear();
            m_dragOriginalPitch.clear();
            invalidate();
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
        clearNoteRenderCache();
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
    if (event->matches(QKeySequence::SelectAll)) {
        if (m_project && m_activeTrack >= 0 && m_activeTrack < m_project->tracks().size()) {
            for (auto& note : m_project->tracks()[m_activeTrack].notes()) note.setSelected(true);
            viewport()->update();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_project && m_activeTrack >= 0 && m_activeTrack < m_project->tracks().size()) {
            auto& notes = m_project->tracks()[m_activeTrack].notes();
            notes.erase(std::remove_if(notes.begin(), notes.end(), [](const Note& note) { return note.isSelected(); }), notes.end());
            emit documentChanged();
            invalidate();
        }
        event->accept();
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
    if (Note* note = noteAt(event->pos())) beginLyricEdit(*note);
    else createNote(event->pos());
}

void PianoRollEditor::updateScrollRanges()
{
    qint64 maxTick = 3840;
    if (m_project) {
        for (const auto& track : m_project->tracks()) for (const auto& note : track.notes()) maxTick = std::max(maxTick, note.getEndTick() + 960);
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(SongTime::tickToPixel(maxTick, m_pxPerBeat)) - viewport()->width()));
    verticalScrollBar()->setRange(0, std::max(0, 128 * m_rowHeight - viewport()->height()));
}

void PianoRollEditor::invalidate()
{
    updateScrollRanges();
    viewport()->update();
}

}