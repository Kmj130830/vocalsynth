#include "Editor/PianoRollEditor.h"

#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace myvocal {

bool PianoRollEditor::viewportEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        mousePressEvent(static_cast<QMouseEvent*>(event));
        viewport()->update();
        return true;
    case QEvent::MouseButtonRelease:
        mouseReleaseEvent(static_cast<QMouseEvent*>(event));
        viewport()->update();
        return true;
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        mouseMoveEvent(mouse);
        if (!m_dragging && !m_panning && m_tool == EditTool::Select) {
            if (Note* note = noteAt(mouse->position().toPoint())) {
                viewport()->setCursor(
                    isResizeHandle(*note, mouse->position().toPoint())
                        ? Qt::SizeHorCursor
                        : Qt::ArrowCursor);
            } else {
                viewport()->setCursor(Qt::ArrowCursor);
            }
        }
        viewport()->update();
        return true;
    }
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent(static_cast<QMouseEvent*>(event));
        viewport()->update();
        return true;
    case QEvent::Wheel:
        wheelEvent(static_cast<QWheelEvent*>(event));
        viewport()->update();
        return true;
    default:
        break;
    }

    return QAbstractScrollArea::viewportEvent(event);
}

}