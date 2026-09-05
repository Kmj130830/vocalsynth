#include "Audio/AudioEngine.h"

#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace myvocal {
namespace {

quint16 rd16(const QByteArray& b, qsizetype p)
{
    return static_cast<quint16>(static_cast<quint8>(b.at(p)) |
                                (static_cast<quint16>(static_cast<quint8>(b.at(p + 1))) << 8));
}

quint32 rd32(const QByteArray& b, qsizetype p)
{
    return static_cast<quint32>(static_cast<quint8>(b.at(p)) |
                                (static_cast<quint32>(static_cast<quint8>(b.at(p + 1))) << 8) |
                                (static_cast<quint32>(static_cast<quint8>(b.at(p + 2))) << 16) |
                                (static_cast<quint32>(static_cast<quint8>(b.at(p + 3))) << 24));
}

bool tag(const QByteArray& b, qsizetype p, const char* text)
{
    return p >= 0 && p + 4 <= b.size() && std::memcmp(b.constData() + p, text, 4) == 0;
}

double sampleToFloat(const QByteArray& bytes, qsizetype p, int bits, int format)
{
    if (format == 3 && bits == 32) {
        float value = 0.0f;
        std::memcpy(&value, bytes.constData() + p, sizeof(float));
        return std::clamp(static_cast<double>(value), -1.0, 1.0);
    }
    switch (bits) {
    case 8:
        return (static_cast<int>(static_cast<quint8>(bytes.at(p))) - 128) / 128.0;
    case 16:
        return static_cast<qint16>(rd16(bytes, p)) / 32768.0;
    case 24: {
        const quint32 raw = static_cast<quint32>(static_cast<quint8>(bytes.at(p))) |
                            (static_cast<quint32>(static_cast<quint8>(bytes.at(p + 1))) << 8) |
                            (static_cast<quint32>(static_cast<quint8>(bytes.at(p + 2))) << 16);
        const qint32 signedValue = (raw & 0x00800000u) ? static_cast<qint32>(raw | 0xff000000u)
                                                        : static_cast<qint32>(raw);
        return signedValue / 8388608.0;
    }
    case 32:
        return static_cast<qint32>(rd32(bytes, p)) / 2147483648.0;
    default:
        return 0.0;
    }
}

}

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    m_positionTimer.setInterval(16);
    connect(&m_positionTimer, &QTimer::timeout, this, [this] {
        emit positionChanged(position());
    });
}

AudioEngine::~AudioEngine()
{
    destroySink();
    destroyPrimaryPlayer();
    stopBackingPlayers();
}

void AudioEngine::destroyPrimaryPlayer()
{
    if (m_primaryPlayer) m_primaryPlayer->stop();
    m_primaryPlayer.reset();
    m_primaryOutput.reset();
}

void AudioEngine::destroySink()
{
    m_positionTimer.stop();
    stopClock();
    if (m_sink) m_sink->stop();
    m_sink.reset();
    if (m_previewSink) m_previewSink->stop();
    m_previewSink.reset();
    if (m_buffer.isOpen()) m_buffer.close();
    if (m_previewBuffer.isOpen()) m_previewBuffer.close();
}

bool AudioEngine::loadPcmWav(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 12 || !tag(bytes, 0, "RIFF") || !tag(bytes, 8, "WAVE")) return false;

    int format = 0;
    int channels = 0;
    int rate = 0;
    int bits = 0;
    qsizetype dataOffset = -1;
    qsizetype dataSize = 0;
    qsizetype pos = 12;

    while (pos + 8 <= bytes.size()) {
        const quint32 chunkSize = rd32(bytes, pos + 4);
        const qsizetype dataPos = pos + 8;
        if (dataPos > bytes.size()) break;
        const qsizetype safeSize = std::min<qsizetype>(chunkSize, bytes.size() - dataPos);
        if (tag(bytes, pos, "fmt ") && safeSize >= 16) {
            format = static_cast<int>(rd16(bytes, dataPos));
            channels = static_cast<int>(rd16(bytes, dataPos + 2));
            rate = static_cast<int>(rd32(bytes, dataPos + 4));
            bits = static_cast<int>(rd16(bytes, dataPos + 14));
            if (format == 0xfffe && safeSize >= 40) {
                const quint32 subFormat = rd32(bytes, dataPos + 24);
                if (subFormat == 1) format = 1;
                else if (subFormat == 3) format = 3;
            }
        } else if (tag(bytes, pos, "data")) {
            dataOffset = dataPos;
            dataSize = safeSize;
            break;
        }
        pos = dataPos + safeSize + (chunkSize & 1u);
    }

    if ((format != 1 && format != 3) || channels <= 0 || channels > 32 || rate <= 0 ||
        (bits != 8 && bits != 16 && bits != 24 && bits != 32) || dataOffset < 0 || dataSize <= 0) {
        return false;
    }
    if (format == 3 && bits != 32) return false;

    const int bytesPerSample = bits / 8;
    const int frameBytes = channels * bytesPerSample;
    if (frameBytes <= 0) return false;
    const qsizetype frames = dataSize / frameBytes;
    if (frames <= 0) return false;

    m_pcm.resize(static_cast<qsizetype>(frames) * channels * 2);
    for (qsizetype frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const qsizetype source = dataOffset + frame * frameBytes + channel * bytesPerSample;
            const double sample = sampleToFloat(bytes, source, bits, format);
            const qint16 converted = static_cast<qint16>(std::round(std::clamp(sample, -1.0, 1.0) * 32767.0));
            const qsizetype target = (frame * channels + channel) * 2;
            m_pcm[target] = static_cast<char>(converted & 0xff);
            m_pcm[target + 1] = static_cast<char>((converted >> 8) & 0xff);
        }
    }

    m_format = QAudioFormat();
    m_format.setSampleRate(rate);
    m_format.setChannelCount(channels);
    m_format.setSampleFormat(QAudioFormat::Int16);
    m_buffer.setData(m_pcm);
    m_buffer.open(QIODevice::ReadOnly);
    m_seekMs = 0;
    m_seekByte = 0;
    m_loaded = true;
    m_primaryIsPcm = true;
    return true;
}

void AudioEngine::load(const QString& path)
{
    destroySink();
    destroyPrimaryPlayer();
    m_loaded = false;
    m_primaryIsPcm = false;
    m_pcm.clear();
    m_seekMs = 0;
    m_seekByte = 0;

    if (!QFileInfo::isFile(path)) {
        emit mediaError(QStringLiteral("Audio file not found: %1").arg(path));
        return;
    }

    if (loadPcmWav(path)) return;

    m_primaryOutput = std::make_unique<QAudioOutput>();
    m_primaryOutput->setVolume(1.0);
    m_primaryPlayer = std::make_unique<QMediaPlayer>();
    m_primaryPlayer->setAudioOutput(m_primaryOutput.get());
    connect(m_primaryPlayer.get(), &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& text) {
        emit mediaError(text.isEmpty() ? QStringLiteral("Unable to decode rendered audio.") : text);
    });
    m_primaryPlayer->setSource(QUrl::fromLocalFile(path));
    m_loaded = true;
}

void AudioEngine::stopBackingPlayers()
{
    for (auto& player : m_backingPlayers) if (player) player->stop();
}

void AudioEngine::setBackingClips(const QVector<AudioClip>& clips)
{
    stopBackingPlayers();
    m_backingPlayers.clear();
    m_backingOutputs.clear();
    m_clips = clips;

    for (const auto& clip : m_clips) {
        if (clip.muted || !QFileInfo::isFile(clip.path)) continue;
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
    std::size_t index = 0;
    for (const auto& clip : m_clips) {
        if (clip.muted || !QFileInfo::isFile(clip.path)) continue;
        if (index >= m_backingPlayers.size()) break;
        auto* player = m_backingPlayers[index++].get();
        if (!player) continue;
        const qint64 local = ms - clip.startMs + clip.offsetMs;
        if (local < 0) {
            player->pause();
            player->setPosition(0);
        } else {
            player->setPosition(local);
            if (start) player->play(); else player->pause();
        }
    }
}

qint64 AudioEngine::pcmPositionMs() const
{
    if (!m_loaded || !m_primaryIsPcm || m_format.sampleRate() <= 0 || m_format.channelCount() <= 0) return m_seekMs;
    const qint64 bytesPerSec = qint64(m_format.sampleRate()) * m_format.channelCount() * 2;
    if (bytesPerSec <= 0) return m_seekMs;
    return m_seekMs + std::max<qint64>(0, qint64(m_buffer.pos()) - m_seekByte) * 1000 / bytesPerSec;
}

void AudioEngine::startClock(qint64 baseMs)
{
    m_clockBaseMs = std::max<qint64>(0, baseMs);
    m_clock.start();
    m_positionTimer.start();
}

void AudioEngine::stopClock()
{
    if (m_clock.isValid()) m_clockBaseMs = std::max<qint64>(0, position());
    m_clock.invalidate();
    m_positionTimer.stop();
}

void AudioEngine::play()
{
    if (!m_loaded && m_backingPlayers.empty()) return;

    const qint64 base = position();
    if (m_loaded && m_primaryIsPcm) {
        if (!m_sink) {
            m_sink = std::make_unique<QAudioSink>(m_format);
            m_sink->setBufferSize(std::max(4096, m_format.sampleRate() * m_format.channelCount() * 2 / 20));
            connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
                if (state == QAudio::StoppedState && m_sink && m_sink->error() != QAudio::NoError) {
                    emit mediaError(QStringLiteral("Audio output error: %1").arg(int(m_sink->error())));
                }
                if (state == QAudio::IdleState) {
                    stopClock();
                    emit playbackStateChanged(false);
                }
            });
        }
        if (m_sink->state() == QAudio::SuspendedState) m_sink->resume();
        else {
            m_seekByte = m_buffer.pos();
            m_sink->start(&m_buffer);
        }
    } else if (m_loaded && m_primaryPlayer) {
        m_primaryPlayer->setPosition(base);
        m_primaryPlayer->play();
    }

    syncBackingPlayers(base, true);
    startClock(base);
    emit playbackStateChanged(true);
}

void AudioEngine::pause()
{
    const qint64 current = position();
    if (m_sink) m_sink->suspend();
    if (m_primaryPlayer) m_primaryPlayer->pause();
    for (auto& player : m_backingPlayers) if (player) player->pause();
    m_seekMs = current;
    stopClock();
    emit positionChanged(m_seekMs);
    emit playbackStateChanged(false);
}

void AudioEngine::stop(bool preservePosition)
{
    const qint64 current = position();
    if (m_sink) m_sink->stop();
    if (m_primaryPlayer) m_primaryPlayer->stop();
    stopBackingPlayers();
    stopClock();

    if (preservePosition) {
        seek(current);
    } else {
        m_seekMs = 0;
        m_seekByte = 0;
        if (m_buffer.isOpen()) m_buffer.seek(0);
        if (m_primaryPlayer) m_primaryPlayer->setPosition(0);
        emit positionChanged(0);
    }
    emit playbackStateChanged(false);
}

void AudioEngine::seek(qint64 ms)
{
    ms = std::max<qint64>(0, ms);
    const bool wasPlaying = isPlaying();

    if (m_primaryIsPcm && m_loaded && m_format.sampleRate() > 0 && m_format.channelCount() > 0) {
        if (m_sink && m_sink->state() != QAudio::StoppedState) m_sink->stop();
        const qint64 bytesPerSec = qint64(m_format.sampleRate()) * m_format.channelCount() * 2;
        const qint64 frameBytes = qint64(m_format.channelCount()) * 2;
        qint64 byte = bytesPerSec > 0 ? ms * bytesPerSec / 1000 : 0;
        if (frameBytes > 0) byte -= byte % frameBytes;
        byte = std::clamp<qint64>(byte, 0, m_pcm.size());
        m_buffer.seek(byte);
        m_seekMs = ms;
        m_seekByte = byte;
    } else if (m_primaryPlayer) {
        m_primaryPlayer->setPosition(ms);
        m_seekMs = ms;
    } else {
        m_seekMs = ms;
    }

    if (wasPlaying) startClock(ms);
    else if (!m_primaryIsPcm) m_clockBaseMs = ms;
    syncBackingPlayers(ms, wasPlaying);
    emit positionChanged(ms);
    if (wasPlaying && m_primaryIsPcm) play();
}

bool AudioEngine::isPlaying() const
{
    if (m_primaryIsPcm) return m_sink && m_sink->state() == QAudio::ActiveState;
    if (m_primaryPlayer && m_primaryPlayer->playbackState() == QMediaPlayer::PlayingState) return true;
    for (const auto& player : m_backingPlayers) {
        if (player && player->playbackState() == QMediaPlayer::PlayingState) return true;
    }
    return m_clock.isValid() && m_clock.isValid();
}

qint64 AudioEngine::position() const
{
    if (m_primaryIsPcm) return pcmPositionMs();
    if (m_primaryPlayer) return std::max<qint64>(0, m_primaryPlayer->position());
    if (m_clock.isValid()) return m_clockBaseMs + m_clock.elapsed();
    return m_seekMs;
}

void AudioEngine::previewTone(int midi, int durationMs)
{
    midi = std::clamp(midi, 0, 127);
    durationMs = std::clamp(durationMs, 30, 250);
    constexpr int rate = 44100;
    const int frames = rate * durationMs / 1000;
    m_previewPcm.resize(frames * 2);

    const double frequency = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        const double attack = std::min(1.0, t / 0.004);
        const double release = std::min(1.0, static_cast<double>(frames - i) / (rate * 0.012));
        const double env = std::min(attack, release);
        const double sample = std::sin(2.0 * M_PI * frequency * t) * env * 0.22;
        const qint16 value = static_cast<qint16>(std::clamp(sample, -1.0, 1.0) * 32767.0);
        m_previewPcm[i * 2] = static_cast<char>(value & 0xff);
        m_previewPcm[i * 2 + 1] = static_cast<char>((value >> 8) & 0xff);
    }

    if (m_previewSink) m_previewSink->stop();
    if (m_previewBuffer.isOpen()) m_previewBuffer.close();
    m_previewFormat = QAudioFormat();
    m_previewFormat.setSampleRate(rate);
    m_previewFormat.setChannelCount(1);
    m_previewFormat.setSampleFormat(QAudioFormat::Int16);
    m_previewBuffer.setData(m_previewPcm);
    m_previewBuffer.open(QIODevice::ReadOnly);
    m_previewSink = std::make_unique<QAudioSink>(m_previewFormat);
    m_previewSink->setVolume(0.7);
    m_previewSink->start(&m_previewBuffer);
}

}