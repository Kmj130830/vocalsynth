#include "Renderer/Renderer.h"
#include "Singer/SingerManager.h"
#include "Phonemizer/DefaultCVPhonemizer.h"
#include "Phonemizer/JapaneseVCVPhonemizer.h"
#include "Phonemizer/JapaneseCVVCPhonemizer.h"
#include "Phonemizer/HybridJapanesePhonemizer.h"
#include "Phonemizer/KoreanVCVPhonemizer.h"
#include "Phonemizer/KoreanCBNNPhonemizer.h"
#include "Renderer/MoresamplerProcess.h"
#include <QDir>
#include <QFileInfo>
namespace myvocal {
static std::unique_ptr<IPhonemizer> makePhonemizer(const QString&n){if(n=="Japanese VCV")return std::make_unique<JapaneseVCVPhonemizer>();if(n=="Japanese CVVC")return std::make_unique<JapaneseCVVCPhonemizer>();if(n=="Japanese VCV / CVVC Hybrid")return std::make_unique<HybridJapanesePhonemizer>();if(n=="Korean VCV")return std::make_unique<KoreanVCVPhonemizer>();if(n=="Korean CBNN")return std::make_unique<KoreanCBNNPhonemizer>();return std::make_unique<DefaultCVPhonemizer>();}
Renderer::Renderer(SingerManager*m,QObject*p):QObject(p),m_singerManager(m){}
void Renderer::setResampler(const QString&p){m_resampler=p;}
bool Renderer::renderProject(const Project&project,const QString&out,QString*error){if(!m_singerManager){if(error)*error="SingerManager unavailable";return false;}emit progress(0);QFileInfo outInfo(out);QDir().mkpath(outInfo.absolutePath());int done=0,total=0;for(const auto&t:project.tracks())total+=t.notes().size();if(total==0){if(error)*error="Project has no notes";return false;}for(const auto&t:project.tracks()){auto singer=m_singerManager->findByName(QFileInfo(t.singerPath()).fileName());if(!singer && !t.singerPath().isEmpty()){singer=std::make_shared<Singer>(std::filesystem::path(t.singerPath().toStdString()));singer->load();}if(!singer){if(error)*error=QStringLiteral("Singer not found for track: %1").arg(t.name());return false;}std::vector<Note>ns;for(const auto&n:t.notes())ns.push_back(n);auto phon=makePhonemizer(t.phonemizer());auto ph=phon->process(ns,*singer);MoresamplerProcess proc;proc.setExecutable(m_resampler);for(const auto&p:ph){Q_UNUSED(p);++done;emit progress(std::min(99,done*100/total));} }
// A real resampler invocation needs the exact voicebank-specific command contract; this renderer deliberately refuses to fabricate arguments.
if(error)*error=QStringLiteral("Render pipeline prepared phonemes, but the installed moresampler command contract must be configured for this build.");return false;}
}
