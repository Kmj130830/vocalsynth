#include "UI/PhonemeStripEditor.h"

#include "Phonemizer/DefaultCVPhonemizer.h"
#include "Phonemizer/HybridJapanesePhonemizer.h"
#include "Phonemizer/JapaneseCVVCPhonemizer.h"
#include "Phonemizer/JapaneseVCVPhonemizer.h"
#include "Phonemizer/KoreanCBNNPhonemizer.h"
#include "Phonemizer/KoreanVCVPhonemizer.h"
#include "Singer/Singer.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>
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
}

PhonemeStripEditor::PhonemeStripEditor(Project* project, QWidget* parent)
    : QAbstractScrollArea(parent), m_project(project)
{
    setMinimumHeight(105);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    updateScrollRanges();
}

void PhonemeStripEditor::setProject(Project* project)
{
    m_project = project;
    m_activeTrack = 0;
    m_playheadMs = 0;
    m_draggingPlayhead = false;
    updateScrollRanges();
    viewport()->update();
}

void PhonemeStripEditor::setActiveTrack(int index)
{
    const int count = m_project ? static_cast<int>(m_project->tracks().size()) : 0;
    m_activeTrack = count > 0 ? std::clamp(index, 0, count - 1) : 0;
    updateScrollRanges();
    viewport()->update();
}

void PhonemeStripEditor::setPlayheadMs(qint64 ms)
{
    m_playheadMs = std::max<qint64>(0, ms);
    const int x = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond);
    const int left = horizontalScrollBar()->value();
    const int width = viewport()->width();
    if (x < left) horizontalScrollBar()->setValue(std::max(0, x - width / 4));
    else if (x > left + width * 3 / 4) horizontalScrollBar()->setValue(std::max(0, x - width / 2));
    viewport()->update();
}

void PhonemeStripEditor::setPixelsPerSecond(double pixels)
{
    m_pixelsPerSecond = std::clamp(pixels, 20.0, 500.0);
    updateScrollRanges();
    viewport()->update();
}

qint64 PhonemeStripEditor::msAtX(int x) const
{
    const double sceneX = x + horizontalScrollBar()->value();
    return std::max<qint64>(0, qRound64(sceneX / m_pixelsPerSecond * 1000.0));
}

void PhonemeStripEditor::updateScrollRanges()
{
    qint64 maxMs = 30000;
    if (m_project && m_activeTrack >= 0 && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
        const auto& track = m_project->tracks()[m_activeTrack];
        for (const auto& note : track.notes()) {
            maxMs = std::max(maxMs, qRound64(m_project->tempoMap().tickToSeconds(note.getEndTick(), m_project->ppq()) * 1000.0));
        }
    }
    horizontalScrollBar()->setRange(0, std::max(0, qRound(maxMs / 1000.0 * m_pixelsPerSecond) + 500 - viewport()->width()));
}

void PhonemeStripEditor::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(viewport()->rect(), QColor("#0f1114"));

    const int sx = horizontalScrollBar()->value();
    const qint64 beatMs = m_project ? qMax<qint64>(1, qRound64(60000.0 / std::max(20.0, m_project->tempoMap().bpm()))) : 500;
    const qint64 firstMs = std::max<qint64>(0, qRound64(sx / m_pixelsPerSecond * 1000.0) - beatMs);
    const qint64 endMs = msAtX(viewport()->width());
    for (qint64 ms = (firstMs / beatMs) * beatMs; ms <= endMs + beatMs; ms += beatMs) {
        const int x = qRound(ms / 1000.0 * m_pixelsPerSecond) - sx;
        p.setPen(QColor(ms % (beatMs * 4) == 0 ? "#454c55" : "#252a30"));
        p.drawLine(x, 0, x, viewport()->height());
    }

    if (m_project && m_activeTrack >= 0 && m_activeTrack < static_cast<int>(m_project->tracks().size())) {
        const auto& track = m_project->tracks()[m_activeTrack];
        Singer singer(std::filesystem::path(track.singerPath().toStdWString()));
        if (singer.load()) {
            auto phonemizer = makePhonemizer(track.phonemizer());
            if (phonemizer) {
                const std::vector<Note> notes(track.notes().cbegin(), track.notes().cend());
                const auto phonemes = phonemizer->process(notes, singer);
                for (const auto& ph : phonemes) {
                    const double startMs = m_project->tempoMap().tickToSeconds(ph.startTick, m_project->ppq()) * 1000.0;
                    const double end = m_project->tempoMap().tickToSeconds(ph.startTick + ph.lengthTick, m_project->ppq()) * 1000.0;
                    const int x = qRound(startMs / 1000.0 * m_pixelsPerSecond) - sx;
                    const int w = std::max(8, qRound((end - startMs) / 1000.0 * m_pixelsPerSecond));
                    const QRect rect(x, 20, w, 48);
                    if (!rect.intersects(viewport()->rect())) continue;
                    p.setBrush(QColor("#273a4d"));
                    p.setPen(QColor("#6387aa"));
                    p.drawRoundedRect(rect, 3, 3);
                    p.setPen(QColor("#e8edf2"));
                    p.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, ph.alias.isEmpty() ? QStringLiteral("?") : ph.alias);
                }
            }
        } else {
            p.setPen(QColor("#777f89"));
            p.drawText(8, 18, QStringLiteral("Singer not loaded"));
        }
    }

    p.setPen(QPen(QColor("#ff5b6e"), 2));
    const int playheadX = qRound(m_playheadMs / 1000.0 * m_pixelsPerSecond) - sx;
    p.drawLine(playheadX, 0, playheadX, viewport()->height());
}

void PhonemeStripEditor::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollRanges();
}

void PhonemeStripEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    m_draggingPlayhead = true;
    m_playheadMs = msAtX(qRound(event->position().x()));
    emit positionClicked(m_playheadMs);
    viewport()->update();
}

void PhonemeStripEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_draggingPlayhead) return;
    m_playheadMs = msAtX(qRound(event->position().x()));
    emit positionClicked(m_playheadMs);
    viewport()->update();
}

void PhonemeStripEditor::mouseReleaseEvent(QMouseEvent*)
{
    m_draggingPlayhead = false;
}

}