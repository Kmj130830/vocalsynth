#pragma once
#include <vector>
#include "Core/Phoneme.h"
#include "Core/Note.h"
namespace myvocal { class Singer; class IPhonemizer { public: virtual ~IPhonemizer()=default; virtual QString name()const=0; virtual std::vector<Phoneme> process(const std::vector<Note>&notes,const Singer&singer)=0; }; }
