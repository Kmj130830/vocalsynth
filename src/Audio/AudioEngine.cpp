#include "Audio/AudioEngine.h"
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>

namespace myvocal {
namespace {
quint16 rd16(const QByteArray& b, qsizetype p) { return quint16(quint8(b.at(p)) | (quint16(quint8(b.at(p+1))) << 8)); }
quint32 rd32(const QByteArray& b, qsizetype p) { return quint32(quint8(b.at(p)) | (quint32(quint8(b.at(p+1))) << 8) | (quint32(quint8(b.at(p+2))) << 16) | (quint32(quint8(b.at(p+3))) << 24)); }
bool tag(const QByteArray& b, qsizetype p, const char* t) { return p + 4 <= b.size() && QByteArray(b.constData()+p,4) == QByteArray(t,4); }
}

AudioEngine::AudioEngine(QObject* parent) : QObject(parent) {
    m_positionTimer.setInterval(16);
    connect(&m_positionTimer, &QTimer::timeout, this, [this] { emit positionChanged(position()); });
}
AudioEngine::~AudioEngine() { destroySink(); stopBackingPlayers(); }

void AudioEngine::destroySink() {
    m_positionTimer.stop();
    if (m_sink) m_sink->stop();
    m_sink.reset();
    if (m_buffer.isOpen()) m_buffer.close();
}

bool AudioEngine::loadPcmWav(const QString& path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray b = f.readAll(); if (b.size() < 44 || !tag(b,0,"RIFF") || !tag(b,8,"WAVE")) return false;
    int format=0, channels=0, rate=0, bits=0; qsizetype data=-1, size=0, p=12;
    while (p+8 <= b.size()) {
        const quint32 n=rd32(b,p+4); const qsizetype begin=p+8; const qsizetype end=std::min<qsizetype>(b.size(),begin+n);
        if (tag(b,p,"fmt ") && begin+16<=b.size()) { format=rd16(b,begin); channels=rd16(b,begin+2); rate=int(rd32(b,begin+4)); bits=rd16(b,begin+14); }
        else if (tag(b,p,"data")) { data=begin; size=end-begin; break; }
        p=end+(n&1u);
    }
    if (format!=1 || channels<=0 || rate<=0 || bits!=16 || data<0 || size<=0) return false;
    m_pcm=QByteArray(b.constData()+data,size);
    m_format=QAudioFormat(); m_format.setSampleRate(rate); m_format.setChannelCount(channels); m_format.setSampleFormat(QAudioFormat::Int16);
    m_buffer.setData(m_pcm); m_buffer.open(QIODevice::ReadOnly); m_seekMs=0; m_seekByte=0; m_loaded=true; return true;
}

void AudioEngine::load(const QString& path) {
    destroySink(); m_loaded=false; m_pcm.clear();
    if (!QFileInfo::exists(path) || !loadPcmWav(path)) emit mediaError(QStringLiteral("Unable to open rendered PCM WAV: %1").arg(path));
}

void AudioEngine::stopBackingPlayers() { for (auto& p:m_backingPlayers) if(p) p->stop(); }
void AudioEngine::setBackingClips(const QVector<AudioClip>& clips) {
    stopBackingPlayers(); m_backingPlayers.clear(); m_backingOutputs.clear(); m_clips=clips;
    for (const auto& c:m_clips) { if(c.muted || !QFileInfo::exists(c.path)) continue; auto p=std::make_unique<QMediaPlayer>(); auto o=std::make_unique<QAudioOutput>(); o->setVolume(std::clamp(c.volume,0.0,2.0)); p->setAudioOutput(o.get()); p->setSource(QUrl::fromLocalFile(c.path)); m_backingOutputs.push_back(std::move(o)); m_backingPlayers.push_back(std::move(p)); }
}
void AudioEngine::syncBackingPlayers(qint64 ms,bool start) {
    std::size_t i=0; for(const auto& c:m_clips) { if(c.muted || !QFileInfo::exists(c.path)) continue; if(i>=m_backingPlayers.size()) break; auto* p=m_backingPlayers[i++].get(); if(!p) continue; const qint64 local=ms-c.startMs+c.offsetMs; if(local<0){p->pause();continue;} p->setPosition(local); if(start)p->play(); }
}
qint64 AudioEngine::pcmPositionMs() const {
    if(!m_loaded || m_format.sampleRate()<=0 || m_format.channelCount()<=0) return m_seekMs;
    const qint64 bps=qint64(m_format.sampleRate())*m_format.channelCount()*2;
    return m_seekMs + std::max<qint64>(0,qint64(m_buffer.pos())-m_seekByte)*1000/bps;
}
void AudioEngine::play() {
    if(!m_loaded) return;
    if(!m_sink) {
        m_sink=std::make_unique<QAudioSink>(m_format);
        m_sink->setBufferSize(std::max(4096,m_format.sampleRate()*m_format.channelCount()*2/20));
        connect(m_sink.get(),&QAudioSink::stateChanged,this,[this](QAudio::State s){ if(s==QAudio::StoppedState && m_sink && m_sink->error()!=QAudio::NoError) emit mediaError(QStringLiteral("Audio output error: %1").arg(int(m_sink->error()))); if(s==QAudio::IdleState){m_positionTimer.stop();emit playbackStateChanged(false);} });
    }
    m_seekMs=pcmPositionMs(); m_seekByte=m_buffer.pos(); m_sink->start(&m_buffer); m_positionTimer.start(); syncBackingPlayers(m_seekMs,true); emit playbackStateChanged(true);
}
void AudioEngine::pause() {
    if(m_sink){m_seekMs=pcmPositionMs();m_seekByte=m_buffer.pos();m_sink->suspend();} for(auto& p:m_backingPlayers)if(p)p->pause(); m_positionTimer.stop(); emit positionChanged(m_seekMs); emit playbackStateChanged(false);
}
void AudioEngine::stop(bool preservePosition) {
    const qint64 cur=position(); if(m_sink)m_sink->stop(); m_positionTimer.stop(); if(preservePosition) seek(cur); else {if(m_buffer.isOpen())m_buffer.seek(0);m_seekMs=0;m_seekByte=0;stopBackingPlayers();emit positionChanged(0);} emit playbackStateChanged(false);
}
void AudioEngine::seek(qint64 ms) {
    if(!m_loaded || m_format.sampleRate()<=0 || m_format.channelCount()<=0) return; const bool playing=isPlaying(); if(m_sink&&playing)m_sink->stop();
    const qint64 bps=qint64(m_format.sampleRate())*m_format.channelCount()*2; const qint64 frame=qint64(m_format.channelCount())*2; qint64 byte=qMax<qint64>(0,ms)*bps/1000; if(frame>0)byte-=byte%frame; byte=std::min<qint64>(byte,m_pcm.size()); m_buffer.seek(byte); m_seekMs=qMax<qint64>(0,ms);m_seekByte=byte;emit positionChanged(m_seekMs);syncBackingPlayers(m_seekMs,playing);if(playing)play();
}
bool AudioEngine::isPlaying() const { return m_sink && m_sink->state()==QAudio::ActiveState; }
qint64 AudioEngine::position() const { return pcmPositionMs(); }
}