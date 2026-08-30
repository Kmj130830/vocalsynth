#include "Utils/ShortcutManager.h"
namespace myvocal { ShortcutManager::ShortcutManager(QObject*p):QObject(p){} QAction*ShortcutManager::create(const QString&n,const QKeySequence&s,const std::function<void()>&fn,QWidget*p){auto*a=new QAction(n,p);a->setShortcut(s);connect(a,&QAction::triggered,this,[fn]{fn();});return a;} }
