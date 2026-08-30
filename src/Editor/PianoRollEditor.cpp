#include "Editor/PianoRollEditor.h"
#include "Core/SongTime.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

namespace myvocal {

    PianoRollEditor::PianoRollEditor(Project* p, QWidget* parent)
        : QAbstractScrollArea(parent)
        , m_project(p)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);

            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        verticalScrollBar()->setRange(
            0,
            std::max(0, 128 * m_rowHeight - height())
        );

        horizontalScrollBar()->setRange(0, 20000);

    }

    void PianoRollEditor::setActiveTrack(int i)
    {
        if (!m_project) {
            m_activeTrack = 0;
            return;
        }

            const int count =
            static_cast<int>(m_project->tracks().size());

        if (count <= 0) {
            m_activeTrack = 0;
            return;
        }

        m_activeTrack = std::clamp(i, 0, count - 1);
        invalidate();

    }

    void PianoRollEditor::setTool(EditTool t)
    {
        m_tool = t;
    }

    EditTool PianoRollEditor::tool() const noexcept
    {
        return m_tool;
    }

    void PianoRollEditor::setPlayheadTick(qint64 t)
    {
        m_playhead = std::max<qint64>(0, t);
        ensureVisibleTick(m_playhead);
        invalidate();
    }

    qint64 PianoRollEditor::playheadTick() const noexcept
    {
        return m_playhead;
    }

    void PianoRollEditor::setSnapEnabled(bool v)
    {
        m_snap = v;
        update();
    }

    bool PianoRollEditor::snapEnabled() const noexcept
    {
        return m_snap;
    }

    void PianoRollEditor::setShowGrid(bool v)
    {
        m_showGrid = v;
        update();
    }

    qint64 PianoRollEditor::snapTick(qint64 t) const
    {
        t = std::max<qint64>(0, t);

            if (!m_snap)
                return t;

        constexpr qint64 grid = 60;

        return ((t + grid / 2) / grid) * grid;

    }

    qint64 PianoRollEditor::tickAtX(int x) const
    {
        const qint64 scrollX =
            static_cast<qint64>(horizontalScrollBar()->value());

            return snapTick(
                static_cast<qint64>(
                    SongTime::pixelToTick(
                        static_cast<double>(x) + scrollX,
                        m_pxPerBeat
                    )
                    )
            );

    }

    int PianoRollEditor::midiAtY(int y) const
    {
        const int scrollY = verticalScrollBar()->value();

            return m_topMidi -
            ((y + scrollY) / std::max(1, m_rowHeight));

    }

    QRect PianoRollEditor::noteRect(const Note& n) const
    {
        const int x = qRound(
            SongTime::tickToPixel(
                n.getStartTick(),
                m_pxPerBeat
            )
        );

            const int w = std::max(
                2,
                qRound(
                    SongTime::tickToPixel(
                        n.getDurationTick(),
                        m_pxPerBeat
                    )
                )
            );

        const int y =
            (m_topMidi - n.getMidiNote()) * m_rowHeight
            - verticalScrollBar()->value();

        return QRect(
            x - horizontalScrollBar()->value(),
            y,
            w,
            std::max(1, m_rowHeight - 2)
        );

    }

    Note* PianoRollEditor::noteAt(const QPoint& p)
    {
        if (!m_project)
            return nullptr;

            const int trackCount =
            static_cast<int>(m_project->tracks().size());

        if (m_activeTrack < 0 || m_activeTrack >= trackCount)
            return nullptr;

        auto& notes =
            m_project->tracks()[m_activeTrack].notes();

        for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
            if (noteRect(*it).contains(p))
                return &*it;
        }

        return nullptr;

    }

    void PianoRollEditor::paintEvent(QPaintEvent*)
    {
        if (!m_project)
            return;

            QPainter p(viewport());

        p.fillRect(
            viewport()->rect(),
            QColor("#17191c")
        );

        const int first =
            std::max(
                0,
                verticalScrollBar()->value()
                / std::max(1, m_rowHeight)
            );

        const int last =
            std::min(
                127,
                (verticalScrollBar()->value() + height())
                / std::max(1, m_rowHeight) + 1
            );

        for (int midi = first; midi <= last; ++midi) {
            const int y =
                midi * m_rowHeight
                - verticalScrollBar()->value();

            if (midi % 12 == 0) {
                p.fillRect(
                    0,
                    y,
                    width(),
                    m_rowHeight,
                    QColor("#1f2226")
                );
            }

            p.setPen(QColor("#292d32"));
            p.drawLine(0, y, width(), y);
        }

        if (m_showGrid) {
            for (int beat = 0; beat < 200; ++beat) {
                const double px =
                    beat * m_pxPerBeat
                    - horizontalScrollBar()->value();

                if (px < 0.0 || px > width())
                    continue;

                p.setPen(
                    beat % 4 == 0
                    ? QColor("#484d54")
                    : QColor("#2d3136")
                );

                p.drawLine(
                    QPointF(px, 0),
                    QPointF(px, height())
                );

                for (int sub = 1; sub < 4; ++sub) {
                    const double sx =
                        px + sub * m_pxPerBeat / 4.0;

                    if (sx > 0.0 &&
                        sx < width() &&
                        beat % 4 != 3) {

                        p.setPen(QColor("#24272c"));

                        p.drawLine(
                            QPointF(sx, 0),
                            QPointF(sx, height())
                        );
                    }
                }
            }
        }

        const int trackCount =
            static_cast<int>(m_project->tracks().size());

        if (m_activeTrack >= 0 &&
            m_activeTrack < trackCount) {

            const auto& notes =
                m_project->tracks()[m_activeTrack].notes();

            for (const auto& n : notes) {
                const QRect r = noteRect(n);

                if (!r.intersects(viewport()->rect()))
                    continue;

                p.setBrush(
                    n.isSelected()
                    ? QColor("#5c90ff")
                    : QColor("#4677bd")
                );

                p.setPen(
                    n.isSelected()
                    ? QColor("#aecdff")
                    : QColor("#6a95ca")
                );

                p.drawRoundedRect(r, 3, 3);

                p.setPen(Qt::white);

                p.drawText(
                    r.adjusted(6, 0, -3, 0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    n.getLyric()
                );
            }
        }

        const int playheadX =
            qRound(
                SongTime::tickToPixel(
                    m_playhead,
                    m_pxPerBeat
                )
            )
            - horizontalScrollBar()->value();

        p.setPen(
            QPen(QColor("#ff5b6e"), 2)
        );

        p.drawLine(
            playheadX,
            0,
            playheadX,
            height()
        );

    }

    void PianoRollEditor::mousePressEvent(QMouseEvent* e)
    {
        if (!m_project)
            return;

            setFocus();
        m_lastPos = e->pos();

        if (e->button() == Qt::MiddleButton) {
            m_panning = true;
            return;
        }

        if (e->button() != Qt::LeftButton)
            return;

        if (m_tool == EditTool::Pen ||
            m_tool == EditTool::PenPlus) {
            createNote(e->pos());
            return;
        }

        if (m_tool == EditTool::Eraser) {
            deleteNoteAt(e->pos());
            return;
        }

        if (m_tool == EditTool::Select) {
            if (auto* n = noteAt(e->pos())) {
                if (!(e->modifiers() & Qt::ControlModifier)) {
                    for (auto& x :
                        m_project->tracks()[m_activeTrack].notes()) {
                        x.setSelected(false);
                    }
                }

                n->setSelected(true);

                m_dragging = true;
                m_dragStartTick =
                    tickAtX(e->pos().x());

                m_dragStartPitch =
                    midiAtY(e->pos().y());

                m_dragIds.clear();

                for (const auto& x :
                    m_project->tracks()[m_activeTrack].notes()) {
                    if (x.isSelected())
                        m_dragIds.push_back(x.getId());
                }

                update();
            }
            else {
                for (auto& x :
                    m_project->tracks()[m_activeTrack].notes()) {
                    x.setSelected(false);
                }

                update();
            }

            return;
        }

        if (m_tool == EditTool::Knife) {
            if (auto* n = noteAt(e->pos())) {
                const qint64 t =
                    tickAtX(e->pos().x());

                if (t > n->getStartTick() &&
                    t < n->getEndTick()) {

                    Note clone = n->clone();

                    clone.setStartTick(t);

                    clone.setDurationTick(
                        n->getEndTick() - t
                    );

                    n->setDurationTick(
                        t - n->getStartTick()
                    );

                    m_project->tracks()[m_activeTrack]
                        .addNote(std::move(clone));

                    emit documentChanged();
                    update();
                }
            }
        }

    }

    void PianoRollEditor::mouseMoveEvent(QMouseEvent* e)
    {
        if (!m_project)
            return;

            if (m_panning) {
                const QPoint d =
                    e->pos() - m_lastPos;

                horizontalScrollBar()->setValue(
                    horizontalScrollBar()->value() - d.x()
                );

                verticalScrollBar()->setValue(
                    verticalScrollBar()->value() - d.y()
                );

                m_lastPos = e->pos();
                return;
            }

        if (m_dragging &&
            m_activeTrack >= 0 &&
            m_activeTrack <
            static_cast<int>(
                m_project->tracks().size()
                )) {

            const qint64 curTick =
                tickAtX(e->pos().x());

            const qint64 dt =
                curTick - m_dragStartTick;

            const int dp =
                midiAtY(e->pos().y())
                - m_dragStartPitch;

            for (qint64 id : m_dragIds) {
                if (auto* n =
                    m_project->tracks()
                    [m_activeTrack]
                    .findNote(id)) {

                    n->setStartTick(
                        n->getStartTick() + dt
                    );

                    n->setMidiNote(
                        n->getMidiNote() + dp
                    );
                }
            }

            m_dragStartTick = curTick;
            m_dragStartPitch =
                midiAtY(e->pos().y());

            emit documentChanged();
            update();
        }

        m_lastPos = e->pos();

    }

    void PianoRollEditor::mouseReleaseEvent(QMouseEvent* e)
    {
        Q_UNUSED(e);

            m_dragging = false;
        m_panning = false;

    }

    void PianoRollEditor::wheelEvent(QWheelEvent* e)
    {
        if (e->modifiers() & Qt::ControlModifier) {
            const double factor =
                e->angleDelta().y() > 0
                ? 1.12
                : 0.89;

                m_pxPerBeat =
                std::clamp(
                    m_pxPerBeat * factor,
                    30.0,
                    500.0
                );

            update();
        }
        else if (e->modifiers() & Qt::ShiftModifier) {
            horizontalScrollBar()->setValue(
                horizontalScrollBar()->value()
                - e->angleDelta().y() / 2
            );
        }
        else {
            verticalScrollBar()->setValue(
                verticalScrollBar()->value()
                - e->angleDelta().y() / 2
            );
        }

    }

    void PianoRollEditor::keyPressEvent(QKeyEvent* e)
    {
        if (!m_project)
            return;

            if (e->key() == Qt::Key_Delete ||
                e->key() == Qt::Key_Backspace) {

                if (m_activeTrack < 0 ||
                    m_activeTrack >=
                    static_cast<int>(
                        m_project->tracks().size()
                        )) {
                    return;
                }

                auto& notes =
                    m_project->tracks()[m_activeTrack].notes();

                for (int i =
                    static_cast<int>(notes.size()) - 1;
                    i >= 0;
                    --i) {

                    if (notes[i].isSelected())
                        notes.removeAt(i);
                }

                emit documentChanged();
                update();
                return;
            }

        QAbstractScrollArea::keyPressEvent(e);

    }

    void PianoRollEditor::createNote(const QPoint& pos)
    {
        if (!m_project)
            return;

            if (m_activeTrack < 0 ||
                m_activeTrack >=
                static_cast<int>(
                    m_project->tracks().size()
                    )) {
                return;
            }

        const qint64 start =
            tickAtX(pos.x());

        const int midi =
            std::clamp(
                midiAtY(pos.y()),
                0,
                127
            );

        Note n(
            QDateTime::currentMSecsSinceEpoch()
        );

        n.setStartTick(start);
        n.setDurationTick(480);
        n.setMidiNote(midi);
        n.setLyric("a");
        n.setSelected(true);

        for (auto& x :
            m_project->tracks()[m_activeTrack].notes()) {
            x.setSelected(false);
        }

        m_project->tracks()[m_activeTrack]
            .addNote(std::move(n));

        emit documentChanged();
        update();

    }

    void PianoRollEditor::deleteNoteAt(const QPoint& pos)
    {
        if (!m_project)
            return;

            if (auto* n = noteAt(pos)) {
                const qint64 id = n->getId();

                m_project->tracks()[m_activeTrack]
                    .removeNoteById(id);

                emit documentChanged();
                update();
            }

    }

    void PianoRollEditor::moveSelectedBy(const QPoint& delta)
    {
        Q_UNUSED(delta);
    }

    void PianoRollEditor::resizeSelectedBy(int dx)
    {
        Q_UNUSED(dx);
    }

    void PianoRollEditor::ensureVisibleTick(qint64 t)
    {
        const int x =
            qRound(
                SongTime::tickToPixel(
                    t,
                    m_pxPerBeat
                )
            );

            const int current =
            horizontalScrollBar()->value();

        const int visibleWidth =
            std::max(1, viewport()->width());

        if (x < current + 100 ||
            x > current + visibleWidth - 100) {

            horizontalScrollBar()->setValue(
                std::max(
                    0,
                    x - visibleWidth / 2
                )
            );
        }

    }

    void PianoRollEditor::invalidate()
    {
        update();
    }

} // namespace myvocal
