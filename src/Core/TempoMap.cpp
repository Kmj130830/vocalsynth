#include "Core/TempoMap.h"
#include <algorithm>
namespace myvocal {
TempoMap::TempoMap(){m_events.push_back({0,120.0});}
double TempoMap::tickToSeconds(double tick,double ppq)const{if(tick<=0)return 0;double sec=0,last=0;double bpm0=120;for(const auto&e:m_events){if(e.tick<=last)continue;if(e.tick>=tick)break;sec+=(e.tick-last)*(60.0/(bpm0*ppq));last=e.tick;bpm0=e.bpm;}sec+=(tick-last)*(60.0/(bpm0*ppq));return sec;}
double TempoMap::secondsToTick(double sec,double ppq)const{if(sec<=0)return 0;double remain=sec,lastTick=0;double bpm0=120;for(const auto&e:m_events){if(e.tick<=lastTick)continue;double span=(e.tick-lastTick)*(60.0/(bpm0*ppq));if(remain<=span)return lastTick+remain*(bpm0*ppq/60.0);remain-=span;lastTick=e.tick;bpm0=e.bpm;}return lastTick+remain*(bpm0*ppq/60.0);}
void TempoMap::setBpm(double b){m_events[0].bpm=std::max(1.0,b);} double TempoMap::bpm()const noexcept{return m_events.first().bpm;} const QVector<TempoEvent>&TempoMap::events()const noexcept{return m_events;} void TempoMap::addTempo(qint64 t,double b){m_events.push_back({std::max<qint64>(0,t),std::max(1.0,b)});std::sort(m_events.begin(),m_events.end(),[](auto&a,auto&b){return a.tick<b.tick;});}
}
