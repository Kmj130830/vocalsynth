#include "Phonemizer/KoreanVCVPhonemizer.h"
#include "Phonemizer/AliasResolver.h"
#include "Singer/Singer.h"
namespace myvocal {
static QString vowelKey(const QString&s){if(s.isEmpty())return {};const QChar c=s.at(0); if(c.unicode()<0xAC00||c.unicode()>0xD7A3)return s; const int idx=c.unicode()-0xAC00; static const QStringList v={"a","ae","ya","yae","eo","e","yeo","ye","o","wa","wae","oe","yo","u","weo","we","wi","yu","eu","ui","i"}; return v.at((idx%588)/28%21);}
QString KoreanVCVPhonemizer::name()const{return QStringLiteral("Korean VCV");}
std::vector<Phoneme>KoreanVCVPhonemizer::process(const std::vector<Note>&notes,const Singer&s){AliasResolver r(s);std::vector<Phoneme>out;QString prev="a";for(size_t i=0;i<notes.size();++i){QString l=notes[i].getLyric().trimmed();QString alias=(i?prev+" "+l:l);auto e=r.resolve(alias);if(!e)e=r.resolve(l);Phoneme p{e?alias:l,(double)notes[i].getStartTick(),(double)notes[i].getDurationTick(),0,0,0,0,0,i};if(e){p.preutterance=e->preutterance;p.overlap=e->overlap;p.offset=e->offset;p.consonant=e->consonant;p.cutoff=e->cutoff;}out.push_back(std::move(p));prev=vowelKey(l);}return out;}
}
