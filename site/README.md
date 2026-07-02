# Site-Specific PVXS Extensions

This directory contains optional, site-local extensions to PVXS IOC behaviour.
Any `.cpp` file placed here is automatically compiled into `libpvxsIoc` by the
build system - no Makefile editing required.  Removing a file reverts to the
default behaviour.

## How it works

Each source file defines a `registerXxx()` function in the
`pvxs::ioc::site` namespace, where `Xxx` is the CamelCase basename of the
file (e.g. `myextension.cpp` -> `registerMyextension()`).  At build time,
`gen_siteregister.py` scans the directory, collects every such function,
and generates `siteregister.cpp` which calls them all from
`registerSiteExtensions()`.  `registerSiteExtensions()` is invoked from
`pvxsBaseRegistrar()`, before `iocInit()` is called.

The registration API is in `ioc/sitehooks.h` and lives in the
`pvxs::ioc::site` namespace.

### Available hooks

| Function | When called | Typical use |
|---|---|---|
| `markFiltered(name)` | Call during `addInitHookAtBeginning` callback | Suppress a record from being served as a PV |
| `addInitHookAtBeginning(fn)` | Fires at EPICS `initHookAtBeginning` | Pre-compute per-record data before the IOC is fully built |
| `addInitHookAfterIocBuilt(fn)` | Fires at EPICS `initHookAfterIocBuilt` | Post-IOC-build setup |
| `addNodePostProcessor(fn)` | Fires at the end of every `IOCSource::get()`; multiple may be registered | Override or augment fields in the PVA response (e.g. `alarm.message`) |

### Iterating records

`pvxs::ioc::forEachRecord(fn)` (declared in `ioc/dbentry.h`, alongside the
`DBEntry` wrapper it uses internally) calls `fn(dbCommon*)` for every record
currently loaded in the database. Use it from an `addInitHookAtBeginning` or
`addInitHookAfterIocBuilt` callback instead of hand-writing the
`dbFirstRecordType`/`dbFirstRecord` double loop - the example extensions in
this directory use it to build their per-record caches.

### Filtering records

To suppress a record from being served as a PV, call `site::markFiltered` for
each record name during an `addInitHookAtBeginning` callback.  Filtered records
are excluded from the PV list, search responses, and channel-open requests.


### Modifying PVA responses

`addNodePostProcessor` registers a `void(dbCommon*, Value&)` callback
that is called at the end of `IOCSource::get()` after all standard fields have
been populated.  Multiple callbacks may be registered; they are fired in
registration order.  The callback may read any field from the node and overwrite
it.  The record is locked for the duration of the call, so `prec->stat` and
other record fields are safe to read.

## Adding a new extension

1. Create a `.cpp` file in this directory.
2. Include `"sitehooks.h"`.
3. Define a `registerXxx()` function in the `pvxs::ioc::site` namespace
   (where `Xxx` is the CamelCase basename of your file) and call the relevant
   registration functions inside it.  Do not use namespace-scope static objects
   - they produce GCC static-constructor symbols that fail the CDT check.
4. Build - `gen_siteregister.py` discovers the function automatically and
   wires it into `registerSiteExtensions()`.

Minimal template (file named `myextension.cpp`):

```cpp
#include "sitehooks.h"

namespace {

void onBeginning()
{
    // called at initHookAtBeginning - database is loaded, IOC not yet running
}

} // namespace

namespace pvxs { namespace ioc { namespace site {
void registerMyextension() {
    addInitHookAtBeginning(onBeginning);
}
}}} // pvxs::ioc::site
```

## Tests

Unit tests for site extensions live in `site/test/`.  Each test links
against the extension file it is testing plus `libpvxsIoc`.  See the existing
tests in `site/test/` for the pattern.
