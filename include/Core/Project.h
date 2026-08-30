#pragma once

#include <QVector>
#include <QString>
#include <filesystem>

#include "Core/AudioClip.h"
#include "Core/TempoMap.h"
#include "Core/Track.h"

namespace myvocal {

class Project {
public:
    Project();

    double ppq() const noexcept;
    TempoMap& tempoMap() noexcept;
    const TempoMap& tempoMap() const noexcept;

    int timeSignatureNumerator() const noexcept;
    int timeSignatureDenominator() const noexcept;
    void setTimeSignature(int numerator, int denominator);

    QVector<Track>& tracks() noexcept;
    const QVector<Track>& tracks() const noexcept;
    Track& addTrack();
    bool removeTrack(int index);

    QVector<AudioClip>& audioClips() noexcept;
    const QVector<AudioClip>& audioClips() const noexcept;
    int addAudioClip(const AudioClip& clip);
    bool removeAudioClip(int index);

    bool save(const std::filesystem::path& path, QString* error = nullptr) const;
    static std::unique_ptr<Project> load(const std::filesystem::path& path,
                                         QString* error = nullptr);

    std::filesystem::path path() const;
    void setPath(const std::filesystem::path& path);

    QString title() const;
    void setTitle(const QString& title);

private:
    double m_ppq{480.0};
    TempoMap m_tempo;
    int m_tsN{4};
    int m_tsD{4};
    QVector<Track> m_tracks;
    QVector<AudioClip> m_audioClips;
    std::filesystem::path m_path;
    QString m_title{QStringLiteral("Untitled")};
};

}