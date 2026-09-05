#include "UI/AudioSpectrumWidget.h"
#include "Core/Project.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QFileInfo>
#include <QPainter>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace myvocal {
namespace {
constexpr int kBands = 24;
constexpr int kFftSamples = 256;
constexpr int kHopSamples = 1024;
constexpr double kPi = 3.14159265358979323846;
}

AudioSpectrumWidget::AudioSpectrumWidget(Project* project, QWidget* parent)
    : QWidget(parent), m_project(project), m_decoder(new QAudioDecoder(this))
{
    setMinimumHeight(74);
    setMaximumHeight(130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    m_decoder->setAudioFormat(format);
    connect(m_decoder, &QAudioDecoder::bufferReady, this, &AudioSpectrumWidget::readBuffer);
    connect(m_decoder, &QAudioDecoder::finished, this, &AudioSpectrumWidget::decoderFinished);
    connect(m_decoder, &QAudioDecoder::errorChanged, this, [this](QAudioDecoder::Error) { decoderError(); });
    refresh();
}

void AudioSpectrumWidget::setProject(Project* project) { m_project = project; refresh(); }
void AudioSpectrumWidget::setPlayheadMs(qint64 ms) { m_playheadMs = std::max<qint64>(0, ms); update(); }

void AudioSpectrumWidget::resetData()
{
    m_columns.clear();
    m_sourcePath.clear();
    m_decodedFrames = 0;
    update();
}

void AudioSpectrumWidget::refresh()
{
    QString source;
    if (m_project) {
        for (const auto& clip : m_project->audioClips()) {
            if (!clip.muted && QFileInfo(clip.path).isFile()) { source = clip.path; break; }
        }
    }
    if (source.isEmpty()) {
        if (m_decoder) m_decoder->stop();
        resetData();
        return;
    }
    if (source == m_sourcePath && !m_columns.isEmpty()) { update(); return; }
    decodeFile(source);
}

void AudioSpectrumWidget::decodeFile(const QString& path)
{
    m_decoder->stop();
    m_columns.clear();
    m_decodedFrames = 0;
    m_sourcePath = path;
    m_decoder->setSource(QUrl::fromLocalFile(path));
    m_decoder->start();
}

void AudioSpectrumWidget::readBuffer()
{
    const QAudioBuffer buffer = m_decoder->read();
    if (!buffer.isValid()) return;
    const QAudioFormat format = buffer.format();
    const int frames = buffer.sampleCount();
    if (frames <= 0 || format.channelCount() <= 0) return;

    QVector<float> mono;
    mono.reserve(frames);
    if (format.sampleFormat() == QAudioFormat::Int16) {
        const qint16* data = buffer.constData<qint16>();
        for (int frame = 0; frame < frames; ++frame) {
            qint64 sum = 0;
            for (int channel = 0; channel < format.channelCount(); ++channel) sum += data[frame * format.channelCount() + channel];
            mono.push_back(static_cast<float>(sum) / static_cast<float>(format.channelCount() * 32768.0));
        }
    } else if (format.sampleFormat() == QAudioFormat::Float) {
        const float* data = buffer.constData<float>();
        for (int frame = 0; frame < frames; ++frame) {
            double sum = 0.0;
            for (int channel = 0; channel < format.channelCount(); ++channel) sum += data[frame * format.channelCount() + channel];
            mono.push_back(static_cast<float>(sum / format.channelCount()));
        }
    } else {
        return;
    }

    for (int offset = 0; offset + kFftSamples <= mono.size(); offset += kHopSamples) {
        QVector<float> bands(kBands, 0.0f);
        for (int band = 0; band < kBands; ++band) {
            const double frequencyBin = 1.0 + band * 3.0;
            const double omega = 2.0 * kPi * frequencyBin / kFftSamples;
            double real = 0.0;
            double imag = 0.0;
            for (int n = 0; n < kFftSamples; ++n) {
                const double window = 0.5 - 0.5 * std::cos(2.0 * kPi * n / (kFftSamples - 1));
                const double sample = mono[offset + n] * window;
                real += sample * std::cos(omega * n);
                imag -= sample * std::sin(omega * n);
            }
            bands[band] = static_cast<float>(std::clamp(std::sqrt(real * real + imag * imag) / kFftSamples * 7.0, 0.0, 1.0));
        }
        m_columns.push_back(std::move(bands));
        m_decodedFrames += kHopSamples;
        if (m_columns.size() > 16000) break;
    }
    update();
}

void AudioSpectrumWidget::decoderFinished() { update(); }
void AudioSpectrumWidget::decoderError() { update(); }

void AudioSpectrumWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#0c0f13"));
    p.setPen(QColor("#343a44"));
    p.drawLine(0, 0, width(), 0);
    if (m_columns.isEmpty()) {
        p.setPen(QColor("#717986"));
        p.drawText(rect().adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Audio Spectrum  •  no decoded audio"));
        return;
    }
    const int columnWidth = std::max(1, width() / std::max(1, m_columns.size()));
    const int usableHeight = height() - 8;
    for (int i = 0; i < m_columns.size(); ++i) {
        const int x = i * columnWidth;
        const auto& bands = m_columns.at(i);
        for (int b = 0; b < bands.size(); ++b) {
            const float level = bands.at(b);
            const int barHeight = std::max(1, static_cast<int>(level * usableHeight * 0.72));
            p.fillRect(x, height() - 5 - barHeight, std::max(1, columnWidth - 1), barHeight, QColor("#485a70"));
        }
    }
    if (m_project) {
        qint64 durationMs = 0;
        for (const auto& clip : m_project->audioClips()) if (clip.path == m_sourcePath) durationMs = std::max(durationMs, clip.durationMs);
        if (durationMs > 0) {
            const int x = qRound(std::clamp(static_cast<double>(m_playheadMs) / durationMs, 0.0, 1.0) * width());
            p.setPen(QPen(QColor("#ff5b6e"), 1.5));
            p.drawLine(x, 0, x, height());
        }
    }
}

}