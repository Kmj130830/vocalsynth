#pragma once
#include <filesystem>
#include <optional>
#include <vector>
#include "Singer/OtoEntry.h"
namespace myvocal {
class OtoParser { public: bool load(const std::filesystem::path& path); bool parseLine(const QString& line,OtoEntry& entry) const; std::optional<OtoEntry> findAlias(const QString& alias) const; const std::vector<OtoEntry>& getEntries() const noexcept; bool isValid() const noexcept; QString error() const; private: std::vector<OtoEntry> m_entries; bool m_valid{false}; QString m_error; };
}
