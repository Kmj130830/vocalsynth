#include "Audio/PlaybackController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include "Renderer/Renderer.h"

namespace myvocal {
namespace {

bool hasAudibleNotes(const Project& project)
{
    bool hasSolo = false;
    for (const auto& track : project.tracks()) hasSolo = hasSolo || track.solo();
    for (const auto& track : project.tracks()) {
        if (!track.muted() && (!hasSolo || track.solo()) && !track.notes().isEmpty()) return true;
    }
    return false;
}

bool hasBackingAudio(const Project& project)
{
    for (const auto& clip : project.audioClips()) {
        if (!clip.muted && QFileInfo::isFile(clip.path)) return true;
    }
    return false;
}

}

PlaybackController::PlaybackController(AudioEngine* audio, Renderer* renderer, QObject* parent)
    : QObject(parent), m_audio(audio), m_renderer(renderer), m_preRenderTimer(std::make_unique<QTimer>())
{
    m_preRenderTimer->setSingleShot(true);
    m_preRenderTimer->setInterval(250);
    connect(m_preRenderTimer.get(), &QTimer::timeout, this, [this] { startRender(false); });
    if (m_audio) connect(m_audio, &AudioEngine::positionChanged, this, &PlaybackController::positionChanged);
}

PlaybackController::~PlaybackController() { cleanupThread(); }

void PlaybackController::setProject(Project* project)
{
    cleanupThread();
    m_project = project;
    ++m_generation;
    m_cachedWav.clear();
    m_pendingPlayMs = -1;
    m_preparing = false;
    m_backgroundRendering = false;
    if (m_preRenderTimer) m_preRenderTimer->stop();
    if (m_audio) {
        m_audio->stop(false);
        m_audio->setBackingClips(project ? project->audioClips() : QVector<AudioClip>{});
    }
    if (m_project && hasAudibleNotes(*m_project)) schedulePreRender();
}

void PlaybackController::invalidateCache()
{
    ++m_generation;
    m_cachedWav.clear();
    if (m_preRenderTimer) {
        m_preRenderTimer->stop();
        if (m_project && hasAudibleNotes(*m_project)) m_preRenderTimer->start();
    }
}

QString PlaybackController::cachePath(quint64 generation) const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/MyVocalSynthPlayback");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/project-cache-%1.wav").arg(generation);
}

void PlaybackController::schedulePreRender()
{
    if (m_preRenderTimer && m_project && hasAudibleNotes(*m_project)) m_preRenderTimer->start();
}

void PlaybackController::playFromMs(qint64 ms)
{
    if (!m_project || !m_audio) {
        emit playbackError(QStringLiteral("Playback is not initialized."));
        return;
    }
    ms = qMax<qint64>(0, ms);
    m_startMs = ms;

    if (!hasAudibleNotes(*m_project)) {
        if (!hasBackingAudio(*m_project)) {
            emit playbackError(QStringLiteral("There are no audible notes or imported audio clips."));
            return;
        }
        m_audio->setBackingClips(m_project->audioClips());
        m_audio->seek(ms);
        m_audio->play();
        emit stateChanged(true);
        return;
    }

    if (!m_renderer) {
        emit playbackError(QStringLiteral("Renderer is not initialized."));
        return;
    }

    if (!m_cachedWav.isEmpty() && QFileInfo::isFile(m_cachedWav)) {
        m_audio->load(m_cachedWav);
        m_audio->setBackingClips(m_project->audioClips());
        m_audio->seek(ms);
        m_audio->play();
        emit stateChanged(true);
        return;
    }

    m_pendingPlayMs = ms;
    if (m_backgroundRendering || m_preparing) return;
    startRender(true);
}

void PlaybackController::startRender(bool playAfter)
{
    if (!m_project || !m_renderer || !hasAudibleNotes(*m_project)) return;
    if (m_thread && m_thread->isRunning()) {
        if (playAfter) m_pendingPlayMs = m_startMs;
        return;
    }

    const quint64 generation = m_generation;
    const QString output = cachePath(generation);
    Project* project = m_project;
    Renderer* renderer = m_renderer;
    m_backgroundRendering = !playAfter;
    m_preparing = playAfter;
    if (playAfter) emit preparingChanged(true);

    m_thread.reset(QThread::create([this, project, renderer, output, generation] {
        QString error;
        const bool ok = renderer && project && renderer->renderProject(*project, output, &error);
        QMetaObject::invokeMethod(this, [this, ok, error, output, generation] {
            const bool current = generation == m_generation;
            if (ok && current && QFileInfo::isFile(output)) {
                const QString old = m_cachedWav;
                m_cachedWav = output;
                if (!old.isEmpty() && old != output) QFile::remove(old);
            } else {
                QFile::remove(output);
            }

            const qint64 pending = m_pendingPlayMs;
            const bool shouldPlay = current && pending >= 0;
            m_pendingPlayMs = -1;
            const bool wasPreparing = m_preparing;
            m_preparing = false;
            m_backgroundRendering = false;
            if (wasPreparing) emit preparingChanged(false);

            if (!ok && wasPreparing) {
                if (m_project && hasBackingAudio(*m_project) && m_audio) {
                    m_audio->setBackingClips(m_project->audioClips());
                    m_audio->seek(pending >= 0 ? pending : m_startMs);
                    m_audio->play();
                    emit stateChanged(true);
                } else {
                    emit playbackError(error.isEmpty() ? QStringLiteral("Voice playback rendering failed.") : error);
                }
            } else if (current && shouldPlay && !m_cachedWav.isEmpty() && m_audio && m_project) {
                m_audio->load(m_cachedWav);
                m_audio->setBackingClips(m_project->audioClips());
                m_audio->seek(pending);
                m_audio->play();
                emit stateChanged(true);
            } else if (!current) {
                schedulePreRender();
            }
        }, Qt::QueuedConnection);
    }));

    connect(m_thread.get(), &QThread::finished, this, [this] {
        if (m_thread && !m_thread->isRunning()) m_thread.reset();
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
    m_pendingPlayMs = -1;
    if (returnToStart) {
        m_audio->stop(false);
        m_audio->seek(m_startMs);
        emit positionChanged(m_startMs);
    } else {
        m_audio->stop(true);
        m_audio->seek(current);
        emit positionChanged(current);
    }
    emit stateChanged(false);
}

void PlaybackController::seekMs(qint64 ms)
{
    if (m_audio) m_audio->seek(qMax<qint64>(0, ms));
}

bool PlaybackController::isPreparing() const noexcept { return m_preparing || m_backgroundRendering; }
qint64 PlaybackController::playbackStartMs() const noexcept { return m_startMs; }

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
    m_backgroundRendering = false;
}

}