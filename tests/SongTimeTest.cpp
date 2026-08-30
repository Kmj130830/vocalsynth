#include "Core/SongTime.h"
#include <cassert>
#include <cmath>
void timeTest(){assert(std::abs(myvocal::SongTime::midiToFrequency(69)-440.0)<1e-9);}
