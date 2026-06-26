.. _facility:

Facility IOC Extensions
========================

This section covers facility-local extensions to PVXS IOC behaviour provided
by the ``facility/`` directory.  Each extension is compiled automatically into
``libpvxsIoc``; removing a file reverts to the default behaviour.  See the
``facility/README.md`` for developer guidance on adding new extensions.

Extension API
-------------

Facility extensions register themselves via the ``pvxs::ioc::facility``
namespace declared in ``ioc/facilityhooks.h``.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Function
     - Purpose
   * - ``addInitHookAtBeginning(fn)``
     - Register a callback for ``initHookAtBeginning``
   * - ``addInitHookAfterIocBuilt(fn)``
     - Register a callback for ``initHookAfterIocBuilt``
   * - ``setNodePostProcessor(fn)``
     - Register a ``void(dbCommon*, Value&)`` called at the end of every ``IOCSource::get()``
