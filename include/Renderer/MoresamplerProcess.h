#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>
namespace myvocal { struct ResamplerResult { bool success{false}; int exitCode{-1}; QString error; QString outputPath; }; class MoresamplerProcess : public QObject { Q_OBJECT public: explicit MoresamplerProcess(QObject*parent=nullptr); void setExecutable(const QString&); ResamplerResult render(const QString&input,const QString&output,int noteNumber,double velocity,double pitchRatio,int timeoutMs=30000); void cancel(); private: QString m_executable; QProcess m_process; };
}
