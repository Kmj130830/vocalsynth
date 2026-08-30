#include "UI/PianoKeyboard.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace myvocal {

    PianoKeyboard::PianoKeyboard(QWidget* p)
        : QWidget(p)
    {
        setMinimumWidth(72);
        setMaximumWidth(90);
    }

    void PianoKeyboard::setScrollPitch(int m)
    {
        m_topMidi = std::clamp(m, 24, 108);
        update();
    }

    void PianoKeyboard::paintEvent(QPaintEvent*)
    {
        QPainter p(this);

            const int rows = 12;
        const int h = std::max(1, height() / rows);

        for (int i = 0; i < rows; ++i) {
            const int midi = m_topMidi - i;

            QRect r(0, i * h, width(), h + 1);

            const bool black =
                QStringLiteral("12356810").contains(
                    QString::number(midi % 12)
                );

            p.fillRect(
                r,
                black ? QColor("#25272a")
                : QColor("#d6d7d9")
            );

            p.setPen(black ? Qt::white : Qt::black);

            const QStringList names = {
                "C", "C#", "D", "D#", "E", "F",
                "F#", "G", "G#", "A", "A#", "B"
            };

            const int noteIndex = ((midi % 12) + 12) % 12;

            p.drawText(
                r.adjusted(5, 0, -2, 0),
                Qt::AlignVCenter | Qt::AlignLeft,
                QStringLiteral("%1%2")
                .arg(names.at(noteIndex))
                .arg(midi / 12 - 1)
            );

            p.setPen(QColor("#555555"));
            p.drawRect(r);
        }

    }

    void PianoKeyboard::mousePressEvent(QMouseEvent* e)
    {
        if (e->button() != Qt::LeftButton)
            return;

            const int h = std::max(1, height() / 12);

        const int row = std::clamp(
            static_cast<int>(e->position().y() / h),
            0,
            11
        );

        const int midi = m_topMidi - row;

        emit keyPressed(midi);

    }

} // namespace myvocal
