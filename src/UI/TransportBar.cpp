#include "UI/TransportBar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace myvocal {

TransportBar::TransportBar(AudioEngine* audio, QWidget* parent)
    : QWidget(parent), m_audio(audio)
{
    setObjectName(QStringLiteral("TransportBar"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(6);

    auto* play = new QPushButton(QStringLiteral("Play"), this);
    play->setObjectName(QStringLiteral("TransportPlay"));
    auto* stop = new QPushButton(QStringLiteral("Stop"), this);
    stop->setObjectName(QStringLiteral("TransportStop"));
    m_pos = new QLabel(QStringLiteral("00:00.000"), this);
    m_pos->setObjectName(QStringLiteral("TransportPosition"));
    m_pos->setMinimumWidth(92);
    m_pos->setAlignment(Qt::AlignCenter);

    layout->addWidget(play);
    layout->addWidget(stop);
    layout->addSpacing(4);
    layout->addWidget(m_pos);
    layout->addStretch(1);

    connect(play, &QPushButton::clicked, this, &TransportBar::playPause);
    connect(stop, &QPushButton::clicked, this, &TransportBar::stopPressed);
    if (m_audio) {
        connect(m_audio, &AudioEngine::positionChanged, this, [this](qint64 ms) {
            const qint64 totalSeconds = std::max<qint64>(0, ms) / 1000;
            const qint64 minutes = totalSeconds / 60;
            const qint64 seconds = totalSeconds % 60;
            const qint64 millis = std::max<qint64>(0, ms) % 1000;
            m_pos->setText(QStringLiteral("%1:%2.%3")
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0'))
                .arg(millis, 3, 10, QChar('0')));
        });
    }
}

}