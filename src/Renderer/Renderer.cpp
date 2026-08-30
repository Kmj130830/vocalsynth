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
#include <memory>
#include <vector>

namespace myvocal {

namespace {

std::unique_ptr<IPhonemizer> makePhonemizer(const QString& name)
{
    if (name == QStringLiteral("Japanese VCV")) {
        return std::make_unique<JapaneseVCVPhonemizer>();
    }
    if (name == QStringLiteral("Japanese CVVC")) {
        return std::make_unique<JapaneseCVVCPhonemizer>();
    }
    if (name == QStringLiteral("Hybrid Japanese") ||
        name == QStringLiteral("Japanese VCV / CVVC Hybrid")) {
        return std::make_unique<HybridJapanesePhonemizer>();
    }
    if (name == QStringLiteral("Korean VCV")) {
        return std::make_unique<KoreanVCVPhonemizer>();
    }
    if (name == QStringLiteral("Korean CBNN")) {
        return std::make_unique<KoreanCBNNPhonemizer>();
    }
    return std::make_unique<DefaultCVPhonemizer>();
}

QString midiToToneName(int midi)
{
    static const QStringList names = {
        QStringLiteral("C"), QStringLiteral("C#"), QStringLiteral("D"),
        QStringLiteral("D#"), QStringLiteral("E"), QStringLiteral("F"),
        QStringLiteral("F#"), QStringLiteral("G"), QStringLiteral("G#"),
        QStringLiteral("A"), QStringLiteral("A#"), QStringLiteral("B")
    };
    midi = std::clamp(midi, 0, 127);
    return names.at(midi % 12) + QString::number(midi / 12 - 1);
}

double tickToMs(const TempoMap& tempo, qint64 tick, double ppq)
{
    return tempo.tickToSeconds(static_cast<double>(tick), ppq) * 1000.0;
}

}

Renderer::Renderer(SingerManager* manager, QObject* parent)
    : QObject(parent), m_singerManager(manager)
{
}

void Renderer::setResampler(const QString& path)
{
    m_resampler = path.trimmed();
}

bool Renderer::renderProject(const Project& project,
                             const QString& output,
                             QString* error)
{
    if (!m_singerManager) {
        if (error) {
            *error = QStringLiteral("SingerManager is unavailable.");
        }
        return false;
    }

    if (m_resampler.isEmpty()) {
        if (error) {
            *error = QStringLiteral(
                "Moresampler executable is not configured. "
                "Set it in Tools > Preferences.");
        }
        return false;
    }

    if (!QFileInfo::exists(m_resampler)) {
        if (error) {
            *error = QStringLiteral("Moresampler executable not found: %1")
                          .arg(m_resampler);
        }
        return false;
    }

    int totalNotes = 0;
    for (const auto& track : project.tracks()) {
        if (!track.muted()) {
            totalNotes += track.notes().size();
        }
    }
    if (totalNotes == 0) {
        if (error) {
            *error = QStringLiteral("Project has no audible notes.");
        }
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Unable to create a render temporary directory.");
        }
        return false;
    }

    std::vector<RenderSegment> segments;
    MoresamplerProcess resampler;
    resampler.setExecutable(m_resampler);

    int processed = 0;
    int segmentIndex = 0;

    for (const auto& track : project.tracks()) {
        if (track.muted()) {
            continue;
        }

        auto singer = m_singerManager->findByPath(track.singerPath());
        if (!singer && !track.singerPath().isEmpty()) {
            singer = std::make_shared<Singer>(
                std::filesystem::path(track.singerPath().toStdWString()));
            singer->load();
        }
        if (!singer) {
            if (error) {
                *error = QStringLiteral("No Singer is assigned to track '%1'.")
                              .arg(track.name());
            }
            return false;
        }
        if (!singer->isValid()) {
            if (error) {
                *error = QStringLiteral("VoiceBank '%1' has an invalid oto.ini: %2")
                              .arg(singer->info().name, singer->oto().error());
            }
            return false;
        }

        std::vector<Note> notes;
        notes.reserve(track.notes().size());
        for (const auto& note : track.notes()) {
            notes.push_back(note);
        }

        auto phonemizer = makePhonemizer(track.phonemizer());
        const auto phonemes = phonemizer->process(notes, *singer);
        AliasResolver resolver(*singer);

        for (const auto& phoneme : phonemes) {
            if (phoneme.noteIndex >= notes.size()) {
                if (error) {
                    *error = QStringLiteral("Phonemizer returned an invalid note index.");
                }
                return false;
            }

            const Note& note = notes.at(phoneme.noteIndex);
            const auto oto = resolver.resolve(phoneme.alias);
            if (!oto) {
                if (error) {
                    *error = QStringLiteral(
                        "Alias '%1' was not found in VoiceBank '%2'.")
                        .arg(phoneme.alias, singer->info().name);
                }
                return false;
            }

            const std::filesystem::path sourcePath =
                singer->path() / std::filesystem::path(oto->filename.toStdWString());
            const QString sourceWav =
                QString::fromStdWString(sourcePath.lexically_normal().wstring());
            if (!QFileInfo::exists(sourceWav)) {
                if (error) {
                    *error = QStringLiteral("oto.ini source WAV not found: %1")
                                  .arg(sourceWav);
                }
                return false;
            }

            const QString outputWav =
                tempDir.path() + QStringLiteral("/segment_%1.wav").arg(segmentIndex++);

            ResamplerRequest request;
            request.inputWav = sourceWav;
            request.outputWav = outputWav;
            request.noteName = midiToToneName(note.getMidiNote());
            request.velocity = std::clamp(note.getVelocity(), 0, 200);
            request.flags = note.getFlags().trimmed();
            if (request.flags.isEmpty()) {
                request.flags = QStringLiteral("g0B0H0P100");
            }
            request.offsetMs = oto->offset;
            request.requiredLengthMs = std::max(
                1.0, tickToMs(project.tempoMap(), note.getDurationTick(), project.ppq()));
            request.consonantMs = oto->consonant;
            request.cutoffMs = oto->cutoff;
            request.volume = std::clamp(track.volume() * 100.0, 0.0, 200.0);
            request.modulation = note.getModulation();
            request.tempo = project.tempoMap().bpm();
            request.pitchBend.clear();

            emit message(QStringLiteral("Rendering %1 / %2: %3")
                             .arg(processed + 1)
                             .arg(totalNotes)
                             .arg(phoneme.alias));

            const ResamplerResult result = resampler.render(request);
            if (!result.success) {
                if (error) {
                    *error = QStringLiteral("Moresampler failed for '%1': %2")
                                  .arg(phoneme.alias, result.error);
                }
                return false;
            }

            RenderSegment segment;
            segment.wavPath = outputWav;
            segment.startMs = qRound64(
                tickToMs(project.tempoMap(), phoneme.startTick, project.ppq()));
            segment.lengthMs = qRound64(
                tickToMs(project.tempoMap(), phoneme.lengthTick, project.ppq()));
            segment.gain = std::clamp(track.volume(), 0.0, 2.0);
            segment.overlapMs = phoneme.overlap;
            segments.push_back(segment);

            ++processed;
            emit progress(std::min(99, processed * 100 / std::max(1, totalNotes)));
        }
    }

    if (segments.empty()) {
        if (error) {
            *error = QStringLiteral("No phonemes were produced by the selected phonemizers.");
        }
        return false;
    }

    WaveAssembler assembler;
    if (!assembler.assemble(segments, output, error)) {
        return false;
    }

    emit progress(100);
    emit message(QStringLiteral("Render complete: %1").arg(output));
    return true;
}

}
