#include "UI/TrackPanel.h"
#include "Singer/SingerManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

namespace myvocal {

namespace {
const QStringList kPhonemizers = {
    QStringLiteral("Default CV"),
    QStringLiteral("Japanese VCV"),
    QStringLiteral("Japanese CVVC"),
    QStringLiteral("Hybrid Japanese"),
    QStringLiteral("Korean VCV"),
    QStringLiteral("Korean CBNN")
};
}

TrackPanel::TrackPanel(Project* project, SingerManager* singers, QWidget* parent)
    : QListWidget(parent)
    , m_project(project)
    , m_singers(singers)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSpacing(2);
    connect(this, &QListWidget::currentRowChanged,
            this, &TrackPanel::trackSelected);
    refresh();
}

void TrackPanel::setSingerManager(SingerManager* singers)
{
    m_singers = singers;
    refresh();
}

void TrackPanel::refresh()
{
    const int oldRow = currentRow();
    clear();

    if (!m_project) {
        return;
    }

    for (int i = 0; i < m_project->tracks().size(); ++i) {
        Track& track = m_project->tracks()[i];
        auto* item = new QListWidgetItem(this);
        item->setSizeHint(QSize(300, 116));

        auto* row = new QWidget(this);
        auto* layout = new QVBoxLayout(row);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto* title = new QLabel(row);
        title->setText(track.name());
        QFont titleFont = title->font();
        titleFont.setBold(true);
        title->setFont(titleFont);
        layout->addWidget(title);

        auto* singer = new QComboBox(row);
        singer->addItem(QStringLiteral("No Singer"), QString());
        if (m_singers) {
            for (const auto& s : m_singers->singers()) {
                QString label = s->info().name;
                if (label.isEmpty()) {
                    label = QString::fromStdWString(s->path().filename().wstring());
                }
                if (!s->isValid()) {
                    label += QStringLiteral(" [oto.ini ERROR]");
                }
                singer->addItem(label,
                                QString::fromStdWString(s->path().wstring()));
            }
        }

        int singerIndex = 0;
        for (int j = 0; j < singer->count(); ++j) {
            if (singer->itemData(j).toString() == track.singerPath()) {
                singerIndex = j;
                break;
            }
        }
        {
            const QSignalBlocker blocker(singer);
            singer->setCurrentIndex(singerIndex);
        }

        auto* phonemizer = new QComboBox(row);
        phonemizer->addItems(kPhonemizers);
        int phonIndex = phonemizer->findText(track.phonemizer());
        if (phonIndex < 0) {
            phonIndex = 0;
        }
        {
            const QSignalBlocker blocker(phonemizer);
            phonemizer->setCurrentIndex(phonIndex);
        }

        auto* controls = new QHBoxLayout();
        controls->setSpacing(6);
        auto* mute = new QCheckBox(QStringLiteral("Mute"), row);
        auto* solo = new QCheckBox(QStringLiteral("Solo"), row);
        mute->setChecked(track.muted());
        solo->setChecked(track.solo());
        controls->addWidget(mute);
        controls->addWidget(solo);
        controls->addStretch();
        layout->addWidget(singer);
        layout->addWidget(phonemizer);
        layout->addLayout(controls);

        auto* level = new QHBoxLayout();
        auto* volumeLabel = new QLabel(QStringLiteral("Vol"), row);
        auto* volume = new QSlider(Qt::Horizontal, row);
        volume->setRange(0, 200);
        volume->setValue(qRound(track.volume() * 100.0));
        volume->setToolTip(QStringLiteral("Volume: %1%").arg(volume->value()));
        level->addWidget(volumeLabel);
        level->addWidget(volume, 1);
        auto* panLabel = new QLabel(QStringLiteral("Pan"), row);
        auto* pan = new QSlider(Qt::Horizontal, row);
        pan->setRange(-100, 100);
        pan->setValue(qRound(track.pan() * 100.0));
        level->addWidget(panLabel);
        level->addWidget(pan, 1);
        layout->addLayout(level);

        setItemWidget(item, row);

        connect(singer, &QComboBox::currentIndexChanged,
                this, [this, i, singer](int index) {
                    if (!m_project || i >= m_project->tracks().size()) {
                        return;
                    }
                    m_project->tracks()[i].setSingerPath(
                        singer->itemData(index).toString());
                    emit trackSettingsChanged(i);
                });
        connect(phonemizer, &QComboBox::currentIndexChanged,
                this, [this, i, phonemizer](int index) {
                    if (!m_project || i >= m_project->tracks().size()) {
                        return;
                    }
                    m_project->tracks()[i].setPhonemizer(
                        phonemizer->itemText(index));
                    emit trackSettingsChanged(i);
                });
        connect(mute, &QCheckBox::toggled, this, [this, i](bool value) {
            if (!m_project || i >= m_project->tracks().size()) {
                return;
            }
            m_project->tracks()[i].setMuted(value);
            emit trackSettingsChanged(i);
        });
        connect(solo, &QCheckBox::toggled, this, [this, i](bool value) {
            if (!m_project || i >= m_project->tracks().size()) {
                return;
            }
            m_project->tracks()[i].setSolo(value);
            emit trackSettingsChanged(i);
        });
        connect(volume, &QSlider::valueChanged, this, [this, i, volume](int value) {
            if (!m_project || i >= m_project->tracks().size()) {
                return;
            }
            m_project->tracks()[i].setVolume(value / 100.0);
            volume->setToolTip(QStringLiteral("Volume: %1%").arg(value));
            emit trackSettingsChanged(i);
        });
        connect(pan, &QSlider::valueChanged, this, [this, i](int value) {
            if (!m_project || i >= m_project->tracks().size()) {
                return;
            }
            m_project->tracks()[i].setPan(value / 100.0);
            emit trackSettingsChanged(i);
        });
    }

    if (!isEmpty()) {
        setCurrentRow(qBound(0, oldRow < 0 ? 0 : oldRow, count() - 1));
    }
}

}
