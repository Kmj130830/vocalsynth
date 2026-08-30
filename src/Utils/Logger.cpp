#include "Utils/Logger.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>
namespace myvocal { static QMutex g_mutex; static QFile*file(){static QFile f(QDir::currentPath()+"/logs/app.log");return &f;}void Logger::initialize(){QDir().mkpath(QDir::currentPath()+"/logs");file()->open(QIODevice::Append|QIODevice::Text);info("Logger initialized");}static void write(const QString&l,const QString&m){QMutexLocker lock(&g_mutex);if(!file()->isOpen())return;QTextStream s(file());s<<QDateTime::currentDateTime().toString(Qt::ISODate)<<" ["<<l<<"] "<<m<<'\n';s.flush();}void Logger::debug(const QString&m){write("DEBUG",m);}void Logger::info(const QString&m){write("INFO",m);}void Logger::warning(const QString&m){write("WARNING",m);}void Logger::error(const QString&m){write("ERROR",m);} }
