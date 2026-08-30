#pragma once
#include <QObject>
#include <QString>
#include <vector>
#include "Core/Project.h"
#include "Core/Phoneme.h"
namespace myvocal { class SingerManager; class Renderer : public QObject { Q_OBJECT public: explicit Renderer(SingerManager*manager,QObject*parent=nullptr); void setResampler(const QString&); bool renderProject(const Project&,const QString&,QString*error); signals: void progress(int); void message(const QString&); private: SingerManager* m_singerManager; QString m_resampler; }; }
