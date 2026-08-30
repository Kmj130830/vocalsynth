#pragma once
#include <vector>
#include "Core/Note.h"
namespace myvocal { class PitchProcessor { public: static double semitoneAt(const Note&,double normalizedTime); static void applyVibrato(const Note&,std::vector<float>&samples,double sampleRate); }; }
