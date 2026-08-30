#include "Renderer/WaveAssembler.h"

#include <QFile>
#include <QDataStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace myvocal {

namespace {

quint16 readU16(const QByteArray& data, int offset)
{
    return static_cast<quint16>(
        static_cast<unsigned char>(data.at(offset)) |
        (static_cast<unsigned char>(data.at(offset + 1)) << 8));
}

quint32 readU32(const QByteArray& data, int offset)
{
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset)) |
                                (static_cast<unsigned char>(data.at(offset + 1)) << 8) |
                                (static_cast<unsigned char>(data.at(offset + 2)) << 16) |
                                (static_cast<unsigned char>(data.at(offset + 3)) << 24));
}

bool readPcm16(const QString& path, std::vector<float>& samples, int& sampleRate,
               QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open WAV: %1").arg(path);
        }
        return false;
    }

    const QByteArray bytes = file.readAll();
    if (bytes.size() < 12 || bytes.mid(0, 4) != "RIFF" || bytes.mid(8, 4) != "WAVE") {
        if (error) {
            *error = QStringLiteral("Invalid RIFF/WAVE file: %1").arg(path);
        }
        return false;
    }

    int channels = 0;
    int bitsPerSample = 0;
    int dataOffset = -1;
    int dataSize = 0;

    int offset = 12;
    while (offset + 8 <= bytes.size()) {
        const QByteArray chunkId = bytes.mid(offset, 4);
        const quint32 chunkSize = readU32(bytes, offset + 4);
        const int chunkData = offset + 8;
        if (chunkData > bytes.size()) {
            break;
        }

        if (chunkId == "fmt " && chunkSize >= 16 && chunkData + 16 <= bytes.size()) {
            const quint16 format = readU16(bytes, chunkData);
            channels = readU16(bytes, chunkData + 2);
            sampleRate = static_cast<int>(readU32(bytes, chunkData + 4));
            bitsPerSample = readU16(bytes, chunkData + 14);
            if (format != 1) {
                if (error) {
                    *error = QStringLiteral("Only PCM WAV input is supported: %1").arg(path);
                }
                return false;
            }
        } else if (chunkId == "data") {
            dataOffset = chunkData;
            dataSize = static_cast<int>(std::min<quint32>(
                chunkSize, static_cast<quint32>(bytes.size() - chunkData)));
            break;
        }

        offset = chunkData + static_cast<int>(chunkSize) + (chunkSize & 1u);
    }

    if (channels <= 0 || bitsPerSample != 16 || sampleRate <= 0 ||
        dataOffset < 0 || dataSize < 0) {
        if (error) {
            *error = QStringLiteral("Unsupported PCM WAV format: %1").arg(path);
        }
        return false;
    }

    const int frameBytes = channels * 2;
    const int frames = dataSize / frameBytes;
    samples.resize(frames);
    for (int frame = 0; frame < frames; ++frame) {
        float sum = 0.0f;
        for (int channel = 0; channel < channels; ++channel) {
            const int p = dataOffset + frame * frameBytes + channel * 2;
            const qint16 value = static_cast<qint16>(readU16(bytes, p));
            sum += static_cast<float>(value) / 32768.0f;
        }
        samples[frame] = sum / static_cast<float>(channels);
    }
    return true;
}

std::vector<float> resampleLinear(const std::vector<float>& input,
                                  int sourceRate, int targetRate)
{
    if (sourceRate == targetRate || input.empty()) {
        return input;
    }

    const size_t outputSize = static_cast<size_t>(
        std::ceil(input.size() * static_cast<double>(targetRate) / sourceRate));
    std::vector<float> output(outputSize);
    for (size_t i = 0; i < output.size(); ++i) {
        const double source = i * static_cast<double>(sourceRate) / targetRate;
        const size_t a = std::min(input.size() - 1, static_cast<size_t>(source));
        const size_t b = std::min(input.size() - 1, a + 1);
        const double frac = source - static_cast<double>(a);
        output[i] = static_cast<float>(input[a] * (1.0 - frac) + input[b] * frac);
    }
    return output;
}

bool writeWav(const QString& path, const std::vector<float>& samples, int rate)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    const quint32 dataBytes = static_cast<quint32>(samples.size() * sizeof(qint16));
    stream.writeRawData("RIFF", 4);
    stream << static_cast<quint32>(36 + dataBytes);
    stream.writeRawData("WAVEfmt ", 8);
    stream << static_cast<quint32>(16);
    stream << static_cast<quint16>(1);
    stream << static_cast<quint16>(1);
    stream << static_cast<quint32>(rate);
    stream << static_cast<quint32>(rate * 2);
    stream << static_cast<quint16>(2);
    stream << static_cast<quint16>(16);
    stream.writeRawData("data", 4);
    stream << dataBytes;

    for (float value : samples) {
        const auto sample = static_cast<qint16>(
            std::clamp(value, -1.0f, 1.0f) * 32767.0f);
        stream << sample;
    }
    return true;
}

}

bool WaveAssembler::assemble(const std::vector<RenderSegment>& segments,
                              const QString& output,
                              QString* error)
{
    if (segments.empty()) {
        if (error) {
            *error = QStringLiteral("No render segments.");
        }
        return false;
    }

    constexpr int targetRate = 48000;
    auto ordered = segments;
    std::sort(ordered.begin(), ordered.end(),
              [](const RenderSegment& a, const RenderSegment& b) {
                  return a.startMs < b.startMs;
              });

    std::vector<float> mix(targetRate * 10, 0.0f);
    size_t maxEnd = 0;

    for (const auto& segment : ordered) {
        std::vector<float> samples;
        int sourceRate = 0;
        if (!readPcm16(segment.wavPath, samples, sourceRate, error)) {
            return false;
        }
        samples = resampleLinear(samples, sourceRate, targetRate);

        const size_t start = static_cast<size_t>(
            std::max<qint64>(0, segment.startMs) * targetRate / 1000);
        if (mix.size() < start + samples.size()) {
            mix.resize(start + samples.size(), 0.0f);
        }

        const float gain = static_cast<float>(segment.gain);
        for (size_t i = 0; i < samples.size(); ++i) {
            mix[start + i] += samples[i] * gain;
        }
        maxEnd = std::max(maxEnd, start + samples.size());
    }

    mix.resize(maxEnd);
    if (!writeWav(output, mix, targetRate)) {
        if (error) {
            *error = QStringLiteral("Cannot write output WAV: %1").arg(output);
        }
        return false;
    }
    return true;
}

}
