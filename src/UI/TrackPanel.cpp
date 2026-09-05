#include "UI/TrackPanel.h"
#include "Singer/SingerManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QMenu>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace myvocal {
namespace {
const QStringList kPhonemizers = {
    QStringLiteral("Default CV"), QStringLiteral("Japanese VCV"), QStringLiteral("Japanese CVVC"),
    QStringLiteral("Hybrid Japanese"), QStringLiteral("Korean VCV"), QStringLiteral("Korean CBNN")
};
}

TrackPanel::TrackPanel(Project* project, SingerManager* singers, QWidget* parent)
    : QListWidget(parent), m_project(project), m_singers(singers)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSpacing(2);
    setViewportMargins(0, 42, 0, 0);

    m_addButton = new QPushButton(QStringLiteral("+  Add Track"), this);
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setFixedHeight(34);
    m_addButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#262c34; border:1px solid #3d4651; border-radius:6px; color:#e9eef5; font-weight:600; }"
        "QPushButton:hover { background:#303844; } QPushButton:pressed { background:#1e232a; }"));
    connect(m_addButton, &QPushButton::clicked, this, &TrackPanel::addTrackRequested);
    connect(this, &QListWidget::currentRowChanged, this, &TrackPanel::trackSelected);
    refresh();
}

void TrackPanel::setSingerManager(SingerManager* singers) { m_singers = singers; refresh(); }

void TrackPanel::resizeEvent(QResizeEvent* event)
{
    QListWidget::resizeEvent(event);
    if (m_addButton) m_addButton->setGeometry(8, 5, width() - 16, 34);
}

void TrackPanel::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = indexAt(event->pos()).row();
    if (index < 0) return;
    setCurrentRow(index);
    QMenu menu(this);
    QAction* remove = menu.addAction(QStringLiteral("Delete Track"));
    remove->setEnabled(m_project && m_project->tracks().size() > 1);
    if (menu.exec(event->globalPos()) == remove && remove->isEnabled()) emit removeTrackRequested(index);
}

void TrackPanel::refresh()
{
    const int oldRow = currentRow();
    clear();
    if (!m_project) return;

    for (int i = 0; i < m_project->tracks().size(); ++i) {
        Track& track = m_project->tracks()[i];
        if (track.singerPath().isEmpty() && m_singers) {
            for (const auto& candidate : m_singers->singers()) {
                if (candidate->isValid()) {
                    track.setSingerPath(QString::fromStdWString(candidate->path().wstring()));
                    break;
                }
            }
        }

        auto* item = new QListWidgetItem(this);
        item->setSizeHint(QSize(300, 116));
        auto* row = new QWidget(this);
        auto* layout = new QVBoxLayout(row);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto* title = new QLabel(row);
        title->setText(track.name());
        QFont titleFont = title->font(); titleFont.setBold(true); title->setFont(titleFont);
        layout->addWidget(title);

        auto* singer = new QComboBox(row);
        singer->addItem(QStringLiteral("No Singer"), QString());
        if (m_singers) {
            for (const auto& info : m_singers->singers()) {
                QString label = info->info().name;
                if (label.isEmpty()) label = QString::fromStdWString(info->path().filename().wstring());
                if (!info->isValid()) label += QStringLiteral(" [oto.ini ERROR]");
                singer->addItem(label, QString::fromStdWString(info->path().wstring()));
            }
        }
        int singerIndex = singer->findData(track.singerPath());
        if (singerIndex < 0) singerIndex = 0;
        { const QSignalBlocker blocker(singer); singer->setCurrentIndex(singerIndex); }

        auto* phonemizer = new QComboBox(row);
        phonemizer->addItems(kPhonemizers);
        int phonIndex = phonemizer->findText(track.phonemizer());
        if (phonIndex < 0) phonIndex = 0;
        { const QSignalBlocker blocker(phonemizer); phonemizer->setCurrentIndex(phonIndex); }

        auto* controls = new QHBoxLayout;
        auto* mute = new QCheckBox(QStringLiteral("Mute"), row);
        auto* solo = new QCheckBox(QStringLiteral("Solo"), row);
        mute->setChecked(track.muted()); solo->setChecked(track.solo());
        controls->addWidget(mute); controls->addWidget(solo); controls->addStretch();
        layout->addWidget(singer); layout->addWidget(phonemizer); layout->addLayout(controls);

        auto* level = new QHBoxLayout;
        auto* volumeLabel = new QLabel(QStringLiteral("Vol"), row);
        auto* volume = new QSlider(Qt::Horizontal, row); volume->setRange(0, 200); volume->setValue(qRound(track.volume() * 100.0));
        level->addWidget(volumeLabel); level->addWidget(volume, 1);
        auto* panLabel = new QLabel(QStringLiteral("Pan"), row);
        auto* pan = new QSlider(Qt::Horizontal, row); pan->setRange(-100, 100); pan->setValue(qRound(track.pan() * 100.0));
        level->addWidget(panLabel); level->addWidget(pan, 1); layout->addLayout(level);

        setItemWidget(item, row);

        connect(singer, &QComboBox::currentIndexChanged, this, [this, i, singer](int index) {
            if (!m_project || i < 0 || i >= m_project->tracks().size()) return;
            m_project->tracks()[i].setSingerPath(singer->itemData(index).toString()); emit trackSettingsChanged(i);
        });
        connect(phonemizer, &QComboBox::currentIndexChanged, this, [this, i, phonemizer](int index) {
            if (!m_project || i < 0 || i >= m_project->tracks().size()) return;
            m_project->tracks()[i].setPhonemizer(phonemizer->itemText(index)); emit trackSettingsChanged(i);
        });
        connect(mute, &QCheckBox::toggled, this, [this, i](bool value) {
            if (!m_project || i < 0 || i >= m_project->tracks().size()) return;
            m_project->tracks()[i].setMuted(value); emit trackSettingsChanged(i);
        });
        connect(solo, &QCheckBox::toggled, this, [this, i](bool value) {
            if (!m_project || i < 0 || i >= m_project->tracks().size()) return;
            m_project->tracks()[i].setSolo(value); emit trackSettingsChanged(i);
        });
        connect(volume, &QSlider::valueChanged, this, [this, i](int value) {
            if (!m_project || i < 0 || i >= m_project->tracks().size()) return;
            m_project->tracks()[i].setVolume(value / 100.0); emit trackSettingsChanged(i);
        });
        connect(pan, &QSlider::valueChanged, this, [this, i](int value) {
            if (!m_project || i < 0 || i >= m_project->tracks().size()) return;
            m_project->tracks()[i].setPan(value / 100.0); emit trackSettingsChanged(i);
        });
    }

    if (count() > 0) setCurrentRow(qBound(0, oldRow < 0 ? 0 : oldRow, count() - 1));
    if (m_addButton) m_addButton->raise();
}

}