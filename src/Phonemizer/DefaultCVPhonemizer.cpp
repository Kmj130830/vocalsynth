#include "Phonemizer/DefaultCVPhonemizer.h"
#include "Phonemizer/AliasResolver.h"
#include "Singer/Singer.h"
namespace myvocal { QString DefaultCVPhonemizer::name()const{return QStringLiteral("Default CV");} std::vector<Phoneme>DefaultCVPhonemizer::process(const std::vector<Note>&notes,const Singer&s){AliasResolver r(s);std::vector<Phoneme>out;for(size_t i=0;i<notes.size();++i){const auto&a=notes[i].getLyric();auto e=r.resolve(a);Phoneme p;p.alias=a;p.startTick=notes[i].getStartTick();p.lengthTick=notes[i].getDurationTick();p.noteIndex=i;if(e){p.preutterance=e->preutterance;p.overlap=e->overlap;p.offset=e->offset;p.consonant=e->consonant;p.cutoff=e->cutoff;}out.push_back(std::move(p));}return out;} }
