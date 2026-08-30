#include "Singer/CharacterParser.h"
#include <QFile>
#include <QTextStream>
namespace myvocal { bool CharacterParser::load(const std::filesystem::path&p){m_values.clear();QFile f(QString::fromStdString(p.string()));if(!f.open(QIODevice::ReadOnly|QIODevice::Text))return false;QTextStream ts(&f);ts.setEncoding(QStringConverter::Utf8);while(!ts.atEnd()){const auto line=ts.readLine();const int eq=line.indexOf('=');if(eq<=0)continue;m_values[line.left(eq).trimmed().toLower()]=line.mid(eq+1).trimmed();}return true;} QString CharacterParser::value(const QString&k)const{return m_values.value(k.toLower());} const QMap<QString,QString>&CharacterParser::values()const noexcept{return m_values;} }
