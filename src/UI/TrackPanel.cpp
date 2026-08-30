#include "UI/TrackPanel.h"
namespace myvocal { TrackPanel::TrackPanel(Project*p,QWidget*par):QListWidget(par),m_project(p){connect(this,&QListWidget::currentRowChanged,this,&TrackPanel::trackSelected);refresh();}void TrackPanel::refresh(){clear();for(const auto&t:m_project->tracks())addItem(QStringLiteral("%1%2").arg(t.muted()?QStringLiteral("[M] "):QString()).arg(t.name()));setCurrentRow(0);} }
