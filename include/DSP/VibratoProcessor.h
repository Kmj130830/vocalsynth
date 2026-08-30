#pragma once
#include <vector>
#include "Core/Note.h"
namespace myvocal { class VibratoProcessor { public: static void apply(std::vector<float>&samples,double sampleRate,const Vibrato&v); }; }
