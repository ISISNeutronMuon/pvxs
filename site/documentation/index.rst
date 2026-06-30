.. _site:

Site IOC Extensions
========================

This section covers site-local extensions to PVXS IOC behaviour provided
by the ``site/`` directory.  Each extension is compiled automatically into
``libpvxsIoc``; removing a file reverts to the default behaviour.  See the
``site/README.md`` for developer guidance on adding new extensions.

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
the file (e.g. ``timetag.cpp`` -> ``registerTimetag()``).  The build system
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
