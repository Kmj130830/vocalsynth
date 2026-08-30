#pragma once
#include <QString>
namespace myvocal { class Logger { public: static void initialize(); static void debug(const QString&); static void info(const QString&); static void warning(const QString&); static void error(const QString&); }; }
