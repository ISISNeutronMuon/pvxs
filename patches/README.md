# ioc/ — PVXS IOC Extensions Reference

## Feature Reference

### Custom Alarm Messages (Q:*_AMSG)

`alarmmsg.cpp` maps EPICS alarm severity to a human-readable string written
into the PVA `alarm.message` field, configured via info fields:

| Info field | Used when alarm status is ... |
|---|---|
| `Q:HIHI_AMSG` | `HIHI_ALARM` |
| `Q:HIGH_AMSG` | `HIGH_ALARM` |
| `Q:LOLO_AMSG` | `LOLO_ALARM` |
| `Q:LOW_AMSG` | `LOW_ALARM` |
| `Q:STATE<n>_AMSG` | `STATE_ALARM` when `value.index` equals *n* |
| `Q:DEFAULT_AMSG` | Any alarm with no matching specific key |

`Q:DEFAULT_AMSG` can be changed at runtime with `dbPutInfoString`; the new
string is used on the next GET with no cache rebuild. Implementation:
`alarmmsg.cpp`, `dbinfocache.{h,cpp}` (the shared per-record info-field cache).

### PV Filtering (Q:pv:disable, Q:pv:loopback_only)

`pvfilter.cpp` provides two independent channel filters:

- `Q:pv:disable` — suppress a record from PVA entirely (name list, search,
  and channel open). Set to any non-zero, non-empty value.
- `Q:pv:loopback_only` — restrict a record to clients connecting via the
  loopback interface (127.0.0.0/8 or `::1`). The record stays visible in the
  PV name list so local tools can still discover it; only get/put/monitor
  from non-loopback peers are denied.

Implementation: `pvfilter.cpp`. Note: peer-address parsing deliberately uses
raw `inet_pton` rather than `pvxs::SockAddr`, since `SockAddr` falls back to
a synchronous DNS lookup on unparseable input and this runs on the PVA
search-reply hot path.

Both flags are also enforced for **Group PVs** (`groupsource.cpp`), per-field:
a group field backed by a flagged record is blanked on GET, rejects PUT, and
is excluded from monitor updates, while the rest of the group and the group
PV itself remain unaffected. `GroupSource::GroupSource()` additionally warns
at group-processing time (via `site::isPvDisabled`/`isPvLoopbackOnly`) when a
flagged record is added to a group, so the `.db` author notices before any
client touches that field.

## Overview

Beyond the core `pvxsIoc` PVA server implementation, this directory includes
a small set of permanent, always-compiled-in IOC features: custom alarm
messages and PV filtering.
These aren't a pluggable subsystem — each one is a fixed, unconditional part
of `pvxsIoc`, wired up explicitly in `sitehooks.cpp`'s `registerHooks()`.

The internal mechanism they're built on lives in `sitehooks.h`/`sitehooks.cpp`
and `dbentry.h`, described below.

## Hook Registration API (sitehooks.h)

| Function | When called | Typical use |
|---|---|---|
| `addInitHookAtBeginning(fn)` | Fires at EPICS `initHookAtBeginning` | Pre-compute per-record data before the IOC is fully built |
| `addInitHookAfterIocBuilt(fn)` | Fires at EPICS `initHookAfterIocBuilt` | Post-IOC-build setup |
| `addNodePostProcessor(fn)` | Fires at the end of every `IOCSource::get()`; multiple may be registered | Override or augment fields in the PVA response (e.g. `alarm.message`) |
| `addChannelFilter(fn)` | `fn(pvName, peerAddr)` returns true to allow, false to deny; multiple may be registered, all must return true | Suppress or restrict a record from being served over PVA |
| `infoFlagSet(val)` | Inline helper, not a hook | Returns true if a boolean-style `Q:*` info field value is present, non-empty, and not the literal string `"0"` |

`forEachRecord(fn)` (in `dbentry.h`, alongside the `DBEntry` wrapper it uses
internally) calls `fn(dbCommon*)` for every record currently loaded in the
database. Used by the features above to build their per-record caches.

`isChannelAllowed(pvName, peerAddr)` and `postProcessNode(prec, node)` are the
dispatch side of `addChannelFilter`/`addNodePostProcessor`: core `ioc/` code
(`singlesource.cpp`, `iocsource.cpp`) calls these once per request rather than
iterating registered callbacks itself. `isPvDisabled(pvName)`/
`isPvLoopbackOnly(pvName)` expose the individual PV-filtering flags (rather
than a single allow/deny decision) for `groupsource.cpp`'s per-field
enforcement and warnings, described above.
