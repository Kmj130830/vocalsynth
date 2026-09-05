#include "Renderer/WaveAssembler.h"

#include <QDataStream>
#include <QFile>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace myvocal {
namespace {

quint16 readU16(const QByteArray& data, qsizetype offset)
{
    return static_cast<quint16>(static_cast<quint8>(data.at(offset)) |
                                (static_cast<quint16>(static_cast<quint8>(data.at(offset + 1))) << 8));
}

quint32 readU32(const QByteArray& data, qsizetype offset)
{
    return static_cast<quint32>(static_cast<quint8>(data.at(offset)) |
                                (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 8) |
                                (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 16) |
                                (static_cast<quint32>(static_cast<quint8>(data.at(offset + 3))) << 24));
}

double readSample(const QByteArray& bytes, qsizetype p, int bits, int format)
{
    if (format == 3 && bits == 32) {
        float value = 0.0f;
        std::memcpy(&value, bytes.constData() + p, sizeof(float));
        return std::clamp(static_cast<double>(value), -1.0, 1.0);
    }
    if (format != 1) return 0.0;
    switch (bits) {
    case 8:
        return (static_cast<int>(static_cast<quint8>(bytes.at(p))) - 128) / 128.0;
    case 16:
        return static_cast<qint16>(readU16(bytes, p)) / 32768.0;
    case 24: {
        const quint32 raw = static_cast<quint32>(static_cast<quint8>(bytes.at(p))) |
                            (static_cast<quint32>(static_cast<quint8>(bytes.at(p + 1))) << 8) |
                            (static_cast<quint32>(static_cast<quint8>(bytes.at(p + 2))) << 16);
        const qint32 value = (raw & 0x00800000u) ? static_cast<qint32>(raw | 0xff000000u)
                                                 : static_cast<qint32>(raw);
        return value / 8388608.0;
    }
    case 32:
        return static_cast<qint32>(readU32(bytes, p)) / 2147483648.0;
    default:
        return 0.0;
    }
}

bool readWaveMono(const QString& path, std::vector<float>& samples, int& sampleRate, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open WAV: %1").arg(path);
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 12 || bytes.mid(0, 4) != "RIFF" || bytes.mid(8, 4) != "WAVE") {
        if (error) *error = QStringLiteral("Invalid RIFF/WAVE file: %1").arg(path);
        return false;
    }

    int format = 0;
    int channels = 0;
    int bits = 0;
    sampleRate = 0;
    qsizetype dataOffset = -1;
    qsizetype dataSize = 0;
    qsizetype pos = 12;

    while (pos + 8 <= bytes.size()) {
        const quint32 chunkSize = readU32(bytes, pos + 4);
        const qsizetype chunkData = pos + 8;
        if (chunkData > bytes.size()) break;
        const qsizetype safeSize = std::min<qsizetype>(chunkSize, bytes.size() - chunkData);

        if (bytes.mid(pos, 4) == "fmt " && safeSize >= 16) {
            format = static_cast<int>(readU16(bytes, chunkData));
            channels = static_cast<int>(readU16(bytes, chunkData + 2));
            sampleRate = static_cast<int>(readU32(bytes, chunkData + 4));
            bits = static_cast<int>(readU16(bytes, chunkData + 14));
            if (format == 0xfffe && safeSize >= 40) {
                const quint32 subFormat = readU32(bytes, chunkData + 24);
                if (subFormat == 1) format = 1;
                else if (subFormat == 3) format = 3;
            }
        } else if (bytes.mid(pos, 4) == "data") {
            dataOffset = chunkData;
            dataSize = safeSize;
            break;
        }
        pos = chunkData + safeSize + (chunkSize & 1u);
    }

    if ((format != 1 && format != 3) || channels <= 0 || channels > 32 || sampleRate <= 0 ||
        (bits != 8 && bits != 16 && bits != 24 && bits != 32) || dataOffset < 0 || dataSize <= 0 ||
        (format == 3 && bits != 32)) {
        if (error) *error = QStringLiteral("Unsupported WAV format: %1").arg(path);
        return false;
    }

    const int bytesPerSample = bits / 8;
    const int frameBytes = channels * bytesPerSample;
    if (frameBytes <= 0) return false;
    const qsizetype frameCount = dataSize / frameBytes;
    samples.resize(static_cast<size_t>(frameCount));
    for (qsizetype frame = 0; frame < frameCount; ++frame) {
        double sum = 0.0;
        for (int channel = 0; channel < channels; ++channel) {
            const qsizetype p = dataOffset + frame * frameBytes + channel * bytesPerSample;
            sum += readSample(bytes, p, bits, format);
        }
        samples[static_cast<size_t>(frame)] = static_cast<float>(sum / channels);
    }
    return true;
}

std::vector<float> resampleLinear(const std::vector<float>& input, int sourceRate, int targetRate)
{
    if (sourceRate <= 0 || targetRate <= 0 || sourceRate == targetRate || input.empty()) return input;
    const size_t outputSize = static_cast<size_t>(std::ceil(input.size() * static_cast<double>(targetRate) / sourceRate));
    std::vector<float> output(outputSize);
    for (size_t i = 0; i < output.size(); ++i) {
        const double source = i * static_cast<double>(sourceRate) / targetRate;
        const size_t a = std::min(input.size() - 1, static_cast<size_t>(source));
        const size_t b = std::min(input.size() - 1, a + 1);
        const double frac = std::clamp(source - static_cast<double>(a), 0.0, 1.0);
        output[i] = static_cast<float>(input[a] * (1.0 - frac) + input[b] * frac);
    }
    return output;
}

bool writeWav(const QString& path, const std::vector<float>& samples, int rate)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
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
    for (const float value : samples) {
        const qint16 sample = static_cast<qint16>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
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
        if (error) *error = QStringLiteral("No render segments.");
        return false;
    }

    constexpr int targetRate = 48000;
    auto ordered = segments;
    std::sort(ordered.begin(), ordered.end(), [](const RenderSegment& a, const RenderSegment& b) {
        return a.startMs < b.startMs;
    });

    std::vector<float> mix(targetRate, 0.0f);
    size_t maxEnd = 0;

    for (const auto& segment : ordered) {
        std::vector<float> samples;
        int sourceRate = 0;
        if (!readWaveMono(segment.wavPath, samples, sourceRate, error)) return false;
        samples = resampleLinear(samples, sourceRate, targetRate);

        const size_t start = static_cast<size_t>(std::max<qint64>(0, segment.startMs) * targetRate / 1000);
        size_t usable = samples.size();
        if (segment.lengthMs > 0) {
            const size_t requested = static_cast<size_t>(std::max<qint64>(1, segment.lengthMs) * targetRate / 1000);
            usable = std::min(usable, requested);
        }
        if (usable == 0) continue;
        if (mix.size() < start + usable) mix.resize(start + usable, 0.0f);

        const float gain = static_cast<float>(std::clamp(segment.gain, 0.0, 2.0));
        for (size_t i = 0; i < usable; ++i) mix[start + i] += samples[i] * gain;
        maxEnd = std::max(maxEnd, start + usable);
    }

    mix.resize(maxEnd);
    if (mix.empty()) {
        if (error) *error = QStringLiteral("Rendered segments contained no audio samples.");
        return false;
    }

    float peak = 0.0f;
    for (const float sample : mix) peak = std::max(peak, std::abs(sample));
    if (peak > 0.98f) {
        const float gain = 0.98f / peak;
        for (float& sample : mix) sample *= gain;
    }

    if (!writeWav(output, mix, targetRate)) {
        if (error) *error = QStringLiteral("Cannot write output WAV: %1").arg(output);
        return false;
    }
    return true;
}

}