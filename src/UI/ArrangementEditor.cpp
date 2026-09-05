#include "UI/ArrangementEditor.h"

#include "Phonemizer/DefaultCVPhonemizer.h"
#include "Phonemizer/HybridJapanesePhonemizer.h"
#include "Phonemizer/JapaneseCVVCPhonemizer.h"
#include "Phonemizer/JapaneseVCVPhonemizer.h"
#include "Phonemizer/KoreanCBNNPhonemizer.h"
#include "Phonemizer/KoreanVCVPhonemizer.h"
#include "Singer/Singer.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>
#include <memory>

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

}

ArrangementEditor::ArrangementEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setMinimumHeight(170);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    updateScrollRanges();
}

void ArrangementEditor::setProject(Project* project)
{
    m_project = project;
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
    else if (x > left + view * 4 / 5) horizontalScrollBar()->setValue(std::max(0, x - view / 5));
    viewport()->update();
}

qint64 ArrangementEditor::playheadMs() const noexcept { return m_playheadMs; }

void ArrangementEditor::setTrackHeight(int pixels)
{
    m_trackHeight = std::clamp(pixels, 50, 110);
    updateScrollRanges();
    viewport()->update();
}

void ArrangementEditor::setPixelsPerSecond(double pixels)
{
    m_pixelsPerSecond = std::clamp(pixels, 20.0, 500.0);
    updateScrollRanges();
    viewport()->update();
}

qint64 ArrangementEditor::msAtX(int x) const
{
    const double sceneX = x + horizontalScrollBar()->value();
    return std::max<qint64>(0, qRound64(sceneX / m_pixelsPerSecond * 1000.0));
}

int ArrangementEditor::yForTrack(int trackIndex) const
{
    return trackIndex * m_trackHeight - verticalScrollBar()->value();
}

void ArrangementEditor::updateScrollRanges()
{
    const int trackCount = m_project ? std::max(1, static_cast<int>(m_project->tracks().size())) : 1;
    verticalScrollBar()->setRange(0, std::max(0, trackCount * m_trackHeight - viewport()->height()));

    qint64 maxMs = 60000;
    if (m_project) {
        for (const auto& track : m_project->tracks()) {
            for (const auto& note : track.notes()) {
                maxMs = std::max(maxMs, qRound64(m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0));
            }
        }
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 600 - viewport()->width()));
}

void ArrangementEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.fillRect(viewport()->rect(), QColor("#101215"));
    const int sx = horizontalScrollBar()->value();
    const int sy = verticalScrollBar()->value();

    const qint64 beatMs = m_project ? qRound64(60000.0 / std::max(1.0, m_project->tempoMap().bpm())) : 500;
    const qint64 barMs = beatMs * 4;
    const qint64 endMs = msAtX(viewport()->width());
    qint64 firstMs = std::max<qint64>(0, qRound64(sx / m_pixelsPerSecond * 1000.0) - 2 * beatMs);
    firstMs = (firstMs / std::max<qint64>(1, beatMs)) * beatMs;
    for (qint64 ms = firstMs; ms <= endMs + beatMs; ms += beatMs) {
        const double x = ms / 1000.0 * m_pixelsPerSecond - sx;
        if (x < 0 || x > viewport()->width()) continue;
        const bool bar = barMs > 0 && ms % barMs == 0;
        p.setPen(QPen(bar ? QColor("#555b64") : QColor("#292e34"), bar ? 1.5 : 1.0));
        p.drawLine(QPointF(x, 0), QPointF(x, viewport()->height()));
    }

    if (!m_project) return;
    const int trackCount = static_cast<int>(m_project->tracks().size());
    for (int ti = 0; ti < trackCount; ++ti) {
        const int y = yForTrack(ti);
        p.fillRect(0, y, viewport()->width(), m_trackHeight, ti % 2 ? QColor("#14171a") : QColor("#181b1f"));
        p.setPen(QColor("#30353b"));
        p.drawLine(0, y + m_trackHeight - 1, viewport()->width(), y + m_trackHeight - 1);

        const auto& track = m_project->tracks()[ti];
        Singer singer(std::filesystem::path(track.singerPath().toStdWString()));
        const bool singerOk = !track.singerPath().isEmpty() && singer.load();
        std::vector<Phoneme> phonemes;
        if (singerOk) {
            auto phonemizer = makePhonemizer(track.phonemizer());
            if (phonemizer) phonemes = phonemizer->process(std::vector<Note>(track.notes().cbegin(), track.notes().cend()), singer);
        }

        p.setPen(QColor("#aeb5bf"));
        p.drawText(7, y + 16, QStringLiteral("Track %1").arg(ti + 1));

        if (phonemes.empty()) {
            p.setPen(QColor("#68717c"));
            p.drawText(70, y + 16, singerOk ? QStringLiteral("No phonemes") : QStringLiteral("Singer not loaded"));
            continue;
        }

        for (const auto& ph : phonemes) {
            const double startMs = m_project->tempoMap().tickToSeconds(ph.startTick, m_project->ppq()) * 1000.0;
            const double endMs = m_project->tempoMap().tickToSeconds(ph.startTick + ph.lengthTick, m_project->ppq()) * 1000.0;
            const int x = qRound(startMs / 1000.0 * m_pixelsPerSecond) - sx;
            const int w = std::max(12, qRound((endMs - startMs) / 1000.0 * m_pixelsPerSecond));
            const QRect r(x, y + 25, w, m_trackHeight - 31);
            if (!r.intersects(viewport()->rect())) continue;
            p.setBrush(QColor("#334b63"));
            p.setPen(QColor("#6587a8"));
            p.drawRoundedRect(r, 3, 3);
            p.setPen(Qt::white);
            p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignCenter, ph.alias.isEmpty() ? QStringLiteral("?") : ph.alias);
        }
    }

    const int ph = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond) - sx;
    p.setPen(QPen(QColor("#ff5b6e"), 2));
    p.drawLine(ph, 0, ph, viewport()->height());
}

void ArrangementEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void ArrangementEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    setFocus();
    m_playheadMs = msAtX(qRound(event->position().x()));
    emit positionClicked(m_playheadMs);
    m_draggingPlayhead = true;
    viewport()->update();
}

void ArrangementEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_draggingPlayhead) return;
    m_playheadMs = msAtX(qRound(event->position().x()));
    emit positionClicked(m_playheadMs);
    viewport()->update();
}

void ArrangementEditor::mouseReleaseEvent(QMouseEvent*) { m_draggingPlayhead = false; }

}
