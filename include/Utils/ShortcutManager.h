#pragma once
#include <QObject>
#include <QAction>
#include <QKeySequence>
#include <QWidget>
#include <functional>
namespace myvocal { class ShortcutManager : public QObject { Q_OBJECT public: explicit ShortcutManager(QObject*p=nullptr); QAction* create(const QString&name,const QKeySequence&seq,const std::function<void()>&fn,QWidget*parent); }; }
