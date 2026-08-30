#include "UI/PianoKeyboard.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace myvocal {

namespace {
const QStringList kNames = {
    QStringLiteral("C"), QStringLiteral("C#"), QStringLiteral("D"),
    QStringLiteral("D#"), QStringLiteral("E"), QStringLiteral("F"),
    QStringLiteral("F#"), QStringLiteral("G"), QStringLiteral("G#"),
    QStringLiteral("A"), QStringLiteral("A#"), QStringLiteral("B")
};
}

PianoKeyboard::PianoKeyboard(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(76);
    setMaximumWidth(96);
    setMouseTracking(true);
}

void PianoKeyboard::setScrollPitch(int midi)
{
    m_topMidi = std::clamp(midi, 0, 127);
    update();
}

void PianoKeyboard::setRowHeight(int pixels)
{
    m_rowHeight = std::max(8, pixels);
    update();
}

int PianoKeyboard::midiAtY(int y) const
{
    return std::clamp(m_topMidi - y / m_rowHeight, 0, 127);
}

void PianoKeyboard::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#111315"));

    const int first = std::clamp(m_topMidi - height() / m_rowHeight - 1, 0, 127);
    const int last = std::clamp(m_topMidi + 1, 0, 127);

    for (int midi = first; midi <= last; ++midi) {
        const int y = (m_topMidi - midi) * m_rowHeight;
        const bool black = kNames.at(midi % 12).contains('#');
        const QRect key(0, y, width(), m_rowHeight);

        painter.fillRect(key, black ? QColor("#24272b") : QColor("#e2e4e7"));
        painter.setPen(black ? QColor("#6e7379") : QColor("#8a8e93"));
        painter.drawRect(key.adjusted(0, 0, -1, -1));

        const QString label = QStringLiteral("%1%2")
                                  .arg(kNames.at(midi % 12))
                                  .arg(midi / 12 - 1);
        painter.setPen(black ? Qt::white : Qt::black);
        painter.drawText(key.adjusted(7, 0, -3, 0),
                         Qt::AlignVCenter | Qt::AlignLeft, label);
    }
}

void PianoKeyboard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit keyPressed(midiAtY(qRound(event->position().y())));
    }
    QWidget::mousePressEvent(event);
}

}
