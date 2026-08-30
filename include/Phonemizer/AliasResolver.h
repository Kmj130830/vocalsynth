#pragma once
#include <QHash>
#include <QString>
#include <optional>
#include "Singer/OtoEntry.h"
namespace myvocal { class Singer; class AliasResolver { public: explicit AliasResolver(const Singer&); std::optional<OtoEntry> resolve(const QString&alias); void clearCache(); private: const Singer& m_singer; QHash<QString,std::optional<OtoEntry>> m_cache; }; }
