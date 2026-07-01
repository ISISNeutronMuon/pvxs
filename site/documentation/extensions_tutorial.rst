.. _site_extensions_tutorial:

Writing a Site Extension: Default Precision
===========================================

This tutorial walks through writing ``site/defaultprec.cpp`` from scratch.
The problem it solves is a common annoyance: newly written EPICS databases
often leave ``PREC=0`` on floating-point records because 0 is the default,
causing PVA clients to display values with no decimal places.  The extension
promotes such records to ``display.precision = 2`` without modifying any
database files.

After completing the tutorial you will understand how to:

- register a post-processor that runs after every ``IOCSource::get()`` call,
- inspect the PVA node structure to check record types and modify fields, and
- move expensive per-record setup into an ``initHookAtBeginning`` callback so
  that the post-processor itself stays cheap.

Step 1: Skeleton
----------------

Create ``site/defaultprec.cpp``.  The build system discovers extension files
automatically -- no Makefile changes are needed.  Every extension defines
exactly one ``registerXxx()`` function in the ``pvxs::ioc::site`` namespace,
where ``Xxx`` is the CamelCase basename of the file.  For ``defaultprec.cpp``
that is ``registerDefaultprec``.

.. code-block:: c++

    #include "sitehooks.h"

    namespace {

    void applyDefaultPrecision(dbCommon* prec, pvxs::Value& node)
    {
    }

    } // namespace

    namespace pvxs { namespace ioc { namespace site {
    void registerDefaultprec() {
        addNodePostProcessor(applyDefaultPrecision);
    }
    }}} // pvxs::ioc::site

``addNodePostProcessor`` registers a ``void(dbCommon*, Value&)`` callback
that is called at the end of every ``IOCSource::get()`` after all standard
fields have been populated.  The record is locked for the duration of the
call.  Multiple callbacks may be registered and fire in registration order.

Building at this point compiles and links correctly; the callback is a no-op.

Step 2: Overriding Precision
-----------------------------

The ``Value& node`` argument holds the PVA structure that ``IOCSource::get()``
is about to return to the client.  The ``value`` sub-field carries the record
value; its ``Kind`` identifies the EPICS type family.

Fill in ``applyDefaultPrecision``:

.. code-block:: c++

    void applyDefaultPrecision(dbCommon* prec, pvxs::Value& node)
    {
        auto val = node["value"];
        if (!val || val.type().kind() != pvxs::Kind::Real)
            return;
        if (auto fld = node["display.precision"]) {
            if (fld.as<int32_t>() == 0)
                fld = int32_t(2);
        }
    }

A few points worth noting:

- ``Kind::Real`` covers ``float``, ``double``, and their array variants.
  Integer, enum, and string records have a different ``Kind`` and are
  skipped unconditionally.

- ``node["display.precision"]`` returns an invalid ``Value`` (evaluates as
  ``false``) when the field is absent, so the ``if (auto fld = ...)`` guard
  is safe without a separate existence check.

- Assigning to ``fld`` writes back into ``node`` because ``Value`` uses
  shared ownership; there is no separate "commit" step.

The extension is already functional at this point.  Every floating-point
record with ``PREC=0`` will be served to PVA clients with
``display.precision = 2``.

Step 3: Adding a Per-Record Opt-Out
-------------------------------------

Some records genuinely need ``precision = 0`` -- for example, a counter that
displays integer values even though its record type is ``ai``.  The extension
should let the database author opt out via an ``info`` field.

The natural place to check the info field would be inside
``applyDefaultPrecision`` itself.  That callback runs on every
``IOCSource::get()``, however, so calling ``dbFindInfo`` on every GET would
add overhead proportional to the call rate.  A better approach is to scan all
records once during ``initHookAtBeginning`` -- after all database files have
been loaded but before the IOC begins processing -- and collect the exempt
records into a set.  The post-processor then only needs a hash-set lookup.

Add the extra headers and the set accessor at the top of the file:

.. code-block:: c++

    #include <cstring>
    #include <unordered_set>

    #include "dbentry.h"
    #include "sitehooks.h"

    namespace {

    std::unordered_set<dbCommon*>& precZeroSet() {
        static std::unordered_set<dbCommon*> s;
        return s;
    }

The set is returned by a function (a Meyers singleton) rather than declared
at namespace scope.  Namespace-scope statics produce
``__static_initialization_and_destruction`` symbols that fail the CDT check
run during CI; function-local statics are initialised on first call and are
safe.

Now add the init-hook callback that populates the set:

.. code-block:: c++

    void buildPrecZeroSet()
    {
        auto& s = precZeroSet();
        s.clear();
        pvxs::ioc::DBEntry ent;
        for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent)) {
            for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent)) {
                auto* prec = static_cast<dbCommon*>(ent->precnode->precord);
                pvxs::ioc::DBEntry infoEnt(prec);
                const char* val = infoEnt.info("Q:PREC_ZERO");
                if (val && val[0] != '\0' && strcmp(val, "0") != 0)
                    s.insert(prec);
            }
        }
    }

``pvxs::ioc::DBEntry`` wraps the EPICS ``DBENTRY`` type from ``dbAccess.h``.
Constructing it with no arguments calls ``dbInitEntry`` on the global
database; constructing it with a ``dbCommon*`` navigates directly to that
record's info list.  ``DBEntry::info(key)`` calls ``dbFindInfo`` and returns
the string value, or ``nullptr`` if the key is absent.

The condition ``val && val[0] != '\0' && strcmp(val, "0") != 0`` treats any
non-empty, non-``"0"`` string as a truthy opt-out, matching the convention
used throughout PVXS info fields.

Update ``applyDefaultPrecision`` to consult the set before touching the node:

.. code-block:: c++

    void applyDefaultPrecision(dbCommon* prec, pvxs::Value& node)
    {
        auto val = node["value"];
        if (!val || val.type().kind() != pvxs::Kind::Real)
            return;
        if (precZeroSet().count(prec))
            return;
        if (auto fld = node["display.precision"]) {
            if (fld.as<int32_t>() == 0)
                fld = int32_t(2);
        }
    }

Register both callbacks in ``registerDefaultprec``:

.. code-block:: c++

    namespace pvxs { namespace ioc { namespace site {
    void registerDefaultprec() {
        addInitHookAtBeginning(buildPrecZeroSet);
        addNodePostProcessor(applyDefaultPrecision);
    }
    }}} // pvxs::ioc::site

The database author can now opt a record out of the precision upgrade:

.. code-block:: none

    record(ai, "$(P)counter") {
        field(PREC, "0")
        info(Q:PREC_ZERO, "1")
    }

The opt-out is evaluated once at startup; there is no per-GET overhead for
records that carry the info field.

Complete Listing
-----------------

.. code-block:: c++

    #include <cstring>
    #include <unordered_set>

    #include "dbentry.h"
    #include "sitehooks.h"

    namespace {

    std::unordered_set<dbCommon*>& precZeroSet() {
        static std::unordered_set<dbCommon*> s;
        return s;
    }

    void buildPrecZeroSet()
    {
        auto& s = precZeroSet();
        s.clear();
        pvxs::ioc::DBEntry ent;
        for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent)) {
            for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent)) {
                auto* prec = static_cast<dbCommon*>(ent->precnode->precord);
                pvxs::ioc::DBEntry infoEnt(prec);
                const char* val = infoEnt.info("Q:PREC_ZERO");
                if (val && val[0] != '\0' && strcmp(val, "0") != 0)
                    s.insert(prec);
            }
        }
    }

    void applyDefaultPrecision(dbCommon* prec, pvxs::Value& node)
    {
        auto val = node["value"];
        if (!val || val.type().kind() != pvxs::Kind::Real)
            return;
        if (precZeroSet().count(prec))
            return;
        if (auto fld = node["display.precision"]) {
            if (fld.as<int32_t>() == 0)
                fld = int32_t(2);
        }
    }

    } // namespace

    namespace pvxs { namespace ioc { namespace site {
    void registerDefaultprec() {
        addInitHookAtBeginning(buildPrecZeroSet);
        addNodePostProcessor(applyDefaultPrecision);
    }
    }}} // pvxs::ioc::site
