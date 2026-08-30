#include "Core/SongTime.h"
#include <cmath>
namespace myvocal { double SongTime::tickToSeconds(double t,const TempoMap&m,double p){return m.tickToSeconds(t,p);} double SongTime::secondsToTick(double s,const TempoMap&m,double p){return m.secondsToTick(s,p);} double SongTime::tickToPixel(double t,double px,double p){return t/p*px;} double SongTime::pixelToTick(double x,double px,double p){return x/px*p;} double SongTime::midiToFrequency(double m){return 440.0*std::pow(2.0,(m-69.0)/12.0);} double SongTime::frequencyToMidi(double h){return h<=0?0:69.0+12.0*std::log2(h/440.0);} }
