#include "Renderer/Renderer.h"

#include "Phonemizer/AliasResolver.h"
#include "Phonemizer/DefaultCVPhonemizer.h"
#include "Phonemizer/HybridJapanesePhonemizer.h"
#include "Phonemizer/JapaneseCVVCPhonemizer.h"
#include "Phonemizer/JapaneseVCVPhonemizer.h"
#include "Phonemizer/KoreanCBNNPhonemizer.h"
#include "Phonemizer/KoreanVCVPhonemizer.h"
#include "Renderer/MoresamplerProcess.h"
#include "Renderer/WaveAssembler.h"
#include "Singer/Singer.h"
#include "Singer/SingerManager.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace myvocal {
namespace {
std::unique_ptr<IPhonemizer> makePhonemizer(const QString& name)
{
    if (name == QStringLiteral("Japanese VCV")) return std::make_unique<JapaneseVCVPhonemizer>();
    if (name == QStringLiteral("Japanese CVVC")) return std::make_unique<JapaneseCVVCPhonemizer>();
    if (name == QStringLiteral("Hybrid Japanese") || name == QStringLiteral("Japanese VCV / CVVC Hybrid")) return std::make_unique<HybridJapanesePhonemizer>();
    if (name == QStringLiteral("Korean VCV")) return std::make_unique<KoreanVCVPhonemizer>();
    if (name == QStringLiteral("Korean CBNN")) return std::make_unique<KoreanCBNNPhonemizer>();
    return std::make_unique<DefaultCVPhonemizer>();
}
QString midiToToneName(int midi)
{
    static const QStringList names = {QStringLiteral("C"),QStringLiteral("C#"),QStringLiteral("D"),QStringLiteral("D#"),QStringLiteral("E"),QStringLiteral("F"),QStringLiteral("F#"),QStringLiteral("G"),QStringLiteral("G#"),QStringLiteral("A"),QStringLiteral("A#"),QStringLiteral("B")};
    midi = std::clamp(midi, 0, 127); return names.at(midi % 12) + QString::number(midi / 12 - 1);
}
double tickToMs(const TempoMap& tempo, qint64 tick, double ppq)
{ return tempo.tickToSeconds(static_cast<double>(tick), ppq) * 1000.0; }
QString encodeInt12(const std::vector<int>& values)
{
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    QString result; result.reserve(static_cast<int>(values.size() * 2)); if (values.empty()) return QStringLiteral("AA");
    QString previous; int duplicates = 0;
    for (int value : values) {
        value = std::clamp(value, -2048, 2047); if (value < 0) value += 4096;
        QString encoded; encoded.append(QChar(table[(value >> 6) & 0x3f])); encoded.append(QChar(table[value & 0x3f]));
        if (previous.isEmpty()) { previous = encoded; result += encoded; }
        else if (previous == encoded) ++duplicates;
        else { if (duplicates > 0) result += QStringLiteral("#%1#").arg(duplicates); result += encoded; previous = encoded; duplicates = 0; }
    }
    if (duplicates > 0) result += QStringLiteral("#%1#").arg(duplicates); return result;
}
QString pitchBendFor(const Note& note, const Phoneme& phoneme, const TempoMap& tempo, double ppq, double renderLengthMs)
{
    const auto& curve = note.getPitchCurve();
    const int sampleCount = std::max(1, static_cast<int>(std::ceil(renderLengthMs / 5.0)));
    std::vector<int> values; values.reserve(static_cast<size_t>(sampleCount));
    const double noteStartMs = tickToMs(tempo, note.getStartTick(), ppq);
    const double phonemeStartMs = tickToMs(tempo, static_cast<qint64>(phoneme.startTick), ppq);
    const double curveOriginMs = phonemeStartMs - noteStartMs;
    for (int i = 0; i < sampleCount; ++i) {
        const double sampleMs = std::min(renderLengthMs, static_cast<double>(i) * 5.0);
        const double curveTimeMs = curveOriginMs + sampleMs;
        const double offset = curve.points().empty() ? 0.0 : curve.evaluateSmooth(curveTimeMs);
        values.push_back(static_cast<int>(std::llround(offset * 100.0)));
    }
    return encodeInt12(values);
}
}
Renderer::Renderer(SingerManager* manager, QObject* parent) : QObject(parent), m_singerManager(manager) {}
void Renderer::setResampler(const QString& path) { m_resampler = path.trimmed(); }
bool Renderer::renderProject(const Project& project, const QString& output, QString* error)
{
    if (!m_singerManager) { if (error) *error = QStringLiteral("SingerManager is unavailable."); return false; }
    if (m_resampler.isEmpty()) { if (error) *error = QStringLiteral("Moresampler executable is not configured. Set it in Tools > Preferences."); return false; }
    if (!QFileInfo(m_resampler).isFile()) { if (error) *error = QStringLiteral("Moresampler executable not found: %1").arg(m_resampler); return false; }
    bool hasSolo = false; int totalNotes = 0;
    for (const auto& track : project.tracks()) hasSolo = hasSolo || track.solo();
    for (const auto& track : project.tracks()) if (!track.muted() && (!hasSolo || track.solo())) totalNotes += track.notes().size();
    if (totalNotes == 0) { if (error) *error = QStringLiteral("Project has no audible notes."); return false; }
    QTemporaryDir tempDir; if (!tempDir.isValid()) { if (error) *error = QStringLiteral("Unable to create a render temporary directory."); return false; }
    std::vector<RenderSegment> segments; MoresamplerProcess resampler; resampler.setExecutable(m_resampler); int processed = 0; int segmentIndex = 0;
    for (const auto& track : project.tracks()) {
        if (track.muted() || (hasSolo && !track.solo())) continue;
        std::shared_ptr<Singer> singer = m_singerManager->findByPath(track.singerPath());
        if (!singer && !track.singerPath().isEmpty()) { singer = std::make_shared<Singer>(std::filesystem::path(track.singerPath().toStdWString())); singer->load(); }
        if (!singer) { if (error) *error = QStringLiteral("No Singer is assigned to track '%1'.").arg(track.name()); return false; }
        if (!singer->isValid()) { if (error) *error = QStringLiteral("VoiceBank '%1' cannot be loaded: %2").arg(singer->info().name, singer->oto().error()); return false; }
        std::vector<Note> notes; notes.reserve(track.notes().size()); for (const auto& note : track.notes()) notes.push_back(note);
        auto phonemizer = makePhonemizer(track.phonemizer()); const auto phonemes = phonemizer->process(notes, *singer); AliasResolver resolver(*singer);
        for (const auto& phoneme : phonemes) {
            if (phoneme.noteIndex >= notes.size()) { if (error) *error = QStringLiteral("Phonemizer returned an invalid note index."); return false; }
            const Note& note = notes.at(phoneme.noteIndex); const auto oto = resolver.resolve(phoneme.alias);
            if (!oto) { if (error) *error = QStringLiteral("Alias '%1' was not found in VoiceBank '%2'.").arg(phoneme.alias, singer->info().name); return false; }
            const std::filesystem::path sourcePath = singer->path() / std::filesystem::path(oto->filename.toStdWString());
            const QString sourceWav = QString::fromStdWString(sourcePath.lexically_normal().wstring());
            if (!QFileInfo(sourceWav).isFile()) { if (error) *error = QStringLiteral("oto.ini source WAV not found for alias '%1': %2").arg(phoneme.alias, sourceWav); return false; }
            const QString outputWav = tempDir.path() + QStringLiteral("/segment_%1.wav").arg(segmentIndex++);
            const double actualLengthMs = std::max(1.0, tickToMs(project.tempoMap(), static_cast<qint64>(phoneme.lengthTick), project.ppq()));
            const double renderLengthMs = std::max(20.0, actualLengthMs);
            ResamplerRequest request; request.inputWav = sourceWav; request.outputWav = outputWav; request.noteName = midiToToneName(note.getMidiNote()); request.velocity = std::clamp(note.getVelocity(), 0, 200); request.flags = note.getFlags().trimmed(); if (request.flags.isEmpty()) request.flags = QStringLiteral("g0B0H0P100"); request.offsetMs = oto->offset; request.requiredLengthMs = renderLengthMs; request.consonantMs = oto->consonant; request.cutoffMs = oto->cutoff; request.volume = std::clamp(track.volume() * note.getIntensity() / 100.0, 0.0, 2.0) * 100.0; request.modulation = std::clamp(note.getModulation(), 0.0, 100.0); request.tempo = std::max(1.0, project.tempoMap().bpm()); request.pitchBend = pitchBendFor(note, phoneme, project.tempoMap(), project.ppq(), renderLengthMs);
            emit message(QStringLiteral("Rendering %1 / %2: %3").arg(processed + 1).arg(totalNotes).arg(phoneme.alias));
            const ResamplerResult result = resampler.render(request);
            if (!result.success) { if (error) *error = QStringLiteral("Moresampler failed for '%1': %2").arg(phoneme.alias, result.error); return false; }
            RenderSegment segment; segment.wavPath = outputWav; segment.startMs = qRound64(tickToMs(project.tempoMap(), static_cast<qint64>(phoneme.startTick), project.ppq())); segment.lengthMs = qRound64(renderLengthMs); segment.gain = std::clamp(track.volume(), 0.0, 2.0); segment.overlapMs = phoneme.overlap; segments.push_back(segment);
            ++processed; emit progress(std::min(99, processed * 100 / std::max(1, totalNotes)));
        }
    }
    if (segments.empty()) { if (error) *error = QStringLiteral("No phonemes were produced by the selected phonemizers."); return false; }
    WaveAssembler assembler; if (!assembler.assemble(segments, output, error)) return false; emit progress(100); emit message(QStringLiteral("Render complete: %1").arg(output)); return true;
}
}
