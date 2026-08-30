#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include "Audio/AudioEngine.h"
namespace myvocal { class TransportBar : public QWidget { Q_OBJECT public: explicit TransportBar(AudioEngine*,QWidget*parent=nullptr); signals: void playPause(); void stopPressed(); private: AudioEngine*m_audio; QLabel*m_pos; }; }
