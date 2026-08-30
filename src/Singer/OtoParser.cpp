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
    if (equal <= 0) {
        return false;
    }

    entry.filename = line.left(equal).trimmed();
    const QStringList values = line.mid(equal + 1).split(',');
    if (values.size() != 6) {
        return false;
    }

    entry.alias = values.at(0).trimmed();
    double numeric[5]{};
    for (int i = 0; i < 5; ++i) {
        if (!parseDouble(values.at(i + 1), numeric[i])) {
            return false;
        }
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

    bool hasInvalidLine = false;
    int lineNumber = 0;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine();
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            continue;
        }

        OtoEntry entry;
        if (parseLine(line, entry)) {
            m_entries.push_back(std::move(entry));
        } else {
            hasInvalidLine = true;
            if (m_error.isEmpty()) {
                m_error = QStringLiteral("Invalid oto.ini line %1").arg(lineNumber);
            }
        }
    }

    m_valid = !m_entries.empty() && !hasInvalidLine;
    if (!m_valid && m_error.isEmpty()) {
        m_error = QStringLiteral("No valid oto.ini entries");
    }
    return m_valid;
}

std::optional<OtoEntry> OtoParser::findAlias(const QString& alias) const
{
    for (const auto& entry : m_entries) {
        if (entry.valid && entry.alias == alias) {
            return entry;
        }
    }
    return std::nullopt;
}

const std::vector<OtoEntry>& OtoParser::getEntries() const noexcept
{
    return m_entries;
}

bool OtoParser::isValid() const noexcept
{
    return m_valid;
}

QString OtoParser::error() const
{
    return m_error;
}

}
