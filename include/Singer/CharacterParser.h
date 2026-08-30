#pragma once
#include <filesystem>
#include <QMap>
namespace myvocal { class CharacterParser { public: bool load(const std::filesystem::path&); QString value(const QString&)const; const QMap<QString,QString>& values()const noexcept; private: QMap<QString,QString> m_values; }; }
