#include "Singer/OtoParser.h"

#include <QFile>
#include <QTextStream>

namespace myvocal {
namespace {

bool parseDouble(const QString& text, double& value)
{
    bool ok = false;
    value = text.trimmed().toDouble(&ok);
    return ok;
}

}

bool OtoParser::parseLine(const QString& line, OtoEntry& entry) const
{
    const int equal = line.indexOf('=');
    if (equal <= 0) return false;

    entry.filename = line.left(equal).trimmed();
    const QStringList values = line.mid(equal + 1).split(',');
    if (values.size() != 6) return false;

    entry.alias = values.at(0).trimmed();
    double numeric[5]{};
    for (int i = 0; i < 5; ++i) {
        if (!parseDouble(values.at(i + 1), numeric[i])) return false;
    }

    entry.offset = numeric[0];
    entry.consonant = numeric[1];
    entry.cutoff = numeric[2];
    entry.preutterance = numeric[3];
    entry.overlap = numeric[4];
    entry.valid = !entry.filename.isEmpty() && !entry.alias.isEmpty();
    return entry.valid;
}

bool OtoParser::load(const std::filesystem::path& path)
{
    m_entries.clear();
    m_valid = false;
    m_error.clear();

    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    bool skippedInvalid = false;
    int lineNumber = 0;

    while (!stream.atEnd()) {
        ++lineNumber;
        QString line = stream.readLine();
        if (line.startsWith(QChar(0xFEFF))) line.remove(0, 1);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;

        OtoEntry entry;
        if (parseLine(line, entry)) {
            m_entries.push_back(std::move(entry));
        } else {
            skippedInvalid = true;
        }
    }

    m_valid = !m_entries.empty();
    if (!m_valid) {
        m_error = QStringLiteral("No valid oto.ini entries");
    } else if (skippedInvalid) {
        m_error = QStringLiteral("Some invalid oto.ini lines were ignored");
    }
    return m_valid;
}

std::optional<OtoEntry> OtoParser::findAlias(const QString& alias) const
{
    const QString key = alias.trimmed();
    for (const auto& entry : m_entries) {
        if (entry.valid && entry.alias == key) return entry;
    }
    return std::nullopt;
}

const std::vector<OtoEntry>& OtoParser::getEntries() const noexcept { return m_entries; }
bool OtoParser::isValid() const noexcept { return m_valid; }
QString OtoParser::error() const { return m_error; }

}