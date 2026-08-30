#pragma once
#include "Phonemizer/IPhonemizer.h"
namespace myvocal { class JapaneseCVVCPhonemizer final: public IPhonemizer { public: QString name()const override; std::vector<Phoneme> process(const std::vector<Note>&,const Singer&) override; }; }
