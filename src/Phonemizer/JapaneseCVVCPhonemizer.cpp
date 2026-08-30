#include "Phonemizer/JapaneseCVVCPhonemizer.h"
#include "Phonemizer/AliasResolver.h"
#include "Singer/Singer.h"
namespace myvocal { QString JapaneseCVVCPhonemizer::name()const{return QStringLiteral("Japanese CVVC");} std::vector<Phoneme>JapaneseCVVCPhonemizer::process(const std::vector<Note>&notes,const Singer&s){AliasResolver r(s);std::vector<Phoneme>out;for(size_t i=0;i<notes.size();++i){QString l=notes[i].getLyric();auto e=r.resolve(l);if(!e){const int sp=l.indexOf(' ');if(sp>0)e=r.resolve(l.mid(sp));}Phoneme p{l,static_cast<double>(notes[i].getStartTick()),static_cast<double>(notes[i].getDurationTick()),0,0,0,0,0,i};if(e){p.preutterance=e->preutterance;p.overlap=e->overlap;p.offset=e->offset;p.consonant=e->consonant;p.cutoff=e->cutoff;}out.push_back(std::move(p));}return out;} }
