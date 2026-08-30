#include "Editor/PianoRollEditor.h"
#include "Core/SongTime.h"

#include <QDateTime>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

namespace myvocal {

PianoRollEditor::PianoRollEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent)
    , m_project(project)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        emit verticalPitchChanged(
            m_topMidi - verticalScrollBar()->value() / m_rowHeight);
        viewport()->update();
    });

    updateScrollRanges();
}

void PianoRollEditor::setActiveTrack(int index)
{
    if (!m_project || m_project->tracks().isEmpty()) {
        m_activeTrack = 0;
        update();
        return;
    }

    const int trackCount = static_cast<int>(m_project->tracks().size());
    m_activeTrack = std::clamp(index, 0, trackCount - 1);
    update();
}

int PianoRollEditor::activeTrack() const noexcept
{
    return m_activeTrack;
}

void PianoRollEditor::setTool(EditTool tool)
{
    m_tool = tool;
    viewport()->setCursor(
        tool == EditTool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

EditTool PianoRollEditor::tool() const noexcept
{
    return m_tool;
}

void PianoRollEditor::setPlayheadTick(qint64 tick)
{
    m_playhead = std::max<qint64>(0, tick);
    ensureVisibleTick(m_playhead);
    update();
}

qint64 PianoRollEditor::playheadTick() const noexcept
{
    return m_playhead;
}

void PianoRollEditor::setSnapEnabled(bool enabled)
{
    m_snap = enabled;
    update();
}

bool PianoRollEditor::snapEnabled() const noexcept
{
    return m_snap;
}

void PianoRollEditor::setShowGrid(bool value)
{
    m_showGrid = value;
    update();
}

bool PianoRollEditor::showGrid() const noexcept
{
    return m_showGrid;
}

void PianoRollEditor::setGridTicks(qint64 ticks)
{
    m_gridTicks = std::clamp<qint64>(ticks, 1, 3840);
    update();
}

qint64 PianoRollEditor::gridTicks() const noexcept
{
    return m_gridTicks;
}

void PianoRollEditor::setKeyboardPitch(int midi)
{
    if (!m_project) {
        return;
    }

    const int trackCount = static_cast<int>(m_project->tracks().size());
    if (m_activeTrack < 0 || m_activeTrack >= trackCount) {
        return;
    }

    midi = std::clamp(midi, 0, 127);
    auto& notes = m_project->tracks()[m_activeTrack].notes();
    bool changed = false;

    for (auto& note : notes) {
        if (note.isSelected()) {
            note.setMidiNote(midi);
            changed = true;
        }
    }

    if (!changed) {
        Note note(QDateTime::currentMSecsSinceEpoch());
        note.setStartTick(snapTick(m_playhead));
        note.setDurationTick(480);
        note.setMidiNote(midi);
        note.setLyric(QStringLiteral("a"));
        note.setSelected(true);

        for (auto& existing : notes) {
            existing.setSelected(false);
        }

        m_project->tracks()[m_activeTrack].addNote(std::move(note));
        m_playhead += 480;
    }

    emit keyboardPreviewRequested(midi);
    emit documentChanged();
    update();
}

qint64 PianoRollEditor::snapTick(qint64 tick) const
{
    tick = std::max<qint64>(0, tick);
    if (!m_snap || m_gridTicks <= 1) {
        return tick;
    }

    return ((tick + m_gridTicks / 2) / m_gridTicks) * m_gridTicks;
}

qint64 PianoRollEditor::tickAtX(int x) const
{
    const double sceneX = static_cast<double>(
        x + horizontalScrollBar()->value());
    return snapTick(static_cast<qint64>(
        SongTime::pixelToTick(sceneX, m_pxPerBeat)));
}

int PianoRollEditor::midiAtY(int y) const
{
    const int sceneY = y + verticalScrollBar()->value();
    return std::clamp(
        m_topMidi - sceneY / std::max(1, m_rowHeight),
        0,
        127);
}

QRect PianoRollEditor::noteRect(const Note& note) const
{
    const int x = qRound(
        SongTime::tickToPixel(note.getStartTick(), m_pxPerBeat))
        - horizontalScrollBar()->value();
    const int width = std::max(
        4,
        qRound(SongTime::tickToPixel(
            note.getDurationTick(), m_pxPerBeat)));
    const int y = (m_topMidi - note.getMidiNote()) * m_rowHeight
        - verticalScrollBar()->value();

    return QRect(x, y, width, std::max(1, m_rowHeight - 2));
}

Note* PianoRollEditor::noteAt(const QPoint& pos)
{
    if (!m_project) {
        return nullptr;
    }

    const int trackCount = static_cast<int>(m_project->tracks().size());
    if (m_activeTrack < 0 || m_activeTrack >= trackCount) {
        return nullptr;
    }

    auto& notes = m_project->tracks()[m_activeTrack].notes();
    for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
        if (noteRect(*it).contains(pos)) {
            return &*it;
        }
    }

    return nullptr;
}

bool PianoRollEditor::isResizeHandle(const Note& note, const QPoint& pos) const
{
    const QRect rect = noteRect(note);
    return rect.contains(pos) && pos.x() >= rect.right() - 7;
}

void PianoRollEditor::paintEvent(QPaintEvent*)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), QColor("#17191c"));

    const int scrollY = verticalScrollBar()->value();
    const int firstMidi = std::clamp(
        m_topMidi - (scrollY + height()) / m_rowHeight - 1,
        0,
        127);
    const int lastMidi = std::clamp(
        m_topMidi - scrollY / m_rowHeight + 1,
        0,
        127);

    const QStringList names = {
        QStringLiteral("C"), QStringLiteral("C#"), QStringLiteral("D"),
        QStringLiteral("D#"), QStringLiteral("E"), QStringLiteral("F"),
        QStringLiteral("F#"), QStringLiteral("G"), QStringLiteral("G#"),
        QStringLiteral("A"), QStringLiteral("A#"), QStringLiteral("B")
    };

    for (int midi = firstMidi; midi <= lastMidi; ++midi) {
        const int y = (m_topMidi - midi) * m_rowHeight - scrollY;
        const bool black = names.at(midi % 12).contains('#');

        if (midi % 12 == 0) {
            painter.fillRect(
                0, y, viewport()->width(), m_rowHeight,
                QColor("#20242a"));
        } else if (black) {
            painter.fillRect(
                0, y, viewport()->width(), m_rowHeight,
                QColor("#191b1f"));
        }

        painter.setPen(
            QColor(midi % 12 == 0 ? "#4d535c" : "#292e34"));
        painter.drawLine(0, y, viewport()->width(), y);
    }

    if (m_showGrid) {
        const qint64 startTick = static_cast<qint64>(
            SongTime::pixelToTick(
                horizontalScrollBar()->value(), m_pxPerBeat));
        const qint64 endTick = static_cast<qint64>(
            SongTime::pixelToTick(
                horizontalScrollBar()->value() + viewport()->width(),
                m_pxPerBeat));
        const qint64 firstGrid = std::max<qint64>(
            0, startTick / m_gridTicks - 1) * m_gridTicks;

        for (qint64 tick = firstGrid;
             tick <= endTick + m_gridTicks;
             tick += m_gridTicks) {
            const double x = SongTime::tickToPixel(tick, m_pxPerBeat)
                - horizontalScrollBar()->value();
            if (x < 0 || x > viewport()->width()) {
                continue;
            }

            const qint64 barTicks = m_project
                ? static_cast<qint64>(m_project->ppq() * 4)
                : 1920;
            const bool bar = barTicks > 0 && tick % barTicks == 0;
            painter.setPen(QPen(
                bar ? QColor("#555b64") : QColor("#30353b"),
                bar ? 1.5 : 1));
            painter.drawLine(
                QPointF(x, 0),
                QPointF(x, viewport()->height()));
        }
    }

    if (m_project) {
        const int trackCount = static_cast<int>(m_project->tracks().size());
        if (m_activeTrack >= 0 && m_activeTrack < trackCount) {
            for (const auto& note : m_project->tracks()[m_activeTrack].notes()) {
                const QRect rect = noteRect(note);
                if (!rect.intersects(viewport()->rect())) {
                    continue;
                }

                painter.setBrush(
                    note.isSelected()
                        ? QColor("#4f8cf7")
                        : QColor("#3c6fae"));
                painter.setPen(
                    note.isSelected()
                        ? QColor("#b9d2ff")
                        : QColor("#668fbe"));
                painter.drawRoundedRect(rect, 3, 3);

                painter.setPen(Qt::white);
                painter.drawText(
                    rect.adjusted(6, 0, -6, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    note.getLyric());

                if (note.isSelected()) {
                    painter.setPen(QColor("#dbe8ff"));
                    painter.drawLine(
                        rect.right() - 4, rect.top() + 3,
                        rect.right() - 4, rect.bottom() - 3);
                }
            }
        }
    }

    const int playheadX = qRound(
        SongTime::tickToPixel(m_playhead, m_pxPerBeat))
        - horizontalScrollBar()->value();
    painter.setPen(QPen(QColor("#ff5b6e"), 2));
    painter.drawLine(playheadX, 0, playheadX, viewport()->height());
}

void PianoRollEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void PianoRollEditor::mousePressEvent(QMouseEvent* event)
{
    if (!m_project || m_project->tracks().isEmpty()) {
        return;
    }

    setFocus();
    m_lastPos = event->pos();

    if (event->button() == Qt::MiddleButton || m_tool == EditTool::Pan) {
        m_panning = true;
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (m_tool == EditTool::Pen || m_tool == EditTool::PenPlus) {
        createNote(event->pos());
        return;
    }

    if (m_tool == EditTool::Eraser) {
        deleteNoteAt(event->pos());
        return;
    }

    if (m_tool == EditTool::Knife) {
        splitNoteAt(event->pos());
        return;
    }

    if (m_tool == EditTool::Zoom) {
        const double factor = 1.25;
        const qint64 before = SongTime::pixelToTick(
            event->pos().x() + horizontalScrollBar()->value(),
            m_pxPerBeat);
        m_pxPerBeat = std::clamp(m_pxPerBeat * factor, 30.0, 600.0);
        const int targetX = qRound(
            SongTime::tickToPixel(before, m_pxPerBeat));
        horizontalScrollBar()->setValue(
            std::max(0, targetX - event->pos().x()));
        updateScrollRanges();
        update();
        return;
    }

    if (m_tool == EditTool::Select) {
        Note* note = noteAt(event->pos());
        if (!note) {
            for (auto& n : m_project->tracks()[m_activeTrack].notes()) {
                n.setSelected(false);
            }
            update();
            return;
        }

        if (!(event->modifiers() & Qt::ControlModifier)) {
            for (auto& n : m_project->tracks()[m_activeTrack].notes()) {
                n.setSelected(false);
            }
        }

        note->setSelected(
            !note->isSelected()
            || !(event->modifiers() & Qt::ControlModifier));

        m_dragIds.clear();
        for (const auto& n : m_project->tracks()[m_activeTrack].notes()) {
            if (n.isSelected()) {
                m_dragIds.push_back(n.getId());
            }
        }

        m_dragging = true;
        m_resizing = isResizeHandle(*note, event->pos());
        m_resizeId = m_resizing ? note->getId() : -1;
        m_resizeOriginalEnd = note->getEndTick();
        m_dragStartTick = tickAtX(event->pos().x());
        m_dragStartPitch = midiAtY(event->pos().y());
        update();
    }
}

void PianoRollEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_project) {
        return;
    }

    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPos;
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - delta.y());
        m_lastPos = event->pos();
        return;
    }

    if (m_dragging) {
        const int trackCount = static_cast<int>(m_project->tracks().size());
        if (m_activeTrack >= 0 && m_activeTrack < trackCount) {
            const qint64 currentTick = tickAtX(event->pos().x());
            const qint64 deltaTick = currentTick - m_dragStartTick;
            const int currentPitch = midiAtY(event->pos().y());
            const int deltaPitch = currentPitch - m_dragStartPitch;

            if (m_resizing) {
                if (Note* note = m_project->tracks()[m_activeTrack]
                                    .findNote(m_resizeId)) {
                    const qint64 newEnd = std::max(
                        note->getStartTick() + 1,
                        m_resizeOriginalEnd + deltaTick);
                    note->setDurationTick(
                        newEnd - note->getStartTick());
                }
            } else {
                for (qint64 id : m_dragIds) {
                    if (Note* note = m_project->tracks()[m_activeTrack]
                                        .findNote(id)) {
                        note->setStartTick(
                            std::max<qint64>(
                                0, note->getStartTick() + deltaTick));
                        note->setMidiNote(
                            std::clamp(
                                note->getMidiNote() + deltaPitch,
                                0,
                                127));
                    }
                }
            }

            m_dragStartTick = currentTick;
            m_dragStartPitch = currentPitch;
            emit documentChanged();
            update();
        }
    }

    m_lastPos = event->pos();
}

void PianoRollEditor::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
    m_resizing = false;
    m_panning = false;
    m_resizeId = -1;

    if (m_tool == EditTool::Pan) {
        viewport()->setCursor(Qt::OpenHandCursor);
    }
}

void PianoRollEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? 1.12 : 0.89;
        const qint64 anchorTick = SongTime::pixelToTick(
            event->position().x() + horizontalScrollBar()->value(),
            m_pxPerBeat);
        m_pxPerBeat = std::clamp(m_pxPerBeat * factor, 30.0, 600.0);
        const int anchorX = qRound(
            SongTime::tickToPixel(anchorTick, m_pxPerBeat));
        horizontalScrollBar()->setValue(
            std::max(0, anchorX - qRound(event->position().x())));
        updateScrollRanges();
        update();
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value()
            - event->angleDelta().y() / 2);
        return;
    }

    verticalScrollBar()->setValue(
        verticalScrollBar()->value()
        - event->angleDelta().y() / 2);
}

void PianoRollEditor::keyPressEvent(QKeyEvent* event)
{
    if (!m_project) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }

    if (event->matches(QKeySequence::Delete)
        || event->key() == Qt::Key_Backspace) {
        const int trackCount = static_cast<int>(m_project->tracks().size());
        if (m_activeTrack < 0 || m_activeTrack >= trackCount) {
            return;
        }

        auto& notes = m_project->tracks()[m_activeTrack].notes();
        for (int i = notes.size() - 1; i >= 0; --i) {
            if (notes[i].isSelected()) {
                notes.removeAt(i);
            }
        }

        emit documentChanged();
        update();
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void PianoRollEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_project) {
        return;
    }

    Note* note = noteAt(event->pos());
    if (!note) {
        return;
    }

    bool ok = false;
    const QString text = QInputDialog::getText(
        this,
        QStringLiteral("Edit Lyric"),
        QStringLiteral("Lyric:"),
        QLineEdit::Normal,
        note->getLyric(),
        &ok);

    if (ok) {
        note->setLyric(text);
        emit noteDoubleClicked(note->getId());
        emit documentChanged();
        update();
    }
}

void PianoRollEditor::createNote(const QPoint& pos)
{
    if (!m_project) {
        return;
    }

    const int trackCount = static_cast<int>(m_project->tracks().size());
    if (m_activeTrack < 0 || m_activeTrack >= trackCount) {
        return;
    }

    const qint64 start = tickAtX(pos.x());
    const int midi = std::clamp(midiAtY(pos.y()), 0, 127);

    for (auto& note : m_project->tracks()[m_activeTrack].notes()) {
        note.setSelected(false);
    }

    Note note(QDateTime::currentMSecsSinceEpoch());
    note.setStartTick(start);
    note.setDurationTick(480);
    note.setMidiNote(midi);
    note.setLyric(QStringLiteral("a"));
    note.setSelected(true);
    m_project->tracks()[m_activeTrack].addNote(std::move(note));

    updateScrollRanges();
    emit documentChanged();
    update();
}

void PianoRollEditor::deleteNoteAt(const QPoint& pos)
{
    if (!m_project) {
        return;
    }

    if (Note* note = noteAt(pos)) {
        const qint64 id = note->getId();
        m_project->tracks()[m_activeTrack].removeNoteById(id);
        emit documentChanged();
        update();
    }
}

void PianoRollEditor::splitNoteAt(const QPoint& pos)
{
    if (!m_project) {
        return;
    }

    Note* note = noteAt(pos);
    if (!note) {
        return;
    }

    const qint64 tick = tickAtX(pos.x());
    if (tick <= note->getStartTick() || tick >= note->getEndTick()) {
        return;
    }

    Note right = note->clone();
    right.setStartTick(tick);
    right.setDurationTick(note->getEndTick() - tick);

    note->setDurationTick(tick - note->getStartTick());
    right.setSelected(false);
    m_project->tracks()[m_activeTrack].addNote(std::move(right));

    emit documentChanged();
    update();
}

void PianoRollEditor::ensureVisibleTick(qint64 tick)
{
    const int x = qRound(SongTime::tickToPixel(tick, m_pxPerBeat));
    const int viewWidth = viewport()->width();
    const int left = horizontalScrollBar()->value();
    const int right = left + viewWidth;

    if (x < left) {
        horizontalScrollBar()->setValue(std::max(0, x - 32));
    } else if (x > right) {
        horizontalScrollBar()->setValue(std::max(0, x - viewWidth + 32));
    }
}

void PianoRollEditor::updateScrollRanges()
{
    const int viewWidth = std::max(1, viewport()->width());
    const int viewHeight = std::max(1, viewport()->height());
    const int sceneWidth = std::max(
        20000,
        qRound(SongTime::tickToPixel(480 * 256, m_pxPerBeat)));
    const int sceneHeight = 128 * m_rowHeight;

    horizontalScrollBar()->setPageStep(viewWidth);
    horizontalScrollBar()->setRange(
        0, std::max(0, sceneWidth - viewWidth));
    verticalScrollBar()->setPageStep(viewHeight);
    verticalScrollBar()->setRange(
        0, std::max(0, sceneHeight - viewHeight));
}

void PianoRollEditor::invalidate()
{
    updateScrollRanges();
    viewport()->update();
}

}
