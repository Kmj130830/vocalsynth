#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include "Singer/Singer.h"

namespace myvocal {

class SingerManager {
public:
    void scan(const std::filesystem::path& root);
    void scan(const std::vector<std::filesystem::path>& roots);
    void scanDefaultPaths(const std::filesystem::path& exeDir,
                          const std::filesystem::path& projectRoot,
                          const std::filesystem::path& workingDir);
    bool add(const std::filesystem::path& path);

    const std::vector<std::shared_ptr<Singer>>& singers() const noexcept;
    std::shared_ptr<Singer> findByName(const QString& name) const;
    std::shared_ptr<Singer> findByPath(const QString& path) const;

    QStringList searchRoots() const;
    int validCount() const noexcept;

private:
    std::vector<std::shared_ptr<Singer>> m_singers;
    std::vector<std::filesystem::path> m_roots;
};

}
