#pragma once
#include <QJsonObject>
#include <QString>
#include <memory>
#include "Core/PitchCurve.h"
namespace myvocal {
struct Vibrato { bool enabled{false}; double delay{0.0}; double length{0.0}; double depth{0.0}; double rate{5.5}; double fadeIn{0.1}; double fadeOut{0.1}; };
class Note {
public:
    Note();
    explicit Note(qint64 id);
    qint64 getId() const noexcept;
    qint64 getStartTick() const noexcept;
    void setStartTick(qint64 v);
    qint64 getDurationTick() const noexcept;
    void setDurationTick(qint64 v);
    qint64 getEndTick() const noexcept;
    int getMidiNote() const noexcept;
    void setMidiNote(int v);
    const QString& getLyric() const noexcept;
    void setLyric(const QString& v);
    int getVelocity() const noexcept;
    void setVelocity(int v);
    const PitchCurve& getPitchCurve() const noexcept;
    PitchCurve& getPitchCurve() noexcept;
    const Vibrato& getVibrato() const noexcept;
    Vibrato& getVibrato() noexcept;
    double getPreutterance() const noexcept;
    void setPreutterance(double v);
    double getOverlap() const noexcept;
    void setOverlap(double v);
    double getIntensity() const noexcept;
    void setIntensity(double v);
    double getModulation() const noexcept;
    void setModulation(double v);
    const QString& getFlags() const noexcept;
    void setFlags(const QString& v);
    bool isSelected() const noexcept;
    void setSelected(bool v);
    Note clone() const;
    QJsonObject serialize() const;
    static Note deserialize(const QJsonObject& o);
private:
    qint64 m_id{0}; qint64 m_startTick{0}; qint64 m_durationTick{480}; int m_midiNote{60}; QString m_lyric{"a"}; int m_velocity{100}; PitchCurve m_pitchCurve; Vibrato m_vibrato; double m_preutterance{0.0}; double m_overlap{0.0}; double m_intensity{100.0}; double m_modulation{0.0}; QString m_flags; bool m_selected{false};
};
}
