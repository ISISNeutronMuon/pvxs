/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_DBINFOCACHE_H
#define PVXS_DBINFOCACHE_H

#include <unordered_map>
#include <utility>
#include <vector>

struct dbCommon;   // pointer-only use; full definition not needed here
struct dbInfoNode; // pointer-only use; full definition not needed here

namespace pvxs { namespace ioc { namespace facility {

// Per-record cache of dbInfoNode pointers built once at initHookAfterIocBuilt.
// Stores dbInfoNode* rather than const char* so that runtime changes via
// dbPutInfoString() are reflected without rebuilding the cache.
class DbInfoCache {
public:
    struct Entry {
        // Info nodes whose names match the build prefix, sorted by name.
        std::vector<std::pair<const char*, dbInfoNode*>> fields;
        // Optional fallback node (e.g. "Q:DEFAULT_AMSG").
        dbInfoNode* defaultNode = nullptr;

        const char* defaultValue() const;

        // Return the string for key, or the default fallback if key is absent.
        const char* lookup(const char* key) const;
    };

    void clear() { map_.clear(); }
    size_t size() const { return map_.size(); }

    const Entry* find(dbCommon* prec) const {
        auto it = map_.find(prec);
        return it != map_.end() ? &it->second : nullptr;
    }

    // Build and cache an entry for prec containing all info nodes whose name
    // begins with prefix.  If defaultKey is non-null, that node is stored as
    // the fallback returned by Entry::defaultValue() and Entry::lookup().
    Entry& build(dbCommon* prec, const char* prefix, const char* defaultKey = nullptr);

    // Iterate all loaded records and call build() for each.
    void buildAll(const char* prefix, const char* defaultKey = nullptr);

private:
    std::unordered_map<dbCommon*, Entry> map_;
};

}}} // pvxs::ioc::facility

#endif // PVXS_DBINFOCACHE_H
