#include "UI/ArrangementEditor.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace myvocal {

namespace {
constexpr int kPeakFrames = 256;

float decodedSample(const QAudioBuffer& buffer, int frame, int channel)
{
    const QAudioFormat format = buffer.format();
    const int channels = std::max(1, format.channelCount());
    const int index = frame * channels + channel;
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8: {
        const auto* data = buffer.constData<quint8>();
        return data ? (static_cast<int>(data[index]) - 128) / 128.0f : 0.0f;
    }
    case QAudioFormat::Int16: {
        const auto* data = buffer.constData<qint16>();
        return data ? std::clamp(static_cast<float>(data[index]) / 32768.0f, -1.0f, 1.0f) : 0.0f;
    }
    case QAudioFormat::Int32: {
        const auto* data = buffer.constData<qint32>();
        return data ? std::clamp(static_cast<float>(data[index] / 2147483648.0), -1.0f, 1.0f) : 0.0f;
    }
    case QAudioFormat::Float: {
        const auto* data = buffer.constData<float>();
        return data ? std::clamp(data[index], -1.0f, 1.0f) : 0.0f;
    }
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        return 0.0f;
    }
    return 0.0f;
}
}

ArrangementEditor::ArrangementEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setMinimumHeight(190);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setViewportMargins(0, 38, 0, 0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("ArrangementHeader"));
    header->setStyleSheet(QStringLiteral(
        "QWidget{background:#181b20;border-bottom:1px solid #343a42;}"
        "QLabel{color:#c7cdd5;}"
        "QDoubleSpinBox{background:#242931;color:#edf2f7;border:1px solid #414954;padding:2px 5px;min-width:72px;}"));
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(10, 4, 10, 4);
    layout->setSpacing(8);
    layout->addWidget(new QLabel(QStringLiteral("Arrangement"), header));
    layout->addSpacing(18);
    layout->addWidget(new QLabel(QStringLiteral("BPM"), header));
    m_bpmSpin = new QDoubleSpinBox(header);
    m_bpmSpin->setRange(20.0, 999.0);
    m_bpmSpin->setDecimals(2);
    m_bpmSpin->setSingleStep(1.0);
    m_bpmSpin->setValue(m_project ? m_project->tempoMap().bpm() : 120.0);
    layout->addWidget(m_bpmSpin);
    layout->addStretch(1);

    connect(m_bpmSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double bpm) {
        if (!m_project) return;
        m_project->tempoMap().setBpm(bpm);
        updateScrollRanges();
        viewport()->update();
        emit documentChanged();
    });

    m_audioDecoder = new QAudioDecoder(this);
    connect(m_audioDecoder, &QAudioDecoder::bufferReady, this, &ArrangementEditor::readAudioBuffer);
    connect(m_audioDecoder, &QAudioDecoder::finished, this, &ArrangementEditor::finishAudioDecode);
    connect(m_audioDecoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this, [this](QAudioDecoder::Error) {
        finishAudioDecode();
    });

    updateHeaderGeometry();
    updateScrollRanges();
    refreshAudioWaveforms();
}

void ArrangementEditor::setProject(Project* project)
{
    m_project = project;
    m_draggingPlayhead = false;
    m_draggingAudioIndex = -1;
    if (m_bpmSpin) {
        const QSignalBlocker blocker(m_bpmSpin);
        m_bpmSpin->setValue(m_project ? m_project->tempoMap().bpm() : 120.0);
    }
    refreshAudioWaveforms();
    updateScrollRanges();
    viewport()->update();
}

void ArrangementEditor::setPlayheadMs(qint64 ms)
{
    m_playheadMs = std::max<qint64>(0, ms);
    const int x = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond);
    const int left = horizontalScrollBar()->value();
    const int view = viewport()->width();
    if (x < left) horizontalScrollBar()->setValue(std::max(0, x - view / 5));
    else if (x > left + view * 4 / 5) horizontalScrollBar()->setValue(std::max(0, x - view * 2 / 5));
    viewport()->update();
}

qint64 ArrangementEditor::playheadMs() const noexcept { return m_playheadMs; }
void ArrangementEditor::setTrackHeight(int pixels) { m_trackHeight = std::clamp(pixels, 44, 100); updateScrollRanges(); viewport()->update(); }
void ArrangementEditor::setPixelsPerSecond(double pixels) { m_pixelsPerSecond = std::clamp(pixels, 20.0, 500.0); updateScrollRanges(); viewport()->update(); }
qint64 ArrangementEditor::msAtX(int x) const { return std::max<qint64>(0, qRound64((x + horizontalScrollBar()->value()) / m_pixelsPerSecond * 1000.0)); }
int ArrangementEditor::trackAtY(int y) const
{
    const int index = (y + verticalScrollBar()->value()) / m_trackHeight;
    const int count = m_project ? static_cast<int>(m_project->tracks().size()) : 0;
    return index >= 0 && index < count ? index : -1;
}

void ArrangementEditor::updateScrollRanges()
{
    const int trackCount = m_project ? std::max(1, static_cast<int>(m_project->tracks().size()) + 1) : 1;
    verticalScrollBar()->setRange(0, std::max(0, trackCount * m_trackHeight - viewport()->height()));
    qint64 maxMs = 30000;
    if (m_project) {
        for (const auto& track : m_project->tracks()) {
            for (const auto& note : track.notes()) {
                maxMs = std::max(maxMs, qRound64(m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0));
            }
        }
        for (const auto& clip : m_project->audioClips()) {
            maxMs = std::max(maxMs, clip.startMs + std::max<qint64>(0, clip.durationMs));
        }
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 600 - viewport()->width()));
}

void ArrangementEditor::updateHeaderGeometry()
{
    if (auto* header = findChild<QWidget*>(QStringLiteral("ArrangementHeader"))) header->setGeometry(0, 0, width(), 38);
}

void ArrangementEditor::refreshAudioWaveforms()
{
    if (!m_audioDecoder) return;
    m_audioDecoder->stop();
    m_decodeIndex = -1;
    m_decodeFramesInBucket = 0;
    m_decodeMin = 1.0f;
    m_decodeMax = -1.0f;
    m_audioPeaks.clear();
    m_waveformPaths.clear();

    if (!m_project) {
        viewport()->update();
        return;
    }

    m_audioPeaks.resize(m_project->audioClips().size());
    m_waveformPaths.reserve(m_project->audioClips().size());
    for (const auto& clip : m_project->audioClips()) m_waveformPaths.push_back(clip.path);
    decodeNextAudioClip();
}

void ArrangementEditor::decodeNextAudioClip()
{
    if (!m_project || !m_audioDecoder) return;
    ++m_decodeIndex;
    if (m_decodeIndex >= m_project->audioClips().size()) {
        viewport()->update();
        return;
    }
    const auto& clip = m_project->audioClips().at(m_decodeIndex);
    if (clip.path.isEmpty() || !QFileInfo(clip.path).isFile()) {
        QTimer::singleShot(0, this, &ArrangementEditor::decodeNextAudioClip);
        return;
    }

    m_decodeFramesInBucket = 0;
    m_decodeMin = 1.0f;
    m_decodeMax = -1.0f;
    m_audioDecoder->setSource(QUrl::fromLocalFile(clip.path));
    m_audioDecoder->start();
}

void ArrangementEditor::readAudioBuffer()
{
    if (!m_audioDecoder || m_decodeIndex < 0 || m_decodeIndex >= m_audioPeaks.size()) return;
    while (m_audioDecoder->bufferAvailable()) {
        const QAudioBuffer buffer = m_audioDecoder->read();
        const QAudioFormat format = buffer.format();
        const int channels = std::max(1, format.channelCount());
        const int frames = buffer.frameCount();
        if (frames <= 0 || channels <= 0) continue;

        for (int frame = 0; frame < frames; ++frame) {
            float frameMin = 1.0f;
            float frameMax = -1.0f;
            for (int channel = 0; channel < channels; ++channel) {
                const float value = decodedSample(buffer, frame, channel);
                frameMin = std::min(frameMin, value);
                frameMax = std::max(frameMax, value);
            }
            m_decodeMin = std::min(m_decodeMin, frameMin);
            m_decodeMax = std::max(m_decodeMax, frameMax);
            ++m_decodeFramesInBucket;
            if (m_decodeFramesInBucket >= kPeakFrames) {
                m_audioPeaks[m_decodeIndex].push_back(qMakePair(m_decodeMin, m_decodeMax));
                m_decodeFramesInBucket = 0;
                m_decodeMin = 1.0f;
                m_decodeMax = -1.0f;
            }
        }
    }
    viewport()->update();
}

void ArrangementEditor::finishAudioDecode()
{
    if (m_decodeIndex >= 0 && m_decodeIndex < m_audioPeaks.size() && m_decodeFramesInBucket > 0) {
        m_audioPeaks[m_decodeIndex].push_back(qMakePair(m_decodeMin, m_decodeMax));
    }
    m_decodeFramesInBucket = 0;
    m_decodeMin = 1.0f;
    m_decodeMax = -1.0f;
    QTimer::singleShot(0, this, &ArrangementEditor::decodeNextAudioClip);
}

void ArrangementEditor::paintEvent(QPaintEvent*)
{
    if (m_project && (m_waveformPaths.size() != m_project->audioClips().size())) refreshAudioWaveforms();

    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(viewport()->rect(), QColor("#101215"));
    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();
    const double bpm = m_project ? std::max(20.0, m_project->tempoMap().bpm()) : 120.0;
    const qint64 beatMs = std::max<qint64>(1, qRound64(60000.0 / bpm));
    const qint64 barMs = beatMs * 4;
    const qint64 firstVisibleMs = std::max<qint64>(0, qRound64(sx / m_pixelsPerSecond * 1000.0) - beatMs * 2);
    const qint64 endVisibleMs = msAtX(viewport()->width());

    for (qint64 ms = (firstVisibleMs / beatMs) * beatMs; ms <= endVisibleMs + beatMs; ms += beatMs) {
        const double x = ms / 1000.0 * m_pixelsPerSecond - sx;
        if (x < 0 || x > viewport()->width()) continue;
        const bool bar = ms % barMs == 0;
        p.setPen(QPen(bar ? QColor("#555d68") : QColor("#272c33"), bar ? 1.5 : 1.0));
        p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
    }
    if (!m_project) return;

    const int trackCount = static_cast<int>(m_project->tracks().size());
    for (int ti = 0; ti < trackCount + 1; ++ti) {
        const int y = ti * m_trackHeight - sy;
        if (y + m_trackHeight < 0 || y > viewport()->height()) continue;
        const bool audioLane = ti == trackCount;
        p.fillRect(0, y, viewport()->width(), m_trackHeight, ti % 2 ? QColor("#14171b") : QColor("#181b20"));
        p.setPen(QColor("#303640"));
        p.drawLine(0, y + m_trackHeight - 1, viewport()->width(), y + m_trackHeight - 1);

        if (!audioLane) {
            const auto& track = m_project->tracks()[ti];
            const auto& notes = track.notes();
            int i = 0;
            while (i < notes.size()) {
                int j = i + 1;
                qint64 partStart = notes[i].getStartTick();
                qint64 partEnd = notes[i].getEndTick();
                while (j < notes.size() && notes[j].getStartTick() <= partEnd + m_project->ppq()) {
                    partEnd = std::max(partEnd, notes[j].getEndTick());
                    ++j;
                }

                const double startMs = m_project->tempoMap().tickToSeconds(partStart, m_project->ppq()) * 1000.0;
                const double endMs = m_project->tempoMap().tickToSeconds(partEnd, m_project->ppq()) * 1000.0;
                const int x = qRound(startMs / 1000.0 * m_pixelsPerSecond) - sx;
                const int w = std::max(10, qRound((endMs - startMs) / 1000.0 * m_pixelsPerSecond));
                const QRect partRect(x, y + 5, w, m_trackHeight - 10);
                if (partRect.intersects(viewport()->rect())) {
                    p.setBrush(track.muted() ? QColor("#3a3d39") : QColor("#587f2e"));
                    p.setPen(QPen(track.solo() ? QColor("#f1c75b") : QColor("#729f3b"), 1));
                    p.drawRoundedRect(partRect, 4, 4);

                    p.setPen(QColor("#edf4e8"));
                    p.drawText(partRect.adjusted(7, 3, -7, -partRect.height() / 2), Qt::AlignLeft | Qt::AlignTop,
                               track.name().isEmpty() ? QStringLiteral("Untitled Track") : track.name());
                    p.setPen(QColor("#d0d9cc"));
                    p.drawText(partRect.adjusted(7, partRect.height() / 2 - 1, -7, -3), Qt::AlignLeft | Qt::AlignBottom,
                               track.singerPath().isEmpty() ? QStringLiteral("No singer") : QFileInfo(track.singerPath()).baseName());

                    int minMidi = 127;
                    int maxMidi = 0;
                    for (int n = i; n < j; ++n) {
                        minMidi = std::min(minMidi, notes[n].getMidiNote());
                        maxMidi = std::max(maxMidi, notes[n].getMidiNote());
                    }
                    int span = std::max(12, maxMidi - minMidi);
                    const int centerMin = (minMidi + maxMidi) / 2 - span / 2;
                    minMidi = std::max(0, centerMin);
                    maxMidi = std::min(127, minMidi + span);
                    const double usableTop = partRect.top() + 12.0;
                    const double usableHeight = std::max(8, partRect.height() - 18);
                    p.setPen(QPen(QColor("#f4f7ee"), 2.0, Qt::SolidLine, Qt::RoundCap));
                    for (int n = i; n < j; ++n) {
                        const auto& note = notes[n];
                        const double noteStartMs = m_project->tempoMap().tickToSeconds(note.getStartTick(), m_project->ppq()) * 1000.0;
                        const double noteEndMs = m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0;
                        const double nx1 = noteStartMs / 1000.0 * m_pixelsPerSecond - sx;
                        const double nx2 = noteEndMs / 1000.0 * m_pixelsPerSecond - sx;
                        const double norm = static_cast<double>(maxMidi - note.getMidiNote()) / std::max(1, maxMidi - minMidi);
                        const double ny = usableTop + norm * usableHeight;
                        p.drawLine(QPointF(nx1, ny), QPointF(std::max(nx1 + 1.0, nx2), ny));
                    }
                }
                i = j;
            }
        } else {
            for (int ci = 0; ci < m_project->audioClips().size(); ++ci) {
                const auto& clip = m_project->audioClips().at(ci);
                if (clip.muted || !QFileInfo(clip.path).isFile()) continue;
                const int x = qRound(clip.startMs / 1000.0 * m_pixelsPerSecond) - sx;
                const int w = std::max(12, qRound(clip.durationMs / 1000.0 * m_pixelsPerSecond));
                const QRect r(x, y + 5, w, m_trackHeight - 10);
                if (!r.intersects(viewport()->rect())) continue;

                p.setBrush(ci == m_draggingAudioIndex ? QColor("#6e343f") : QColor("#5c2f38"));
                p.setPen(QPen(ci == m_draggingAudioIndex ? QColor("#d96b7b") : QColor("#a64b5b"), 1));
                p.drawRoundedRect(r, 4, 4);

                const auto& peaks = ci < m_audioPeaks.size() ? m_audioPeaks.at(ci) : QVector<QPair<float, float>>{};
                if (!peaks.isEmpty() && r.width() > 2) {
                    const double center = r.center().y();
                    const double amp = std::max(3.0, r.height() * 0.43);
                    p.setPen(QPen(QColor("#e6a0a8"), 1));
                    for (int px = 0; px < r.width(); ++px) {
                        const int first = static_cast<int>((static_cast<double>(px) / r.width()) * peaks.size());
                        const int last = std::max(first + 1, static_cast<int>((static_cast<double>(px + 1) / r.width()) * peaks.size()));
                        float minValue = 1.0f;
                        float maxValue = -1.0f;
                        for (int k = first; k < std::min(last, peaks.size()); ++k) {
                            minValue = std::min(minValue, peaks[k].first);
                            maxValue = std::max(maxValue, peaks[k].second);
                        }
                        const double y1 = center - maxValue * amp;
                        const double y2 = center - minValue * amp;
                        p.drawLine(QPointF(r.left() + px, y1), QPointF(r.left() + px, y2));
                    }
                }
                p.setPen(QColor("#f1dfe2"));
                p.drawText(r.adjusted(7, 2, -7, -2), Qt::AlignLeft | Qt::AlignTop, QFileInfo(clip.path).fileName());
            }
        }
    }

    const int px = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond) - sx;
    p.setPen(QPen(QColor("#ff5b6e"), 2));
    p.drawLine(px, 0, px, viewport()->height());
}

void ArrangementEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateHeaderGeometry();
    updateScrollRanges();
}

void ArrangementEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_project) return;
    setFocus();
    const QPoint pos = event->pos();
    const int trackCount = static_cast<int>(m_project->tracks().size());
    const int audioLaneY = trackCount * m_trackHeight - verticalScrollBar()->value();

    if (pos.y() >= audioLaneY && pos.y() < audioLaneY + m_trackHeight) {
        const qint64 clickMs = msAtX(pos.x());
        for (int i = m_project->audioClips().size() - 1; i >= 0; --i) {
            const auto& clip = m_project->audioClips().at(i);
            if (clickMs >= clip.startMs && clickMs <= clip.startMs + std::max<qint64>(1, clip.durationMs)) {
                m_draggingAudioIndex = i;
                m_audioDragOffsetMs = clickMs - clip.startMs;
                viewport()->update();
                return;
            }
        }
    }

    if (const int track = trackAtY(pos.y()); track >= 0) emit trackClicked(track);
    m_playheadMs = msAtX(pos.x());
    emit positionClicked(m_playheadMs);
    m_draggingPlayhead = true;
    viewport()->update();
}

void ArrangementEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_project) return;
    const int x = qRound(event->position().x());
    if (m_draggingAudioIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        const qint64 newStart = std::max<qint64>(0, msAtX(x) - m_audioDragOffsetMs);
        auto& clip = m_project->audioClips()[m_draggingAudioIndex];
        if (clip.startMs != newStart) {
            clip.startMs = newStart;
            emit documentChanged();
            viewport()->update();
        }
        return;
    }
    if (!m_draggingPlayhead) return;
    m_playheadMs = msAtX(x);
    emit positionClicked(m_playheadMs);
    viewport()->update();
}

void ArrangementEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    if (m_draggingAudioIndex >= 0) {
        m_draggingAudioIndex = -1;
        emit documentChanged();
        viewport()->update();
    }
    m_draggingPlayhead = false;
}

}