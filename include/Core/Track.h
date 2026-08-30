#pragma once
#include <QVector>
#include <QString>
#include "Core/Note.h"
namespace myvocal {
class Singer;
class Track { public: explicit Track(int id=0); int id()const noexcept; const QString& name()const noexcept; void setName(const QString&); bool muted()const noexcept; void setMuted(bool); bool solo()const noexcept; void setSolo(bool); double volume()const noexcept; void setVolume(double); double pan()const noexcept; void setPan(double); const QString& singerPath()const noexcept; void setSingerPath(const QString&); const QString& phonemizer()const noexcept; void setPhonemizer(const QString&); QVector<Note>& notes() noexcept; const QVector<Note>& notes()const noexcept; qint64 addNote(Note note); bool removeNoteById(qint64 id,Note* removed=nullptr); Note* findNote(qint64 id); const Note* findNote(qint64 id)const; private: int m_id; QString m_name{"Track 1"}; bool m_muted{false},m_solo{false}; double m_volume{1.0},m_pan{0.0}; QString m_singerPath,m_phonemizer{"Default CV"}; QVector<Note> m_notes; };
}
