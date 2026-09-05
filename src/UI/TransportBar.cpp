#include "UI/TransportBar.h"

#include <QFont>
#include <QHBoxLayout>

namespace myvocal {

namespace {
QString formatTime(qint64 ms)
{
    ms = qMax<qint64>(0, ms);
    const qint64 minutes = ms / 60000;
    const qint64 seconds = (ms / 1000) % 60;
    const qint64 millis = ms % 1000;
    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(millis, 3, 10, QChar('0'));
}
}

TransportBar::TransportBar(AudioEngine* audio, QWidget* parent)
    : QWidget(parent), m_audio(audio)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(6);

    auto* play = new QPushButton(QStringLiteral("Play"), this);
    auto* stop = new QPushButton(QStringLiteral("Stop"), this);
    m_pos = new QLabel(QStringLiteral("00:00.000"), this);
    m_pos->setMinimumWidth(92);
    m_pos->setAlignment(Qt::AlignCenter);

    QFont timeFont = m_pos->font();
    timeFont.setStyleHint(QFont::Monospace);
    timeFont.setPointSize(std::max(9, timeFont.pointSize() + 1));
    m_pos->setFont(timeFont);
    m_pos->setToolTip(QStringLiteral("Current playback position"));

    layout->addWidget(play);
    layout->addWidget(stop);
    layout->addSpacing(6);
    layout->addWidget(m_pos);
    layout->addStretch(1);

    connect(play, &QPushButton::clicked, this, &TransportBar::playPause);
    connect(stop, &QPushButton::clicked, this, &TransportBar::stopPressed);
    if (m_audio) {
        connect(m_audio, &AudioEngine::positionChanged, this, [this](qint64 ms) {
            m_pos->setText(formatTime(ms));
        });
    }
}

}