#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <memory>

#include "Audio/AudioEngine.h"
#include "Core/Project.h"

namespace myvocal {

class Renderer;

class PlaybackController final : public QObject {
    Q_OBJECT
public:
    explicit PlaybackController(AudioEngine* audio, Renderer* renderer, QObject* parent = nullptr);
    ~PlaybackController() override;

    void setProject(Project* project);
    void playFromMs(qint64 ms);
    void pause();
    void stop(bool returnToStart);
    void seekMs(qint64 ms);
    bool isPreparing() const noexcept;
    qint64 playbackStartMs() const noexcept;

signals:
    void preparingChanged(bool preparing);
    void playbackError(const QString& message);
    void positionChanged(qint64 ms);
    void stateChanged(bool playing);

private slots:
    void onAudioPosition(qint64 ms);

private:
    void prepareAndPlay(qint64 startMs);
    QString cachePath() const;
    void cleanupThread();

    AudioEngine* m_audio{nullptr};
    Renderer* m_renderer{nullptr};
    Project* m_project{nullptr};
    std::unique_ptr<QThread> m_thread;
    qint64 m_startMs{0};
    bool m_preparing{false};
    QString m_cachedWav;
};

}
