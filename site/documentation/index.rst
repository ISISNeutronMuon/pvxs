.. _site:

Site IOC Extensions
========================

This section is intended to cover site-local extensions to PVXS IOC behaviour provided
by the ``site/`` directory.  Each extension is compiled automatically into
``libpvxsIoc``; removing a file reverts to the default behaviour.  See the
``site/README.md`` for developer guidance on adding new extensions.

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
