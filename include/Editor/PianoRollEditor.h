#pragma once
#include <QAbstractScrollArea>
#include <QVector>
#include <QPoint>
#include <QRect>
#include "Core/Project.h"
namespace myvocal {
enum class EditTool { Select, Pen, PenPlus, Eraser, Knife, Pitch, PitchLine, Vibrato, Zoom, Pan };
class PianoRollEditor : public QAbstractScrollArea { Q_OBJECT public: explicit PianoRollEditor(Project* project,QWidget* parent=nullptr); void setActiveTrack(int index); void setTool(EditTool tool); EditTool tool()const noexcept; void setPlayheadTick(qint64 tick); qint64 playheadTick()const noexcept; void setSnapEnabled(bool enabled); bool snapEnabled()const noexcept; void setShowGrid(bool v); signals: void documentChanged(); void noteDoubleClicked(qint64 id); void requestPlaybackTick(qint64 tick); protected: void paintEvent(QPaintEvent*) override; void mousePressEvent(QMouseEvent*) override; void mouseMoveEvent(QMouseEvent*) override; void mouseReleaseEvent(QMouseEvent*) override; void wheelEvent(QWheelEvent*) override; void keyPressEvent(QKeyEvent*) override; private: qint64 snapTick(qint64 tick)const; qint64 tickAtX(int x)const; int midiAtY(int y)const; QRect noteRect(const Note&n)const; Note* noteAt(const QPoint&pos); void ensureVisibleTick(qint64 tick); void createNote(const QPoint&pos); void deleteNoteAt(const QPoint&pos); void moveSelectedBy(const QPoint&delta); void resizeSelectedBy(int dx); void invalidate(); Project* m_project; int m_activeTrack{0}; EditTool m_tool{EditTool::Select}; qint64 m_playhead{0}; bool m_snap{true}; bool m_showGrid{true}; double m_pxPerBeat{110.0}; int m_rowHeight{22}; int m_topMidi{84}; bool m_dragging{false}; bool m_resizing{false}; bool m_panning{false}; QPoint m_lastPos; qint64 m_dragStartTick{0}; int m_dragStartPitch{60}; QVector<qint64> m_dragIds; };
}
