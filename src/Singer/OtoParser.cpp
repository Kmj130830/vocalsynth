#include "Singer/OtoParser.h"
#include <QFile>
#include <QTextStream>
namespace myvocal {
static bool parseDouble(const QString&s,double&v){bool ok=false;v=s.trimmed().toDouble(&ok);return ok;}
bool OtoParser::parseLine(const QString&line,OtoEntry&entry)const{const int eq=line.indexOf('=');if(eq<=0)return false;entry.filename=line.left(eq).trimmed();const auto p=line.mid(eq+1).split(',');if(p.size()!=6)return false;entry.alias=p[0].trimmed();double vals[5]{};for(int i=0;i<5;++i)if(!parseDouble(p[i+1],vals[i]))return false;entry.offset=vals[0];entry.consonant=vals[1];entry.cutoff=vals[2];entry.preutterance=vals[3];entry.overlap=vals[4];entry.valid=!entry.filename.isEmpty()&&!entry.alias.isEmpty();return entry.valid;}
bool OtoParser::load(const std::filesystem::path&path){m_entries.clear();m_valid=false;m_error.clear();QFile f(QString::fromStdString(path.string()));if(!f.open(QIODevice::ReadOnly|QIODevice::Text)){m_error=f.errorString();return false;}QTextStream ts(&f);ts.setEncoding(QStringConverter::Utf8);int lineNo=0;while(!ts.atEnd()){++lineNo;const QString line=ts.readLine();if(line.trimmed().isEmpty()||line.trimmed().startsWith('#'))continue;OtoEntry e;if(parseLine(line,e))m_entries.push_back(std::move(e));else m_error=QStringLiteral("Invalid oto.ini line %1").arg(lineNo);}m_valid=!m_entries.empty();if(!m_valid&&m_error.isEmpty())m_error=QStringLiteral("No valid oto entries");return m_valid;}
std::optional<OtoEntry> OtoParser::findAlias(const QString&alias)const{for(const auto&e:m_entries)if(e.valid&&e.alias==alias)return e;return std::nullopt;} const std::vector<OtoEntry>&OtoParser::getEntries()const noexcept{return m_entries;} bool OtoParser::isValid()const noexcept{return m_valid;} QString OtoParser::error()const{return m_error;}
}
