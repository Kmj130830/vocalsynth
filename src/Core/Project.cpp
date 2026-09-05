#include "Core/Project.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFile>

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
    for (int i = 0; i < m_tracks.size(); ++i) m_tracks[i].setName(m_tracks[i].name());
    return true;
}

QVector<AudioClip>& Project::audioClips() noexcept { return m_audioClips; }
const QVector<AudioClip>& Project::audioClips() const noexcept { return m_audioClips; }
int Project::addAudioClip(const AudioClip& clip) { m_audioClips.push_back(clip); return m_audioClips.size() - 1; }
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

QByteArray Project::serializeJson() const
{
    QJsonObject root;
    root[QStringLiteral("formatVersion")] = 4;
    root[QStringLiteral("title")] = m_title;
    root[QStringLiteral("ppq")] = m_ppq;
    root[QStringLiteral("tempo")] = m_tempo.bpm();

    QJsonObject ts;
    ts[QStringLiteral("numerator")] = m_tsN;
    ts[QStringLiteral("denominator")] = m_tsD;
    root[QStringLiteral("timeSignature")] = ts;

    QJsonObject editor;
    editor[QStringLiteral("gridTicks")] = QString::number(m_gridTicks);
    editor[QStringLiteral("snapEnabled")] = m_snapEnabled;
    editor[QStringLiteral("gridVisible")] = m_gridVisible;
    root[QStringLiteral("editor")] = editor;

    QJsonArray tracks;
    for (const auto& track : m_tracks) {
        QJsonObject object;
        object[QStringLiteral("id")] = track.id();
        object[QStringLiteral("name")] = track.name();
        object[QStringLiteral("mute")] = track.muted();
        object[QStringLiteral("solo")] = track.solo();
        object[QStringLiteral("volume")] = track.volume();
        object[QStringLiteral("pan")] = track.pan();
        object[QStringLiteral("singer")] = track.singerPath();
        object[QStringLiteral("phonemizer")] = track.phonemizer();
        QJsonArray notes;
        for (const auto& note : track.notes()) notes.append(note.serialize());
        object[QStringLiteral("notes")] = notes;
        tracks.append(object);
    }
    root[QStringLiteral("tracks")] = tracks;

    QJsonArray audio;
    for (const auto& clip : m_audioClips) audio.append(clip.serialize());
    root[QStringLiteral("audioClips")] = audio;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool Project::restoreJson(const QByteArray& json, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }

    const QJsonObject root = document.object();
    m_title = root[QStringLiteral("title")].toString(QStringLiteral("Untitled"));
    m_ppq = root[QStringLiteral("ppq")].toDouble(480.0);
    m_tempo.setBpm(root[QStringLiteral("tempo")].toDouble(120.0));

    const QJsonObject ts = root[QStringLiteral("timeSignature")].toObject();
    setTimeSignature(ts[QStringLiteral("numerator")].toInt(4), ts[QStringLiteral("denominator")].toInt(4));

    const QJsonObject editor = root[QStringLiteral("editor")].toObject();
    m_gridTicks = std::clamp<qint64>(editor[QStringLiteral("gridTicks")].toString().toLongLong(), 1, 3840);
    if (editor[QStringLiteral("gridTicks")].isUndefined()) m_gridTicks = 120;
    m_snapEnabled = editor[QStringLiteral("snapEnabled")].isUndefined() ? true : editor[QStringLiteral("snapEnabled")].toBool(true);
    m_gridVisible = editor[QStringLiteral("gridVisible")].isUndefined() ? true : editor[QStringLiteral("gridVisible")].toBool(true);

    QVector<Track> restoredTracks;
    for (const auto& value : root[QStringLiteral("tracks")].toArray()) {
        const QJsonObject object = value.toObject();
        Track track(object[QStringLiteral("id")].toInt(restoredTracks.size()));
        track.setName(object[QStringLiteral("name")].toString(track.name()));
        track.setMuted(object[QStringLiteral("mute")].toBool(false));
        track.setSolo(object[QStringLiteral("solo")].toBool(false));
        track.setVolume(object[QStringLiteral("volume")].toDouble(1.0));
        track.setPan(object[QStringLiteral("pan")].toDouble(0.0));
        track.setSingerPath(object[QStringLiteral("singer")].toString());
        track.setPhonemizer(object[QStringLiteral("phonemizer")].toString(QStringLiteral("Default CV")));
        for (const auto& note : object[QStringLiteral("notes")].toArray()) track.addNote(Note::deserialize(note.toObject()));
        restoredTracks.push_back(std::move(track));
    }
    if (restoredTracks.isEmpty()) restoredTracks.push_back(Track(0));
    m_tracks = std::move(restoredTracks);

    m_audioClips.clear();
    for (const auto& value : root[QStringLiteral("audioClips")].toArray()) m_audioClips.push_back(AudioClip::deserialize(value.toObject()));
    return true;
}

bool Project::save(const std::filesystem::path& path, QString* error) const
{
    QSaveFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(serializeJson());
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
    auto project = std::make_unique<Project>();
    if (!project->restoreJson(file.readAll(), error)) return nullptr;
    project->m_path = path;
    return project;
}

}