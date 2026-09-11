#pragma once

#include <QPainter>
#include <QRect>
#include <QString>

namespace myvocal {

struct EditorFrame final {
    static constexpr int kRadius = 4;
    static constexpr int kBorder = 1;

    static void drawBackground(QPainter& painter, const QRect& rect)
    {
        painter.fillRect(rect, QColor("#111316"));
    }

    static void drawPanel(QPainter& painter, const QRect& rect, bool selected = false)
    {
        const QColor fill = selected ? QColor("#273c55") : QColor("#1a1d22");
        const QColor border = selected ? QColor("#4d8ddd") : QColor("#2c3239");
        painter.setBrush(fill);
        painter.setPen(QPen(border, kBorder));
        painter.drawRoundedRect(rect, kRadius, kRadius);
    }

    static void drawSectionHeader(QPainter& painter, const QRect& rect, const QString& text)
    {
        painter.fillRect(rect, QColor("#181b20"));
        painter.setPen(QColor("#343a42"));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
        painter.setPen(QColor("#d7dce3"));
        painter.drawText(rect.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
};

}
