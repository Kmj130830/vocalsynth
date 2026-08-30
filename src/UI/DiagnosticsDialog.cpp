#include "UI/DiagnosticsDialog.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
namespace myvocal { DiagnosticsDialog::DiagnosticsDialog(const QString&t,QWidget*p):QDialog(p){setWindowTitle("Voicebank Diagnostics");resize(650,450);auto*l=new QVBoxLayout(this);auto*e=new QPlainTextEdit(this);e->setReadOnly(true);e->setPlainText(t);l->addWidget(e);}}
