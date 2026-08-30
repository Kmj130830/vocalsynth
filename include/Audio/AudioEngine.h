#pragma once
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
namespace myvocal { class AudioEngine : public QObject { Q_OBJECT public: explicit AudioEngine(QObject* parent=nullptr); void load(const QString&path); void play(); void pause(); void stop(); void seek(qint64 ms); bool isPlaying()const; qint64 position()const; signals: void positionChanged(qint64); private: QMediaPlayer m_player; QAudioOutput m_output; }; }
