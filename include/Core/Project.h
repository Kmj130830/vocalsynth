#pragma once
#include <QVector>
#include <QString>
#include <filesystem>
#include "Core/TempoMap.h"
#include "Core/Track.h"
namespace myvocal {
class Project { public: Project(); double ppq()const noexcept; TempoMap& tempoMap() noexcept; const TempoMap& tempoMap()const noexcept; int timeSignatureNumerator()const noexcept; int timeSignatureDenominator()const noexcept; void setTimeSignature(int n,int d); QVector<Track>& tracks() noexcept; const QVector<Track>& tracks()const noexcept; Track& addTrack(); bool removeTrack(int index); bool save(const std::filesystem::path& path,QString*error=nullptr)const; static std::unique_ptr<Project> load(const std::filesystem::path& path,QString*error=nullptr); std::filesystem::path path()const; void setPath(const std::filesystem::path&); QString title()const; void setTitle(const QString&); private: double m_ppq{480.0}; TempoMap m_tempo; int m_tsN{4},m_tsD{4}; QVector<Track> m_tracks; std::filesystem::path m_path; QString m_title{"Untitled"}; qint64 m_nextNoteId{1}; };
}
