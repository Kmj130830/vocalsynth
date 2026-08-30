#include "UI/PreferencesDialog.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFileDialog>
namespace myvocal { PreferencesDialog::PreferencesDialog(QString*r,QWidget*p):QDialog(p),m_resampler(r){setWindowTitle("Preferences");auto*l=new QFormLayout(this);auto*e=new QLineEdit(*r,this);l->addRow("Moresampler",e);auto*b=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);l->addRow(b);connect(b,&QDialogButtonBox::accepted,this,[this,e]{*m_resampler=e->text();accept();});connect(b,&QDialogButtonBox::rejected,this,&QDialog::reject);}}
