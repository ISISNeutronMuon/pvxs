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

#include <pvxs/iochooks.h>

struct dbCommon;   // pointer-only use; full definition not needed here
struct dbInfoNode; // pointer-only use; full definition not needed here

namespace pvxs { namespace ioc { namespace site {

// Per-record cache of dbInfoNode pointers built once at initHookAfterIocBuilt.
// Stores dbInfoNode* rather than const char* so that runtime changes via
// dbPutInfoString() are reflected without rebuilding the cache.
//
// PVXS_IOC_API is required here: out-of-line methods (build(), buildAll(),
// Entry::lookup(), Entry::defaultValue()) are defined in dbinfocache.cpp and
// called from other translation units (e.g. alarmmsg.cpp) within the same
// pvxsIoc library; without dllexport/dllimport the MSVC linker can't see them
// (GCC/Clang exports everything by default, so this only goes wrong on Windows).
// The nested Entry struct needs its own annotation since MSVC does not
// propagate a class's dllexport to its nested types.
class PVXS_IOC_API DbInfoCache {
public:
    struct PVXS_IOC_API Entry {
        // Info nodes whose names match the build prefix.
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
    // Returns true if at least one matching field was found and an entry was
    // inserted; false if the record has no matching fields (nothing is cached).
    bool build(dbCommon* prec, const char* prefix, const char* defaultKey = nullptr);

    // Iterate all loaded records and call build() for each.
    void buildAll(const char* prefix, const char* defaultKey = nullptr);

private:
    std::unordered_map<dbCommon*, Entry> map_;
};

}}} // pvxs::ioc::site

#endif // PVXS_DBINFOCACHE_H
