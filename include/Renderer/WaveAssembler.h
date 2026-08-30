#pragma once
#include "Renderer/IAudioAssembler.h"
namespace myvocal { class WaveAssembler final: public IAudioAssembler { public: bool assemble(const std::vector<RenderSegment>&,const QString&,QString*) override; }; }
