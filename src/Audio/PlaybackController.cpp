#include "Audio/PlaybackController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include "Renderer/Renderer.h"

namespace myvocal {

PlaybackController::PlaybackController(AudioEngine* audio, Renderer* renderer, QObject* parent)
    : QObject(parent), m_audio(audio), m_renderer(renderer)
{
    if (m_audio) {
        connect(m_audio, &AudioEngine::positionChanged,
                this, &PlaybackController::onAudioPosition);
    }
}

PlaybackController::~PlaybackController()
{
    cleanupThread();
}

void PlaybackController::setProject(Project* project)
{
    cleanupThread();
    m_project = project;
    m_cachedWav.clear();
    m_startMs = 0;
    if (m_audio) {
        m_audio->stop(false);
        m_audio->setBackingClips(project ? project->audioClips() : QVector<AudioClip>{});
    }
}

QString PlaybackController::cachePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/MyVocalSynthPlayback");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/project-cache.wav");
}

void PlaybackController::playFromMs(qint64 ms)
{
    if (!m_project || !m_audio || !m_renderer) {
        emit playbackError(QStringLiteral("Playback is not initialized."));
        return;
    }

    ms = qMax<qint64>(0, ms);
    m_startMs = ms;

    if (m_preparing) return;

    if (!m_cachedWav.isEmpty() && QFileInfo::exists(m_cachedWav)) {
        m_audio->load(m_cachedWav);
        m_audio->seek(ms);
        m_audio->setBackingClips(m_project->audioClips());
        m_audio->play();
        emit stateChanged(true);
        return;
    }

    prepareAndPlay(ms);
}

void PlaybackController::prepareAndPlay(qint64 startMs)
{
    cleanupThread();
    m_preparing = true;
    emit preparingChanged(true);

    const QString output = cachePath();
    m_cachedWav = output;
    Project* project = m_project;
    Renderer* renderer = m_renderer;

    m_thread.reset(QThread::create([this, project, renderer, output, startMs] {
        QString error;
        const bool ok = renderer && project && renderer->renderProject(*project, output, &error);
        QMetaObject::invokeMethod(this, [this, ok, error, output, startMs] {
            m_preparing = false;
            emit preparingChanged(false);
            if (!ok) {
                m_cachedWav.clear();
                emit playbackError(error.isEmpty()
                    ? QStringLiteral("Voice playback rendering failed.")
                    : error);
                return;
            }
            if (!m_audio || !m_project) return;
            m_audio->load(output);
            m_audio->setBackingClips(m_project->audioClips());
            m_audio->seek(startMs);
            m_audio->play();
            emit stateChanged(true);
        }, Qt::QueuedConnection);
    }));

    connect(m_thread.get(), &QThread::finished, this, [this] {
        if (m_thread && !m_thread->isRunning()) {
            m_thread.reset();
        }
    });
    m_thread->start();
}

void PlaybackController::pause()
{
    if (m_audio) m_audio->pause();
    emit stateChanged(false);
}

void PlaybackController::stop(bool returnToStart)
{
    if (!m_audio) return;
    const qint64 current = m_audio->position();
    m_audio->stop(!returnToStart);
    if (returnToStart) {
        m_audio->seek(m_startMs);
        emit positionChanged(m_startMs);
    } else {
        m_audio->seek(current);
        emit positionChanged(current);
    }
    emit stateChanged(false);
}

void PlaybackController::seekMs(qint64 ms)
{
    if (!m_audio) return;
    m_audio->seek(qMax<qint64>(0, ms));
}

bool PlaybackController::isPreparing() const noexcept
{
    return m_preparing;
}

qint64 PlaybackController::playbackStartMs() const noexcept
{
    return m_startMs;
}

void PlaybackController::onAudioPosition(qint64 ms)
{
    emit positionChanged(ms);
}

void PlaybackController::cleanupThread()
{
    if (!m_thread) return;
    if (m_thread->isRunning()) {
        m_thread->requestInterruption();
        m_thread->quit();
        m_thread->wait();
    }
    m_thread.reset();
    m_preparing = false;
}

}
