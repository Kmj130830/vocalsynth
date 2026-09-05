#pragma once

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>

#include <memory>
#include <vector>

#include "Core/AudioClip.h"

namespace myvocal {

class AudioEngine final : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject* parent = nullptr);

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
    void stopBackingPlayers();
    void syncBackingPlayers(qint64 ms, bool start);

    QMediaPlayer m_player;
    QAudioOutput m_output;
    QVector<AudioClip> m_clips;
    std::vector<std::unique_ptr<QMediaPlayer>> m_backingPlayers;
    std::vector<std::unique_ptr<QAudioOutput>> m_backingOutputs;
};

}