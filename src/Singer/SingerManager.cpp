#include "Singer/SingerManager.h"

#include <set>

namespace myvocal {

namespace {
std::filesystem::path normalizePath(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }
    return std::filesystem::absolute(path, ec);
}

void collectVoiceBankRoots(const std::filesystem::path& requested,
                           std::vector<std::filesystem::path>& out)
{
    if (requested.empty()) {
        return;
    }

    out.push_back(requested);

    // The executable is normally inside out/build/... while VoiceBanks lives
    // at the project root. Walk upward so launching from the IDE or build
    // directory does not make the bundled banks invisible.
    std::filesystem::path cursor = requested;
    if (cursor.filename() == "VoiceBanks") {
        cursor = cursor.parent_path();
    }

    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        out.push_back(cursor / "VoiceBanks");
        const auto parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
}
}

void SingerManager::scan(const std::filesystem::path& root)
{
    scan(std::vector<std::filesystem::path>{root});
}

void SingerManager::scan(const std::vector<std::filesystem::path>& roots)
{
    m_singers.clear();
    m_roots.clear();

    std::vector<std::filesystem::path> expandedRoots;
    for (const auto& root : roots) {
        collectVoiceBankRoots(root, expandedRoots);
    }

    std::set<std::filesystem::path> uniqueRoots;
    for (const auto& root : expandedRoots) {
        if (root.empty()) {
            continue;
        }

        const auto normalized = normalizePath(root);
        if (!std::filesystem::exists(normalized) ||
            !std::filesystem::is_directory(normalized)) {
            continue;
        }

        if (uniqueRoots.insert(normalized).second) {
            m_roots.push_back(normalized);
        }
    }

    std::set<std::filesystem::path> uniqueSingers;
    for (const auto& root : m_roots) {
        std::error_code ec;
        std::filesystem::directory_iterator it(root, ec);
        const std::filesystem::directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (!it->is_directory()) {
                continue;
            }

            const auto singerPath = normalizePath(it->path());
            if (uniqueSingers.insert(singerPath).second) {
                add(singerPath);
            }
        }
    }
}

void SingerManager::scanDefaultPaths(const std::filesystem::path& exeDir,
                                     const std::filesystem::path& projectRoot,
                                     const std::filesystem::path& workingDir)
{
    scan({exeDir / "VoiceBanks", projectRoot / "VoiceBanks",
          workingDir / "VoiceBanks"});
}

bool SingerManager::add(const std::filesystem::path& path)
{
    const auto normalized = normalizePath(path);
    if (!std::filesystem::is_directory(normalized)) {
        return false;
    }

    for (const auto& existing : m_singers) {
        if (normalizePath(existing->path()) == normalized) {
            return existing->isValid();
        }
    }

    auto singer = std::make_shared<Singer>(normalized);
    singer->load();
    m_singers.push_back(std::move(singer));
    return m_singers.back()->isValid();
}

const std::vector<std::shared_ptr<Singer>>& SingerManager::singers() const noexcept
{
    return m_singers;
}

std::shared_ptr<Singer> SingerManager::findByName(const QString& name) const
{
    for (const auto& singer : m_singers) {
        if (singer->info().name.compare(name, Qt::CaseInsensitive) == 0) {
            return singer;
        }
    }
    return nullptr;
}

std::shared_ptr<Singer> SingerManager::findByPath(const QString& path) const
{
    if (path.isEmpty()) {
        return nullptr;
    }

    const auto wanted = normalizePath(std::filesystem::path(path.toStdWString()));
    for (const auto& singer : m_singers) {
        if (normalizePath(singer->path()) == wanted) {
            return singer;
        }
    }
    return nullptr;
}

QStringList SingerManager::searchRoots() const
{
    QStringList result;
    for (const auto& root : m_roots) {
        result.append(QString::fromStdWString(root.wstring()));
    }
    return result;
}

int SingerManager::validCount() const noexcept
{
    int count = 0;
    for (const auto& singer : m_singers) {
        if (singer->isValid()) {
            ++count;
        }
    }
    return count;
}

}
