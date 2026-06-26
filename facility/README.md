# facility/

This directory contains optional, facility-local extensions to PVXS IOC behaviour.
Any `.cpp` file placed here is automatically compiled into `libpvxsIoc` by the
build system — no Makefile editing required.  Removing a file reverts to the
default behaviour.

## How it works

Each source file registers its behaviour at static-initialisation time (library
load), before `iocInit()` is called.  The registration API is in
`ioc/facilityhooks.h` and lives in the `pvxs::ioc::facility` namespace.

### Available hooks

| Function | When called | Typical use |
|---|---|---|
| `markFiltered(name)` | Call during `addInitHookAtBeginning` callback | Suppress a record from being served as a PV |
| `addInitHookAtBeginning(fn)` | Fires at EPICS `initHookAtBeginning` | Pre-compute per-record data before the IOC is fully built |
| `addInitHookAfterIocBuilt(fn)` | Fires at EPICS `initHookAfterIocBuilt` | Post-IOC-build setup |
| `setNodePostProcessor(fn)` | Registered once; called at the end of every `IOCSource::get()` | Override or augment fields in the PVA response (e.g. `alarm.message`) |

### Filtering records

To suppress a record from being served as a PV, call `facility::markFiltered` for
each record name during an `addInitHookAtBeginning` callback.  Filtered records
are excluded from the PV list, search responses, and channel-open requests.

See `qdisable.cpp` for a complete example that reads the `Q:DISABLE` info field
at `initHookAtBeginning` and marks matching records.

### Modifying PVA responses

`setNodePostProcessor` registers a single `void(dbCommon*, Value&)` callback
that is called at the end of `IOCSource::get()` after all standard fields have
been populated.  The callback may read any field from the node and overwrite it.
The record is locked for the duration of the call, so `prec->stat` and other
record fields are safe to read.

Only one post-processor may be registered; a second call to
`setNodePostProcessor` replaces the first.

See `alarmmsg.cpp` for a complete example that reads per-record `Q:*_AMSG` info
fields and writes a custom `alarm.message`.

## Adding a new extension

1. Create a `.cpp` file in this directory.
2. Include `"facilityhooks.h"`.
3. Define your logic in a static `Registrar` struct whose constructor calls the
   relevant `pvxs::ioc::facility::` registration functions.
4. Build — the file is picked up automatically.

Minimal template:

```cpp
#include "facilityhooks.h"

namespace {

void onBeginning()
{
    // called at initHookAtBeginning — database is loaded, IOC not yet running
}

struct Registrar {
    Registrar() {
        pvxs::ioc::facility::addInitHookAtBeginning(onBeginning);
    }
} s_registrar;

} // namespace
```

## Tests

Unit tests for facility extensions live in `test/`.  Each test links
against the extension file it is testing plus `libpvxsIoc`.  See the existing
`testqdisable` and `testalarmmsg` targets in `test/Makefile` for the pattern.
