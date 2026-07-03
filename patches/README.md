# PVXS IOC Modifications at ISIS

There is a Developer section below. This file starts with guidance for users authoring a .db file.

## Feature Reference

- **Custom Alarm Messages** (`Q:*_AMSG`) — replace the generic alarm
  status/severity a PVA client sees with a human-readable message.
- **PV Filtering** (`Q:pva:access`) — hide a record from PVA entirely, or
  restrict it to clients on the IOC itself.

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

### PV Filtering (Q:pva:access)

Add `Q:pva:access` to a record to restrict how it's exposed over PVA. It's a
single field with one of three values, so a record can never end up with
two conflicting restrictions at once -- there's nowhere to write both:

| Value | Effect |
|---|---|
| *(absent)* | Normal access, no restriction |
| `"enable"` | Same as absent -- an explicit way to say "no restriction" |
| `"disable"` | Hide the record from PVA entirely: no search response, no channel open, for any client. Use for records that should stay Channel Access-only. |
| `"loopback_only"` | Discoverable and usable by clients connecting from the IOC itself (127.0.0.0/8 or `::1`); invisible to everyone else. A remote client's PVA search for the name is never answered, so the record doesn't just reject remote get/put/monitor -- it doesn't appear to exist at all from off the IOC. Use for records a local diagnostic tool needs but that shouldn't be reachable from the network. |

Values not in the table above (probably typos) are treated as "enable" and a warning is logged at startup.

```
record(ai, "IOC:INTERNAL_STATE") {
    info(Q:pva:access, "disable")
}

record(bo, "IOC:CALIBRATE_CMD") {
    info(Q:pva:access, "loopback_only")
}
```

#### Groups

`Q:pva:access` also applies per-field inside **Group PVs** (`Q:group`, see
`groupsource.cpp`): if a group field is backed by a restricted record, that
field alone is left out of the GET response entirely (not sent as a
zeroed/blank value -- omitted), rejects PUT, and is excluded from monitor
updates for any client the restriction applies to — the rest of the group,
and the group PV itself, are unaffected. The IOC also prints a startup
warning when a restricted record is added to a group, so you find out at
boot time rather than when a client hits it:

```
record(ai, "IOC:PRESSURE_STATUS") {
    field(VAL, "21.5")
    info(Q:group, {
        "IOC:STATUS": {
            "pressure": {+channel:"VAL"}
        }
    })
}

record(ai, "IOC:PRESSURE_RAW") {
    field(VAL, "1013.2")
    info(Q:pva:access, "loopback_only")
    info(Q:group, {
        "IOC:STATUS": {
            "raw": {+channel:"VAL", +putorder:0}
        }
    })
}
```

`IOC:STATUS` isn't defined by its own `record()` block -- like any Group PV,
it's created implicitly by collecting the `Q:group` info fields scattered
across its member records (here, `IOC:PRESSURE_STATUS` and `IOC:PRESSURE_RAW`;
see `documentation/qgroup.rst` and the `atomic:src` group in
`test/testpvalink.db` for further worked examples of this same pattern).
`+putorder` is needed for `raw` to be writable through the group at all
-- without it, a PUT would already be rejected for an unrelated reason ("no
putorder"), and wouldn't demonstrate `Q:pva:access` doing anything.

Unlike `"disable"`, `"loopback_only"` makes `IOC:STATUS.raw` depend on who's
asking, not a blanket restriction. Verified against a real `softIocPVX`
running this exact `.db`, queried with `pvxget -v IOC:STATUS`:

- Run **on the IOC host itself** (or anywhere else connecting via
  127.0.0.0/8 or `::1`), `raw` comes back with its real value alongside
  `pressure`:
  ```
  IOC:STATUS
      pressure.value double = 21.5
      ...
      raw.value double = 1013.2
      ...
  ```
- Run from **any other machine**, `pressure` is unaffected but every
  `raw.*` field is missing from the response entirely -- not present with
  a zeroed/blank value, just absent:
  ```
  IOC:STATUS
      pressure.value double = 21.5
      ...
  ```
  A PUT targeting `raw` through the group is rejected (`pvxput` gets a
  `RemoteError`, and the IOC logs `IOC:STATUS : raw: Q:pva:access
  restricted, ignore write`), and a remote client's monitor of `IOC:STATUS`
  never delivers updates for `raw` specifically.

Either way `IOC:STATUS` itself is served without error -- `raw` is the only
thing that differs by peer, not whether the group PV responds at all. The
IOC logs a startup warning for the `raw` field regardless of which value
(`"disable"` or `"loopback_only"`) is set.

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

`forEachRecord(fn)` (in `dbentry.h`, alongside the `DBEntry` wrapper it uses
internally) calls `fn(dbCommon*)` for every record currently loaded in the
database. Used by the features above to build their per-record caches.

`isChannelAllowed(pvName, peerAddr)` and `postProcessNode(prec, node)` are the
dispatch side of `addChannelFilter`/`addNodePostProcessor`: core `ioc/` code
(`singlesource.cpp`, `iocsource.cpp`) calls these once per request rather than
iterating registered callbacks itself. `groupsource.cpp` reuses
`isChannelAllowed(pvName, peerAddr)` itself (via a local `fieldAccessAllowed()`
helper) to enforce `Q:pva:access` per-field in Group PVs -- it is not a separate
enforcement path. `isPvDisabled(pvName)`/`isPvLoopbackOnly(pvName)` expose the
individual `Q:pva:access` values (rather than a single allow/deny decision)
purely so `GroupSource::GroupSource()` can print a specific startup warning
when a restricted record is added to a group; they play no part in
enforcement.
