#include "Renderer/WaveAssembler.h"
#include <QFile>
#include <QDataStream>
#include <QAudioFormat>
#include <QAudioDecoder>
#include <QEventLoop>
#include <QAudioBuffer>
#include <vector>
#include <cmath>
#include <algorithm>
namespace myvocal {
static bool writeWav(const QString&path,const std::vector<float>&samples,int rate){QFile f(path);if(!f.open(QIODevice::WriteOnly))return false;QDataStream d(&f);d.setByteOrder(QDataStream::LittleEndian);auto u16=[&](quint16 x){d<<x;};auto u32=[&](quint32 x){d<<x;};const quint32 dataBytes=samples.size()*2;d.writeRawData("RIFF",4);u32(36+dataBytes);d.writeRawData("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(rate);u32(rate*2);u16(2);u16(16);d.writeRawData("data",4);u32(dataBytes);for(float v:samples){const auto s=static_cast<qint16>(std::clamp(v,-1.0f,1.0f)*32767.0f);d<<s;}return true;}
bool WaveAssembler::assemble(const std::vector<RenderSegment>&segments,const QString&output,QString*error){if(segments.empty()){if(error)*error=QStringLiteral("No render segments");return false;}auto ordered=segments;std::sort(ordered.begin(),ordered.end(),[](const auto&a,const auto&b){return a.startMs<b.startMs;});std::vector<float>mix(48000*60,0);size_t maxEnd=0;for(const auto&s:ordered){QFile f(s.wavPath);if(!f.open(QIODevice::ReadOnly)){if(error)*error=QStringLiteral("Cannot open %1").arg(s.wavPath);return false;}QByteArray bytes=f.readAll();if(bytes.size()<44||bytes.mid(0,4)!="RIFF"){if(error)*error=QStringLiteral("Invalid WAV: ")+s.wavPath;return false;}const int dataStart=44;const int count=(bytes.size()-dataStart)/2;const size_t start=std::max<qint64>(0,s.startMs)*48; if(mix.size()<start+count)mix.resize(start+count);const char*ptr=bytes.constData()+dataStart;for(int i=0;i<count;++i){qint16 q=*reinterpret_cast<const qint16*>(ptr+i*2);mix[start+i]+=q/32768.0f*static_cast<float>(s.gain);}maxEnd=std::max(maxEnd,start+static_cast<size_t>(count));}mix.resize(maxEnd);if(!writeWav(output,mix,48000)){if(error)*error=QStringLiteral("Cannot write output WAV");return false;}return true;}
}
