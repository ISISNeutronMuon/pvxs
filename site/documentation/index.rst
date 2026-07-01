.. _site:

Site IOC Extensions
========================

This section covers site-local extensions to PVXS IOC behaviour provided
by the ``site/`` directory.  Each extension is compiled automatically into
``libpvxsIoc``; removing a file reverts to the default behaviour.  See the
``site/README.md`` for developer guidance on adding new extensions.

.. _site_alarmmsg:

Custom Alarm Messages
---------------------

The ``alarmmsg`` extension maps EPICS alarm severity codes to human-readable
strings, written into the PVA ``alarm.message`` field on every
:func:`IOCSource::get` call.

Configuration is done entirely through EPICS ``info`` fields on individual
records:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Info field
     - Used when alarm status is ...
   * - ``Q:HIHI_AMSG``
     - ``HIHI_ALARM``
   * - ``Q:HIGH_AMSG``
     - ``HIGH_ALARM``
   * - ``Q:LOLO_AMSG``
     - ``LOLO_ALARM``
   * - ``Q:LOW_AMSG``
     - ``LOW_ALARM``
   * - ``Q:STATE<n>_AMSG``
     - ``STATE_ALARM`` when ``value.index`` equals *n*
   * - ``Q:DEFAULT_AMSG``
     - Any alarm with no matching specific key

Messages longer than 40 characters are transmitted in full; the standard
``DB_AMSG_SIZE`` limit does not apply to the PVA ``alarm.message`` field.

The value of ``Q:DEFAULT_AMSG`` can be changed at runtime with
``dbPutInfoString``.  The new string is used on the next GET without any
cache rebuild.

Example database fragment::

    record(ai, "$(P)temperature") {
        field(HIHI, "90")
        field(HIGH, "80")
        field(LOW,  "10")
        field(LOLO, "5")
        field(HHSV, "MAJOR")
        field(HSV,  "MINOR")
        field(LSV,  "MINOR")
        field(LLSV, "MAJOR")
        info(Q:HIHI_AMSG, "Temperature critically high")
        info(Q:HIGH_AMSG, "Temperature elevated")
        info(Q:LOW_AMSG,  "Temperature low")
        info(Q:LOLO_AMSG, "Temperature critically low")
        info(Q:DEFAULT_AMSG, "Temperature out of range")
    }

.. _site_defaultprec:

Default Floating-Point Precision (Q:PREC_ZERO)
----------------------------------------------

The ``defaultprec`` extension sets ``display.precision`` to 2 for any
floating-point record whose ``PREC`` field is 0, preventing clients from
displaying values with no decimal places simply because the database author
omitted an explicit ``PREC`` setting.

Only records whose PVA ``value`` field has a ``Kind::Real`` type (``float``,
``double``, or their array variants) are affected.  Integer, enum, and string
records are left untouched.

To preserve ``precision = 0`` for a specific record, set the ``Q:PREC_ZERO``
info field to any non-zero value:

.. code-block:: none

    record(ai, "$(P)counter") {
        field(PREC, "0")
        info(Q:PREC_ZERO, "1")
    }

The opt-out is evaluated once at ``initHookAtBeginning`` and has no per-GET
overhead.

.. _site_pvfilter:

Suppressing and Restricting PVs (Q:pv:disable, Q:pv:loopback_only)
--------------------------------------------------------------------

The ``pvfilter`` extension provides two info fields for controlling which
records are visible over PVA.

**Q:pv:disable** -- suppress a record from PVA entirely.  A record is
suppressed when the info field is set to any non-zero value:

.. code-block:: none

    info(Q:pv:disable, "1")

Suppressed records are excluded from the PV name list, search replies, and
channel-open requests.  This is useful for internal bookkeeping records,
hardware-simulation records, or any record that should remain accessible via
Channel Access but not via PVA.

**Q:pv:loopback_only** -- restrict a record to clients connecting via the
loopback interface.  Clients on other network interfaces receive no search
reply and cannot open a channel:

.. code-block:: none

    info(Q:pv:loopback_only, "1")

The record remains visible in the PV name list so that local tools can
discover it.  Only connections from the full IPv4 loopback range
(``127.0.0.0/8``) and the IPv6 loopback address (``::1``) are accepted.

Both filters are evaluated once at ``initHookAtBeginning`` and have no
per-request overhead.

.. _site_timetag:

Timestamp Tag Bits (Q:time:tag)
-------------------------------

The ``timetag`` extension reserves the lowest N bits of a record's nanosecond
timestamp for a user-defined tag, packing them into ``timeStamp.userTag`` and
clearing them from ``timeStamp.nanoseconds`` on every PVA get and monitor
update.

Configure it with a single ``info`` field on the record:

.. code-block:: none

    info(Q:time:tag, "nsec:lsb:N")

where ``N`` is the number of bits to reserve (1-32).  Any other format is
silently ignored and the timestamp is left unmodified.

Example -- reserve the lowest 8 bits:

.. code-block:: none

    record(ai, "$(P)value") {
        info(Q:time:tag, "nsec:lsb:8")
    }

For a step-by-step explanation of how this extension is built, see the
:ref:`tutorial <site_tutorial_timetag>`.

.. toctree::
   :maxdepth: 1

   tutorial_timetag

Extension API
-------------

Site extensions are picked up automatically from ``site/*.cpp``.  Each
file must define exactly one ``registerXxx()`` function in the
``pvxs::ioc::site`` namespace, where ``Xxx`` is the CamelCase basename of
the file (e.g. ``alarmmsg.cpp`` -> ``registerAlarmmsg()``).  The build system
collects these functions and calls them all from ``pvxsBaseRegistrar()`` before
``iocInit()`` runs.

The registration functions declared in ``ioc/sitehooks.h`` are:

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Function
     - Purpose
   * - ``markFiltered(name)``
     - Suppress a record from PVA (call inside an ``addInitHookAtBeginning`` callback)
   * - ``addInitHookAtBeginning(fn)``
     - Register a callback for ``initHookAtBeginning``
   * - ``addInitHookAfterIocBuilt(fn)``
     - Register a callback for ``initHookAfterIocBuilt``
   * - ``addNodePostProcessor(fn)``
     - Register a ``void(dbCommon*, Value&)`` called at the end of every ``IOCSource::get()``; multiple may be registered and are fired in order
