#pragma once
#include <QDialog>
#include <QString>
namespace myvocal { class DiagnosticsDialog : public QDialog { Q_OBJECT public: explicit DiagnosticsDialog(const QString&text,QWidget*parent=nullptr); }; }
