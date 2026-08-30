#include "UI/MainToolBar.h"

#include <QActionGroup>
#include <QComboBox>
#include <QLabel>

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
    connect(snap, &QAction::toggled, this, &MainToolBar::snapToggled);

    QAction* grid = addAction(QStringLiteral("Grid"));
    grid->setCheckable(true);
    grid->setChecked(true);
    connect(grid, &QAction::toggled, this, &MainToolBar::gridToggled);

    addWidget(new QLabel(QStringLiteral("Grid:"), this));
    m_gridCombo = new QComboBox(this);
    m_gridCombo->addItem(QStringLiteral("1/4"), 4);
    m_gridCombo->addItem(QStringLiteral("1/8"), 8);
    m_gridCombo->addItem(QStringLiteral("1/16"), 16);
    m_gridCombo->addItem(QStringLiteral("1/32"), 32);
    m_gridCombo->addItem(QStringLiteral("1/64"), 64);
    m_gridCombo->setCurrentIndex(2);
    m_gridCombo->setToolTip(QStringLiteral("Grid resolution"));
    addWidget(m_gridCombo);

    connect(m_gridCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        emit gridResolutionChanged(m_gridCombo->itemData(index).toInt());
    });
}

}