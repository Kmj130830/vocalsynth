#pragma once

#include <QAudioFormat>
#include <QAudioOutput>
#include <QAudioSink>
#include <QBuffer>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <memory>
#include <vector>

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
    void previewTone(int midi, int durationMs = 90);

signals:
    void positionChanged(qint64 ms);
    void playbackStateChanged(bool playing);
    void mediaError(const QString& message);

private:
    bool loadPcmWav(const QString& path);
    void destroyPrimaryPlayer();
    void destroySink();
    void stopBackingPlayers();
    void syncBackingPlayers(qint64 ms, bool start);
    qint64 pcmPositionMs() const;
    void startClock(qint64 baseMs);
    void stopClock();

    QByteArray m_pcm;
    QAudioFormat m_format;
    QBuffer m_buffer;
    std::unique_ptr<QAudioSink> m_sink;
    std::unique_ptr<QMediaPlayer> m_primaryPlayer;
    std::unique_ptr<QAudioOutput> m_primaryOutput;
    QTimer m_positionTimer;
    QElapsedTimer m_clock;
    qint64 m_clockBaseMs{0};
    qint64 m_seekMs{0};
    qint64 m_seekByte{0};
    bool m_loaded{false};
    bool m_primaryIsPcm{false};

    QVector<AudioClip> m_clips;
    std::vector<std::unique_ptr<QMediaPlayer>> m_backingPlayers;
    std::vector<std::unique_ptr<QAudioOutput>> m_backingOutputs;

    QByteArray m_previewPcm;
    QAudioFormat m_previewFormat;
    std::unique_ptr<QAudioSink> m_previewSink;
    QBuffer m_previewBuffer;
};

}