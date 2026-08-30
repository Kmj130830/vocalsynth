#include "UI/LyricEditor.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
namespace myvocal { LyricEditor::LyricEditor(QWidget*p):QDialog(p){setWindowTitle("Lyric");auto*l=new QVBoxLayout(this);m_edit=new QLineEdit(this);l->addWidget(m_edit);auto*b=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);l->addWidget(b);connect(b,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(b,&QDialogButtonBox::rejected,this,&QDialog::reject);}QString LyricEditor::lyric()const{return m_edit->text();}void LyricEditor::setLyric(const QString&s){m_edit->setText(s);m_edit->selectAll();}}
