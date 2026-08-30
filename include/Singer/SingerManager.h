#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include "Singer/Singer.h"
namespace myvocal { class SingerManager { public: void scan(const std::filesystem::path& root); bool add(const std::filesystem::path&); const std::vector<std::shared_ptr<Singer>>& singers()const noexcept; std::shared_ptr<Singer> findByName(const QString&)const; private: std::vector<std::shared_ptr<Singer>> m_singers; }; }
