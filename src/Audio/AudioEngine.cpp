#include "Audio/AudioEngine.h"

#include <QFileInfo>
#include <QUrl>

#include <algorithm>

namespace myvocal {

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    m_player.setAudioOutput(&m_output);
    m_output.setVolume(1.0);
    connect(&m_player, &QMediaPlayer::positionChanged,
            this, &AudioEngine::positionChanged);
}

void AudioEngine::load(const QString& path)
{
    m_player.stop();
    m_player.setSource(QUrl::fromLocalFile(path));
}

void AudioEngine::stopBackingPlayers()
{
    for (auto& player : m_backingPlayers) {
        if (player) player->stop();
    }
}

void AudioEngine::setBackingClips(const QVector<AudioClip>& clips)
{
    stopBackingPlayers();
    m_backingPlayers.clear();
    m_backingOutputs.clear();
    m_clips = clips;

    for (const auto& clip : m_clips) {
        if (clip.muted || !QFileInfo::exists(clip.path)) continue;
        auto player = std::make_unique<QMediaPlayer>();
        auto output = std::make_unique<QAudioOutput>();
        output->setVolume(std::clamp(clip.volume, 0.0, 2.0));
        player->setAudioOutput(output.get());
        player->setSource(QUrl::fromLocalFile(clip.path));
        m_backingOutputs.push_back(std::move(output));
        m_backingPlayers.push_back(std::move(player));
    }
}

void AudioEngine::syncBackingPlayers(qint64 ms, bool start)
{
    int playerIndex = 0;
    for (const auto& clip : m_clips) {
        if (clip.muted || !QFileInfo::exists(clip.path)) continue;
        if (playerIndex >= m_backingPlayers.size()) break;
        auto* player = m_backingPlayers[playerIndex++].get();
        if (!player) continue;

        const qint64 local = ms - clip.startMs + clip.offsetMs;
        if (local < 0) {
            player->pause();
            continue;
        }
        player->setPosition(local);
        if (start) player->play();
    }
}

void AudioEngine::play()
{
    m_player.play();
    syncBackingPlayers(m_player.position(), true);
}

void AudioEngine::pause()
{
    m_player.pause();
    for (auto& player : m_backingPlayers) {
        if (player) player->pause();
    }
}

void AudioEngine::stop()
{
    m_player.stop();
    stopBackingPlayers();
}

void AudioEngine::seek(qint64 ms)
{
    ms = std::max<qint64>(0, ms);
    m_player.setPosition(ms);
    syncBackingPlayers(ms, isPlaying());
}

bool AudioEngine::isPlaying() const
{
    return m_player.playbackState() == QMediaPlayer::PlayingState;
}

qint64 AudioEngine::position() const
{
    return m_player.position();
}

}