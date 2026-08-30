#pragma once
#include <filesystem>
#include <memory>
#include "Singer/SingerInfo.h"
#include "Singer/OtoParser.h"
namespace myvocal { class Singer { public: explicit Singer(const std::filesystem::path& root); bool load(); const SingerInfo& info()const noexcept; const OtoParser& oto()const noexcept; bool isValid()const noexcept; std::filesystem::path path()const; private: std::filesystem::path m_root; SingerInfo m_info; OtoParser m_oto; bool m_valid{false}; }; }
