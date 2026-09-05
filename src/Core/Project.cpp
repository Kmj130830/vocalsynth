#include "Core/Project.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace myvocal {

Project::Project()
{
    m_tracks.push_back(Track(0));
}

double Project::ppq() const noexcept { return m_ppq; }
TempoMap& Project::tempoMap() noexcept { return m_tempo; }
const TempoMap& Project::tempoMap() const noexcept { return m_tempo; }
int Project::timeSignatureNumerator() const noexcept { return m_tsN; }
int Project::timeSignatureDenominator() const noexcept { return m_tsD; }

void Project::setTimeSignature(int numerator, int denominator)
{
    m_tsN = std::max(1, numerator);
    m_tsD = std::max(1, denominator);
}

qint64 Project::gridTicks() const noexcept { return m_gridTicks; }
void Project::setGridTicks(qint64 ticks) noexcept { m_gridTicks = std::clamp<qint64>(ticks, 1, 3840); }
bool Project::snapEnabled() const noexcept { return m_snapEnabled; }
void Project::setSnapEnabled(bool enabled) noexcept { m_snapEnabled = enabled; }
bool Project::gridVisible() const noexcept { return m_gridVisible; }
void Project::setGridVisible(bool visible) noexcept { m_gridVisible = visible; }

QVector<Track>& Project::tracks() noexcept { return m_tracks; }
const QVector<Track>& Project::tracks() const noexcept { return m_tracks; }

Track& Project::addTrack()
{
    m_tracks.push_back(Track(m_tracks.size()));
    return m_tracks.back();
}

bool Project::removeTrack(int index)
{
    if (index < 0 || index >= m_tracks.size() || m_tracks.size() == 1) return false;
    m_tracks.removeAt(index);
    return true;
}

QVector<AudioClip>& Project::audioClips() noexcept { return m_audioClips; }
const QVector<AudioClip>& Project::audioClips() const noexcept { return m_audioClips; }

int Project::addAudioClip(const AudioClip& clip)
{
    m_audioClips.push_back(clip);
    return m_audioClips.size() - 1;
}

bool Project::removeAudioClip(int index)
{
    if (index < 0 || index >= m_audioClips.size()) return false;
    m_audioClips.removeAt(index);
    return true;
}

std::filesystem::path Project::path() const { return m_path; }
void Project::setPath(const std::filesystem::path& path) { m_path = path; }
QString Project::title() const { return m_title; }
void Project::setTitle(const QString& title) { m_title = title; }

bool Project::save(const std::filesystem::path& path, QString* error) const
{
    QSaveFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }

    QJsonObject root;
    root["formatVersion"] = 3;
    root["title"] = m_title;
    root["ppq"] = m_ppq;
    root["tempo"] = m_tempo.bpm();

    QJsonObject ts;
    ts["numerator"] = m_tsN;
    ts["denominator"] = m_tsD;
    root["timeSignature"] = ts;

    QJsonObject editor;
    editor["gridTicks"] = QString::number(m_gridTicks);
    editor["snapEnabled"] = m_snapEnabled;
    editor["gridVisible"] = m_gridVisible;
    root["editor"] = editor;

    QJsonArray tracks;
    for (const auto& track : m_tracks) {
        QJsonObject object;
        object["id"] = track.id();
        object["name"] = track.name();
        object["mute"] = track.muted();
        object["solo"] = track.solo();
        object["volume"] = track.volume();
        object["pan"] = track.pan();
        object["singer"] = track.singerPath();
        object["phonemizer"] = track.phonemizer();

        QJsonArray notes;
        for (const auto& note : track.notes()) notes.append(note.serialize());
        object["notes"] = notes;
        tracks.append(object);
    }
    root["tracks"] = tracks;

    QJsonArray audio;
    for (const auto& clip : m_audioClips) audio.append(clip.serialize());
    root["audioClips"] = audio;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

std::unique_ptr<Project> Project::load(const std::filesystem::path& path, QString* error)
{
    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return nullptr;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return nullptr;
    }

    auto project = std::make_unique<Project>();
    const QJsonObject root = document.object();
    project->m_path = path;
    project->m_title = root["title"].toString(QStringLiteral("Untitled"));
    project->m_ppq = root["ppq"].toDouble(480.0);
    project->m_tempo.setBpm(root["tempo"].toDouble(120.0));

    const QJsonObject ts = root["timeSignature"].toObject();
    project->setTimeSignature(ts["numerator"].toInt(4), ts["denominator"].toInt(4));

    const QJsonObject editor = root["editor"].toObject();
    project->m_gridTicks = std::clamp<qint64>(
        editor["gridTicks"].toString().toLongLong(), 1, 3840);
    if (editor["gridTicks"].isUndefined()) project->m_gridTicks = 120;
    project->m_snapEnabled = editor["snapEnabled"].isUndefined() ? true : editor["snapEnabled"].toBool(true);
    project->m_gridVisible = editor["gridVisible"].isUndefined() ? true : editor["gridVisible"].toBool(true);

    project->m_tracks.clear();
    for (const auto& value : root["tracks"].toArray()) {
        const QJsonObject object = value.toObject();
        Track track(object["id"].toInt(project->m_tracks.size()));
        track.setName(object["name"].toString(track.name()));
        track.setMuted(object["mute"].toBool(false));
        track.setSolo(object["solo"].toBool(false));
        track.setVolume(object["volume"].toDouble(1.0));
        track.setPan(object["pan"].toDouble(0.0));
        track.setSingerPath(object["singer"].toString());
        track.setPhonemizer(object["phonemizer"].toString(QStringLiteral("Default CV")));
        for (const auto& note : object["notes"].toArray()) {
            track.addNote(Note::deserialize(note.toObject()));
        }
        project->m_tracks.push_back(std::move(track));
    }

    if (project->m_tracks.isEmpty()) project->m_tracks.push_back(Track(0));

    for (const auto& value : root["audioClips"].toArray()) {
        project->m_audioClips.push_back(AudioClip::deserialize(value.toObject()));
    }

    return project;
}

}