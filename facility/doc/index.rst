.. _facility:

Facility IOC Extensions
========================

This section covers facility-local extensions to PVXS IOC behaviour provided
by the ``facility/`` directory.  Each extension is compiled automatically into
``libpvxsIoc``; removing a file reverts to the default behaviour.  See the
``facility/README.md`` for developer guidance on adding new extensions.

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
     - Used when alarm status is …
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

.. _site_qdisable:

Suppressing PVs from the Network (Q:DISABLE)
---------------------------------------------

The ``qdisable`` extension prevents selected records from being served over
PVA.  A record is suppressed when its ``Q:DISABLE`` info field is set to any
non-zero value:

.. code-block:: none

    info(Q:DISABLE, "1")

Suppressed records are excluded from PV search replies and channel-open
requests.  The filtering is evaluated once at ``initHookAtBeginning``, before
``SingleSource`` is constructed, so it has no runtime overhead.

This is useful for internal bookkeeping records, hardware-simulation records,
or any record that should remain accessible via Channel Access but not via PVA.

Extension API
-------------

Facility extensions register themselves via the ``pvxs::ioc::site``
namespace declared in ``ioc/sitehooks.h``.

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
   * - ``setNodePostProcessor(fn)``
     - Register a ``void(dbCommon*, Value&)`` called at the end of every ``IOCSource::get()``
