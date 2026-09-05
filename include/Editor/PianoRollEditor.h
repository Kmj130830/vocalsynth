#pragma once

#include <QAbstractScrollArea>
#include <QHash>
#include <QPoint>
#include <QRect>
#include <QVector>

#include "Core/Project.h"

class QEvent;
class QLineEdit;
class QMouseEvent;

namespace myvocal {

enum class EditTool { Select, Pen, PenPlus, Eraser, Knife, Pitch, PitchLine, Vibrato, Zoom, Pan };

class PianoRollEditor final : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit PianoRollEditor(Project* project, QWidget* parent = nullptr);
    Project* project() const noexcept;
    void setActiveTrack(int index);
    int activeTrack() const noexcept;
    void setTool(EditTool tool);
    EditTool tool() const noexcept;
    void setPlayheadTick(qint64 tick);
    qint64 playheadTick() const noexcept;
    void setSnapEnabled(bool enabled);
    bool snapEnabled() const noexcept;
    void setShowGrid(bool value);
    bool showGrid() const noexcept;
    void setGridTicks(qint64 ticks);
    qint64 gridTicks() const noexcept;
    void setKeyboardPitch(int midi);

signals:
    void documentChanged();
    void noteDoubleClicked(qint64 id);
    void requestPlaybackTick(qint64 tick);
    void keyboardPreviewRequested(int midi);
    void verticalPitchChanged(int topMidi);

protected:
    bool viewportEvent(QEvent* event) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    qint64 snapTick(qint64 tick) const;
    qint64 tickAtX(int x) const;
    int midiAtY(int y) const;
    QRect noteRect(const Note& note) const;
    Note* noteAt(const QPoint& pos);
    bool isResizeHandle(const Note& note, const QPoint& pos) const;
    bool hasTimeOverlap(qint64 start, qint64 end, qint64 ignoreId = -1) const;
    qint64 constrainedMoveDelta(qint64 desiredDelta) const;
    qint64 availableEndForNote(const Note& note, qint64 desiredEnd) const;
    void ensureVisibleTick(qint64 tick);
    void createNote(const QPoint& pos);
    void createDraggedNote(qint64 startTick, qint64 endTick, int midi);
    void deleteNoteAt(const QPoint& pos);
    void splitNoteAt(const QPoint& pos);
    void beginLyricEdit(Note& note);
    void finishLyricEdit(bool accept);
    void commitRightDragNote();
    void sortNotes();
    void invalidate();
    void updateScrollRanges();

    Project* m_project{nullptr};
    QLineEdit* m_lyricEditor{nullptr};
    qint64 m_editingLyricId{-1};
    int m_activeTrack{0};
    EditTool m_tool{EditTool::Pen};
    qint64 m_playhead{0};
    bool m_snap{true};
    bool m_showGrid{true};
    qint64 m_gridTicks{120};
    double m_pxPerBeat{110.0};
    int m_rowHeight{22};
    int m_topMidi{84};

    bool m_dragging{false};
    bool m_resizing{false};
    bool m_panning{false};
    bool m_playheadDragging{false};
    bool m_rightDrawing{false};
    bool m_mouseMovedDuringDrag{false};

    QPoint m_lastPos;
    qint64 m_dragStartTick{0};
    int m_dragStartPitch{60};
    qint64 m_dragAnchorTick{0};
    int m_dragAnchorPitch{60};
    QVector<qint64> m_dragIds;
    QHash<qint64, qint64> m_dragOriginalStart;
    QHash<qint64, int> m_dragOriginalPitch;
    qint64 m_resizeId{-1};
    qint64 m_resizeOriginalEnd{0};

    qint64 m_rightDrawId{-1};
    qint64 m_rightDrawStart{0};
    int m_rightDrawPitch{60};
};

}