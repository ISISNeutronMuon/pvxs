/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>
#include <cstring>

#include <dbCommon.h>
#include <dbStaticLib.h>


#include "dbentry.h"
#include "dbinfocache.h"

namespace pvxs { namespace ioc { namespace site {

const char* DbInfoCache::Entry::defaultValue() const {
    return defaultNode ? defaultNode->string : nullptr;
}

const char* DbInfoCache::Entry::lookup(const char* key) const {
    auto cmp = [](const std::pair<const char*, dbInfoNode*>& e, const char* k) {
        return strcmp(e.first, k) < 0;
    };
    auto it = std::lower_bound(fields.begin(), fields.end(), key, cmp);
    if (it != fields.end() && strcmp(it->first, key) == 0)
        return it->second->string;
    return defaultValue();
}

bool DbInfoCache::build(dbCommon* prec, const char* prefix, const char* defaultKey)
{
    DBEntry ent(prec);
    const size_t prefixLen = strlen(prefix);

    std::vector<std::pair<const char*, dbInfoNode*>> fields;
    for (auto status = dbFirstInfo(ent); !status; status = dbNextInfo(ent)) {
        if (strncmp(ent->pinfonode->name, prefix, prefixLen) == 0)
            fields.emplace_back(ent->pinfonode->name, ent->pinfonode);
    }
    if (fields.empty())
        return false;

    std::sort(fields.begin(), fields.end(),
              [](const std::pair<const char*, dbInfoNode*>& a,
                 const std::pair<const char*, dbInfoNode*>& b) {
                  return strcmp(a.first, b.first) < 0;
              });

    Entry& entry = map_[prec];
    entry.fields = std::move(fields);

    if (defaultKey) {
        auto cmp = [](const std::pair<const char*, dbInfoNode*>& e, const char* k) {
            return strcmp(e.first, k) < 0;
        };
        auto it = std::lower_bound(entry.fields.begin(), entry.fields.end(), defaultKey, cmp);
        if (it != entry.fields.end() && strcmp(it->first, defaultKey) == 0)
            entry.defaultNode = it->second;
    }
    return true;
}

void DbInfoCache::buildAll(const char* prefix, const char* defaultKey)
{
    DBEntry ent;
    for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent))
        for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent))
            build(static_cast<dbCommon*>(ent->precnode->precord), prefix, defaultKey);
}

}}} // pvxs::ioc::site
