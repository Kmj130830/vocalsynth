#pragma once

#include <QAudioFormat>
#include <QAudioOutput>
#include <QAudioSink>
#include <QBuffer>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <memory>

#include "Core/AudioClip.h"

namespace myvocal {

class AudioEngine final : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    void load(const QString& path);
    void setBackingClips(const QVector<AudioClip>& clips);
    void play();
    void pause();
    void stop(bool preservePosition = false);
    void seek(qint64 ms);
    bool isPlaying() const;
    qint64 position() const;

signals:
    void positionChanged(qint64 ms);
    void playbackStateChanged(bool playing);
    void mediaError(const QString& message);

private:
    bool loadPcmWav(const QString& path);
    void destroySink();
    void stopBackingPlayers();
    void syncBackingPlayers(qint64 ms, bool start);
    qint64 pcmPositionMs() const;

    QByteArray m_pcm;
    QAudioFormat m_format;
    QBuffer m_buffer;
    std::unique_ptr<QAudioSink> m_sink;
    QTimer m_positionTimer;
    qint64 m_seekMs{0};
    qint64 m_seekByte{0};
    bool m_loaded{false};

    QVector<AudioClip> m_clips;
    QVector<QMediaPlayer*> m_backingPlayers;
    QVector<QAudioOutput*> m_backingOutputs;
};

}