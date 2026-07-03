# PVXS IOC Modifications at ISIS

There is a Developer section below. This file starts with guidance for users authoring a .db file.

## Feature Reference

- **Custom Alarm Messages** (`Q:*_AMSG`) — replace the generic alarm
  status/severity a PVA client sees with a human-readable message.
- **PV Filtering** (`Q:pv:disable`, `Q:pv:loopback_only`) — hide a record
  from PVA entirely, or restrict it to clients on the IOC itself.

### Custom Alarm Messages (Q:*_AMSG)

By default a PVA client only sees the alarm severity/status names (e.g.
`HIHI`) in `alarm.message`. Add a `Q:*_AMSG` info field to a record to
replace that with a human-readable string instead:

| Info field | Used when alarm status is ... |
|---|---|
| `Q:HIHI_AMSG` | `HIHI_ALARM` |
| `Q:HIGH_AMSG` | `HIGH_ALARM` |
| `Q:LOLO_AMSG` | `LOLO_ALARM` |
| `Q:LOW_AMSG` | `LOW_ALARM` |
| `Q:STATE<n>_AMSG` | `STATE_ALARM` when `value.index` equals *n* |
| `Q:DEFAULT_AMSG` | Any alarm with no matching specific key above |

For example, an `ai` record with limit alarms:

```
record(ai, "IOC:PRESSURE") {
    field(HIHI, "90")
    field(HHSV, "MAJOR")
    field(HIGH, "80")
    field(HSV,  "MINOR")
    info(Q:HIHI_AMSG, "Pressure critically high - evacuate")
    info(Q:HIGH_AMSG, "Pressure above safe operating limit")
}
```

When `IOC:PRESSURE` goes into `HIHI_ALARM`, PVA clients see "Pressure
critically high - evacuate" in `alarm.message` instead of just `HIHI`.

`Q:STATE<n>_AMSG` works the same way for multi-bit-binary records, keyed on
the numeric state rather than a severity name:

```
record(mbbi, "IOC:MODE") {
    field(ZRST, "Idle")
    field(ONST, "Running")
    field(TWST, "Fault")
    field(TWSV, "MAJOR")
    info(Q:STATE2_AMSG, "Mode fault - check interlocks")
}
```

`Q:DEFAULT_AMSG` is a fallback for any of the five statuses above that
doesn't have its own more specific key set, e.g. a `HIGH_ALARM` with no
`Q:HIGH_AMSG`. It does *not* apply to alarm statuses outside that table --
a `SCAN` or `READ` alarm, for example, is never overridden and shows PVA's
usual raw status name in `alarm.message` (`"SCAN"`, `"READ"`) even if
`Q:DEFAULT_AMSG` is set:

```
record(ai, "IOC:TEMPERATURE") {
    field(HIHI, "90")
    field(HHSV, "MAJOR")
    info(Q:DEFAULT_AMSG, "See ops log for details")
}
```

Here a `HIHI_ALARM` (no `Q:HIHI_AMSG` set) falls back to "See ops log for
details" -- but if this record instead goes into `READ_ALARM` (e.g. its
input link fails), `alarm.message` reads `"READ"`, not "See ops log for
details"; `Q:DEFAULT_AMSG` is never consulted for a status outside the
table.

Any `Q:*_AMSG` field can be changed at runtime
with `dbPutInfoString()`; the new string is used on the next GET, no IOC
restart needed.

### PV Filtering (Q:pv:disable, Q:pv:loopback_only)

Add one of these info fields to a record to restrict how it's exposed over
PVA. Both are independent and read as booleans: any non-empty value other
than the literal string `"0"` counts as set.

- `Q:pv:disable` — hide the record from PVA entirely. It won't appear in a
  PV name list, won't answer a channel search, and can't be opened as a
  channel. Use this for records that should stay Channel Access-only.
- `Q:pv:loopback_only` — restrict the record to clients connecting from the
  IOC itself (127.0.0.0/8 or `::1`). It still appears in the PV name list
  so local tools can find it; get/put/monitor from any other host are
  denied. Use this for records that a local diagnostic tool needs but that
  shouldn't be reachable from the network.

```
record(ai, "IOC:INTERNAL_STATE") {
    info(Q:pv:disable, "1")
}

record(bo, "IOC:CALIBRATE_CMD") {
    info(Q:pv:loopback_only, "1")
}
```

Both flags also apply per-field inside **Group PVs** (`Q:group`, see
`groupsource.cpp`): if a group field is backed by a flagged record, that
field alone is blanked on GET, rejects PUT, and is excluded from monitor
updates — the rest of the group, and the group PV itself, are unaffected.
The IOC also prints a startup warning when a flagged record is added to a
group, so you find out at boot time rather than when a client hits it:

```
record(ai, "IOC:INTERNAL_STATE") {
    field(VAL, "0")
    info(Q:pv:disable, "1")
    info(Q:group, {
        "IOC:STATUS": {
            "internal": {+channel:"VAL"}
        }
    })
}
```

Here `IOC:STATUS.internal` is always blanked and read-only over PVA, even
though the rest of the `IOC:STATUS` group PV works normally — and the IOC
logs a warning for this field when it starts up.

## Developer

### Overview

Beyond the core `pvxsIoc` PVA server implementation, this directory includes
a small set of permanent, always-compiled-in IOC features: custom alarm
messages and PV filtering.
These aren't a pluggable subsystem — each one is a fixed, unconditional part
of `pvxsIoc`, wired up explicitly in `sitehooks.cpp`'s `registerHooks()`.

The internal mechanism they're built on lives in `sitehooks.h`/`sitehooks.cpp`
and `dbentry.h`, described below.

### Hook Registration API (sitehooks.h)

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
iterating registered callbacks itself. `groupsource.cpp` reuses
`isChannelAllowed(pvName, peerAddr)` itself (via a local `fieldAccessAllowed()`
helper) to enforce both flags per-field in Group PVs -- it is not a separate
enforcement path. `isPvDisabled(pvName)`/`isPvLoopbackOnly(pvName)` expose the
individual flags (rather than a single allow/deny decision) purely so
`GroupSource::GroupSource()` can print a specific startup warning when a
flagged record is added to a group; they play no part in enforcement.
