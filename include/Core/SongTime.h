#pragma once
#include <QtGlobal>
#include "Core/TempoMap.h"
namespace myvocal {
class SongTime { public: static constexpr double PPQ=480.0; static double tickToSeconds(double tick,const TempoMap& map,double ppq=PPQ); static double secondsToTick(double sec,const TempoMap& map,double ppq=PPQ); static double tickToPixel(double tick,double pxPerBeat,double ppq=PPQ); static double pixelToTick(double px,double pxPerBeat,double ppq=PPQ); static double midiToFrequency(double midi); static double frequencyToMidi(double hz); };
}
