#pragma once
#include <QObject>
#include "Core/Project.h"
namespace myvocal { class Renderer; class RenderWorker : public QObject { Q_OBJECT public: RenderWorker(Renderer*renderer,Project*project,const QString&output,QObject*parent=nullptr); public slots: void run(); signals: void finished(bool,const QString&); private: Renderer*m_renderer; Project*m_project; QString m_output; }; }
