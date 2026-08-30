#include "Core/Note.h"

#include <algorithm>

namespace myvocal {

namespace {
qint64 allocateEditorNoteId()
{
    static qint64 nextId = 1000000000LL;
    return nextId++;
}
}

Note::Note()
    : m_id(allocateEditorNoteId())
{
}

Note::Note(qint64 id)
    : m_id(id > 0 ? id : allocateEditorNoteId())
{
}

qint64 Note::getId() const noexcept { return m_id; }
qint64 Note::getStartTick() const noexcept { return m_startTick; }
void Note::setStartTick(qint64 v) { m_startTick = std::max<qint64>(0, v); }
qint64 Note::getDurationTick() const noexcept { return m_durationTick; }
void Note::setDurationTick(qint64 v) { m_durationTick = std::max<qint64>(1, v); }
qint64 Note::getEndTick() const noexcept { return m_startTick + m_durationTick; }
int Note::getMidiNote() const noexcept { return m_midiNote; }
void Note::setMidiNote(int v) { m_midiNote = std::clamp(v, 0, 127); }
const QString& Note::getLyric() const noexcept { return m_lyric; }
void Note::setLyric(const QString& v) { m_lyric = v.isEmpty() ? QStringLiteral("a") : v; }
int Note::getVelocity() const noexcept { return m_velocity; }
void Note::setVelocity(int v) { m_velocity = std::clamp(v, 1, 127); }
const PitchCurve& Note::getPitchCurve() const noexcept { return m_pitchCurve; }
PitchCurve& Note::getPitchCurve() noexcept { return m_pitchCurve; }
const Vibrato& Note::getVibrato() const noexcept { return m_vibrato; }
Vibrato& Note::getVibrato() noexcept { return m_vibrato; }
double Note::getPreutterance() const noexcept { return m_preutterance; }
void Note::setPreutterance(double v) { m_preutterance = v; }
double Note::getOverlap() const noexcept { return m_overlap; }
void Note::setOverlap(double v) { m_overlap = v; }
double Note::getIntensity() const noexcept { return m_intensity; }
void Note::setIntensity(double v) { m_intensity = v; }
double Note::getModulation() const noexcept { return m_modulation; }
void Note::setModulation(double v) { m_modulation = v; }
const QString& Note::getFlags() const noexcept { return m_flags; }
void Note::setFlags(const QString& v) { m_flags = v; }
bool Note::isSelected() const noexcept { return m_selected; }
void Note::setSelected(bool v) { m_selected = v; }

Note Note::clone() const
{
    Note clone = *this;
    clone.m_id = allocateEditorNoteId();
    clone.m_selected = false;
    return clone;
}

QJsonObject Note::serialize() const
{
    QJsonObject object;
    object["id"] = m_id;
    object["startTick"] = QString::number(m_startTick);
    object["durationTick"] = QString::number(m_durationTick);
    object["midiNote"] = m_midiNote;
    object["lyric"] = m_lyric;
    object["velocity"] = m_velocity;
    object["pitchCurve"] = m_pitchCurve.serialize();

    QJsonObject vibrato;
    vibrato["enabled"] = m_vibrato.enabled;
    vibrato["delay"] = m_vibrato.delay;
    vibrato["length"] = m_vibrato.length;
    vibrato["depth"] = m_vibrato.depth;
    vibrato["rate"] = m_vibrato.rate;
    vibrato["fadeIn"] = m_vibrato.fadeIn;
    vibrato["fadeOut"] = m_vibrato.fadeOut;
    object["vibrato"] = vibrato;

    object["preutterance"] = m_preutterance;
    object["overlap"] = m_overlap;
    object["intensity"] = m_intensity;
    object["modulation"] = m_modulation;
    object["flags"] = m_flags;
    return object;
}

Note Note::deserialize(const QJsonObject& object)
{
    Note note(object["id"].toVariant().toLongLong());
    note.m_startTick = object["startTick"].toString().toLongLong();
    note.m_durationTick = object["durationTick"].toString().toLongLong();
    note.m_midiNote = object["midiNote"].toInt(60);
    note.m_lyric = object["lyric"].toString("a");
    note.m_velocity = object["velocity"].toInt(100);
    note.m_pitchCurve = PitchCurve::deserialize(object["pitchCurve"].toArray());

    const auto vibrato = object["vibrato"].toObject();
    note.m_vibrato.enabled = vibrato["enabled"].toBool();
    note.m_vibrato.delay = vibrato["delay"].toDouble();
    note.m_vibrato.length = vibrato["length"].toDouble();
    note.m_vibrato.depth = vibrato["depth"].toDouble();
    note.m_vibrato.rate = vibrato["rate"].toDouble(5.5);
    note.m_vibrato.fadeIn = vibrato["fadeIn"].toDouble();
    note.m_vibrato.fadeOut = vibrato["fadeOut"].toDouble();

    note.m_preutterance = object["preutterance"].toDouble();
    note.m_overlap = object["overlap"].toDouble();
    note.m_intensity = object["intensity"].toDouble(100);
    note.m_modulation = object["modulation"].toDouble();
    note.m_flags = object["flags"].toString();
    note.m_selected = false;
    return note;
}

}
