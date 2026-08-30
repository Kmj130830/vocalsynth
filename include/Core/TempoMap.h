#pragma once
#include <QVector>
namespace myvocal {
struct TempoEvent { qint64 tick{0}; double bpm{120.0}; };
class TempoMap { public: TempoMap(); double tickToSeconds(double tick,double ppq) const; double secondsToTick(double sec,double ppq) const; void setBpm(double bpm); double bpm()const noexcept; const QVector<TempoEvent>& events()const noexcept; void addTempo(qint64 tick,double bpm); private: QVector<TempoEvent> m_events; };
}
