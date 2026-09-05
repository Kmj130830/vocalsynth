#include "UI/MainToolBar.h"
#include "Editor/PianoRollEditor.h"

#include <QActionGroup>
#include <QComboBox>
#include <QLabel>
#include <QTimer>

namespace myvocal {

MainToolBar::MainToolBar(QWidget* parent)
    : QToolBar(parent)
{
    setIconSize(QSize(18, 18));
    setMovable(false);

    m_group = new QActionGroup(this);
    m_group->setExclusive(true);

    const QList<QPair<QString, EditTool>> tools = {
        {QStringLiteral("Select"), EditTool::Select},
        {QStringLiteral("Pen"), EditTool::Pen},
        {QStringLiteral("Pen+"), EditTool::PenPlus},
        {QStringLiteral("Eraser"), EditTool::Eraser},
        {QStringLiteral("Knife"), EditTool::Knife},
        {QStringLiteral("Pitch"), EditTool::Pitch},
        {QStringLiteral("Pitch Line"), EditTool::PitchLine},
        {QStringLiteral("Vibrato"), EditTool::Vibrato},
        {QStringLiteral("Zoom"), EditTool::Zoom},
        {QStringLiteral("Pan"), EditTool::Pan}
    };

    for (const auto& item : tools) {
        QAction* action = addAction(item.first);
        action->setCheckable(true);
        action->setToolTip(item.first);
        action->setData(static_cast<int>(item.second));
        m_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, action] {
            emit toolChanged(static_cast<EditTool>(action->data().toInt()));
        });
        if (item.second == EditTool::Pen) {
            action->setChecked(true);
        }
    }

    addSeparator();

    QAction* snap = addAction(QStringLiteral("Snap"));
    snap->setCheckable(true);
    snap->setChecked(true);
    snap->setToolTip(QStringLiteral("Snap notes and edits to the selected grid"));
    connect(snap, &QAction::toggled, this, &MainToolBar::snapToggled);

    QAction* grid = addAction(QStringLiteral("Grid"));
    grid->setCheckable(true);
    grid->setChecked(true);
    grid->setToolTip(QStringLiteral("Show or hide piano-roll grid"));
    connect(grid, &QAction::toggled, this, &MainToolBar::gridToggled);

    addWidget(new QLabel(QStringLiteral("Grid:"), this));
    m_gridCombo = new QComboBox(this);
    m_gridCombo->addItem(QStringLiteral("1/4"), 4);
    m_gridCombo->addItem(QStringLiteral("1/8"), 8);
    m_gridCombo->addItem(QStringLiteral("1/16"), 16);
    m_gridCombo->addItem(QStringLiteral("1/32"), 32);
    m_gridCombo->addItem(QStringLiteral("1/64"), 64);
    m_gridCombo->setCurrentIndex(2);
    m_gridCombo->setMinimumWidth(72);
    m_gridCombo->setToolTip(QStringLiteral("Grid resolution"));
    addWidget(m_gridCombo);

    connect(m_gridCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit gridResolutionChanged(m_gridCombo->itemData(index).toInt());
    });

    // MainWindow recreates PianoRollEditor when New/Open loads a project.
    // Keep a dynamic connection so these controls always affect the editor
    // which is currently part of the main window.
    QTimer::singleShot(0, this, [this] {
        auto findEditor = [this]() -> PianoRollEditor* {
            QWidget* host = window();
            return host ? host->findChild<PianoRollEditor*>() : nullptr;
        };

        connect(this, &MainToolBar::snapToggled, this,
                [findEditor](bool enabled) {
            if (auto* editor = findEditor()) {
                editor->setSnapEnabled(enabled);
                editor->viewport()->update();
            }
        });

        connect(this, &MainToolBar::gridToggled, this,
                [findEditor](bool enabled) {
            if (auto* editor = findEditor()) {
                editor->setShowGrid(enabled);
                editor->viewport()->update();
            }
        });

        connect(this, &MainToolBar::gridResolutionChanged, this,
                [findEditor](int denominator) {
            if (auto* editor = findEditor()) {
                if (const auto* project = editor->project()) {
                    editor->setGridTicks(
                        qMax<qint64>(1, qRound64(
                            project->ppq() * 4.0 / denominator)));
                }
                editor->viewport()->update();
            }
        });
    });
}

}