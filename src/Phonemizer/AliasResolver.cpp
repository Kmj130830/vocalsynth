#include "Phonemizer/AliasResolver.h"
#include "Singer/Singer.h"
namespace myvocal { AliasResolver::AliasResolver(const Singer&s):m_singer(s){} std::optional<OtoEntry>AliasResolver::resolve(const QString&a){if(m_cache.contains(a))return m_cache.value(a);auto e=m_singer.oto().findAlias(a);if(!e)e=m_singer.oto().findAlias(a.trimmed());m_cache.insert(a,e);return e;}void AliasResolver::clearCache(){m_cache.clear();} }
