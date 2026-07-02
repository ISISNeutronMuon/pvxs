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
#include "sitehooks.h"

namespace pvxs { namespace ioc { namespace site {

namespace {
// Shared by Entry::lookup() and build()'s defaultKey lookup: find the node
// for key in a fields vector sorted by name, or nullptr if absent.
dbInfoNode* findNode(const std::vector<std::pair<const char*, dbInfoNode*>>& fields, const char* key)
{
    auto cmp = [](const std::pair<const char*, dbInfoNode*>& e, const char* k) {
        return strcmp(e.first, k) < 0;
    };
    auto it = std::lower_bound(fields.begin(), fields.end(), key, cmp);
    return (it != fields.end() && strcmp(it->first, key) == 0) ? it->second : nullptr;
}
} // namespace

const char* DbInfoCache::Entry::defaultValue() const {
    return defaultNode ? defaultNode->string : nullptr;
}

const char* DbInfoCache::Entry::lookup(const char* key) const {
    if (auto* node = findNode(fields, key))
        return node->string;
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

    if (defaultKey)
        entry.defaultNode = findNode(entry.fields, defaultKey);
    return true;
}

void DbInfoCache::buildAll(const char* prefix, const char* defaultKey)
{
    forEachRecord([this, prefix, defaultKey](dbCommon* prec) {
        build(prec, prefix, defaultKey);
    });
}

}}} // pvxs::ioc::site
