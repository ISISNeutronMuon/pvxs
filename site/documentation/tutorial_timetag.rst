.. _site_tutorial_timetag:

Tutorial: Writing a Site Extension (Q:time:tag)
===============================================

This tutorial walks through building a site extension from nothing up to
the ``timetag`` extension as it exists in ``site/timetag.cpp``.  The goal
isn't just to show the final file, but to show *why* each piece is needed,
in the order the need for it arises.

The feature we're building: ``info(Q:time:tag, "nsec:lsb:N")`` on a record
reserves the lowest ``N`` bits of the record's nanosecond timestamp as a
user-defined tag.  On every PVA ``get``, those bits must be moved out of
``timeStamp.nanoseconds`` and into ``timeStamp.userTag``.

Step 1: The minimal extension skeleton
--------------------------------------

Every file in ``site/*.cpp`` must define one ``registerXxx()`` function in
``pvxs::ioc::site``, named after the file's CamelCase basename.  With
nothing registered yet, the extension does nothing:

.. code-block:: c++

    #include "sitehooks.h"

    namespace pvxs { namespace ioc { namespace site {
    void registerTimetag() {
    }
    }}} // pvxs::ioc::site

``gen_siteregister.py`` finds this function by scanning ``site/*.cpp`` and
wires it into ``siteregister.cpp`` automatically — nothing else needs to
change for the build to pick it up.

Step 2: Hook the per-record GET path
------------------------------------

The thing we need to change — ``timeStamp.nanoseconds`` and
``timeStamp.userTag`` — is filled in by ``IOCSource::get()``.  Despite the
name, this single function is the shared path for *both* PVA gets and
monitor updates — ``singlesource.cpp`` calls it both from the get handler
and from the subscription callback that fires on every record update.  A
post-processor registered here therefore applies uniformly to both, with
no extra wiring required. ``addNodePostProcessor()`` registers a callback
that runs immediately after that, with the chance to override any field:

.. code-block:: c++

    namespace {
    void applyTimeTag(dbCommon* prec, pvxs::Value& node) {
        // not implemented yet
    }
    } // namespace

    namespace pvxs { namespace ioc { namespace site {
    void registerTimetag() {
        addNodePostProcessor(applyTimeTag);
    }
    }}} // pvxs::ioc::site

At this point the callback fires for *every* record on *every* get, so it
needs a cheap way to know two things: (a) does this record have
``Q:time:tag`` at all, and (b) if so, what's its bit-width mask.

Step 3: Where do we get the mask from?
--------------------------------------

``info()`` fields are static database configuration, not something to
parse on every single GET. ``dbCommon*`` is a stable, persistent identifier
for a record's lifetime, so the natural approach is to parse the info field
once and cache the result, keyed by ``dbCommon*``:

.. code-block:: c++

    #include <unordered_map>
    #include <dbCommon.h>

    namespace {
    std::unordered_map<dbCommon*, uint32_t>& nsecMaskCache() {
        static std::unordered_map<dbCommon*, uint32_t> s;
        return s;
    }

    void applyTimeTag(dbCommon* prec, pvxs::Value& node) {
        auto& cache = nsecMaskCache();
        auto it = cache.find(prec);
        if (it == cache.end())
            return;     // no Q:time:tag on this record
        uint32_t nsecMask = it->second;
        // ... apply the mask, see Step 6
    }
    } // namespace

PVXS forbids C++ global constructors/destructors in the library, since
some EPICS targets don't reliably run them and ordering across
translation units is otherwise undefined. ``nsecMaskCache()`` complies by
being a function-local ``static``, lazily constructed on first call
instead of at library load time.

Step 4: Populating the cache at IOC init
----------------------------------------

The cache needs to be built once, after the database is loaded but before
any client can issue a GET. ``addInitHookAfterIocBuilt()`` registers a
callback for exactly that point. To find the info field, scan every record
with ``DBEntry`` (a thin wrapper round ``dbStaticLib``'s entry-iteration
API — see ``dbentry.h``):

.. code-block:: c++

    #include <dbStaticLib.h>
    #include "dbentry.h"

    namespace {
    void onIocBuilt() {
        auto& cache = nsecMaskCache();
        pvxs::ioc::DBEntry ent;
        for (long s = dbFirstRecordType(ent); !s; s = dbNextRecordType(ent)) {
            for (s = dbFirstRecord(ent); !s; s = dbNextRecord(ent)) {
                auto* prec = static_cast<dbCommon*>(ent->precnode->precord);
                const char* val = ent.info("Q:time:tag");
                if (!val)
                    continue;
                // ... parse val and insert into cache, see Step 5
            }
        }
    }
    } // namespace

    namespace pvxs { namespace ioc { namespace site {
    void registerTimetag() {
        addInitHookAfterIocBuilt(onIocBuilt);
        addNodePostProcessor(applyTimeTag);
    }
    }}} // pvxs::ioc::site

Step 5: Parsing "nsec:lsb:N" robustly
-------------------------------------

The info string has one recognised form: ``"nsec:lsb:<N>"`` where ``N`` is
a bit-width from 1 to 32. Anything else — a typo, an unsupported keyword, a
width out of range — should be ignored rather than crash the IOC or
silently produce a nonsensical mask. ``epicsParseInt32`` parses the digits
after the prefix and reports failure cleanly:

.. code-block:: c++

    #include <cstring>
    #include <epicsStdlib.h>
    #include <epicsTypes.h>

    void onIocBuilt() {
        // ... (inside the inner loop from Step 4)
        const char* val = ent.info("Q:time:tag");
        if (!val || strncmp(val, "nsec:lsb:", 9) != 0)
            continue;
        epicsInt32 dig = 0;
        if (epicsParseInt32(val + 9, &dig, 10, nullptr) || dig < 1 || dig > 32)
            continue;
        cache[prec] = (uint32_t(1u) << dig) - 1u;
    }

This is exercised by ``testInvalidTag`` in ``site/test/testtimetag.cpp``
(more on tests at Step 8), which checks that a malformed ``Q:time:tag``
value leaves the record's timestamp completely unaffected rather than
crashing or misbehaving.

Step 6: Applying the mask
-------------------------

With the mask available, the post-processor from Step 2 can now do its
job: clear the tag bits from ``nanoseconds`` and expose them as
``userTag``:

.. code-block:: c++

    void applyTimeTag(dbCommon* prec, pvxs::Value& node) {
        auto& cache = nsecMaskCache();
        auto it = cache.find(prec);
        if (it == cache.end())
            return;
        uint32_t nsecMask = it->second;
        uint32_t nsec = prec->time.nsec;
        node["timeStamp.nanoseconds"] = int32_t(nsec & ~nsecMask);
        node["timeStamp.userTag"]     = int32_t(nsec &  nsecMask);
    }

Note that this unconditionally overwrites ``timeStamp.userTag``, even if
``IOCSource::get()`` already populated it from ``prec->utag`` (when
``DBR_UTAG`` is available). That's intentional: a record using
``Q:time:tag`` is using the nsec bits *as* its user tag, so those bits
should always take precedence. See the discussion of this interaction in
the comment at the top of ``timetag.cpp``.

Step 7: Handling repeated IOC init in the same process
------------------------------------------------------

Unit tests build, tear down, and rebuild a ``TestIOC`` repeatedly within
one process (``site/test/testtimetag.cpp`` runs several scenarios this
way). If the cache from a previous IOC instance isn't cleared, the next
``onIocBuilt()`` would still see stale ``dbCommon*`` keys pointing at freed
records. ``addInitHookAtBeginning()`` fires before the database is even
loaded, making it the right place to reset the cache:

.. code-block:: c++

    namespace {
    void onBeginning() { nsecMaskCache().clear(); }
    } // namespace

    namespace pvxs { namespace ioc { namespace site {
    void registerTimetag() {
        addInitHookAtBeginning(onBeginning);
        addInitHookAfterIocBuilt(onIocBuilt);
        addNodePostProcessor(applyTimeTag);
    }
    }}} // pvxs::ioc::site

Step 8: Adding tests
--------------------

Tests for a site extension live alongside it, in ``site/test/``, not in
``test/``. Add a ``site/test/testtimetag.cpp`` and a matching
``site/test/testtimetag.db`` with the records needed to exercise it (one
per scenario: valid tag, no tag, invalid tag, different width). No
``Makefile`` changes are needed: ``site/test/Makefile`` wildcards
``site/*.cpp`` and auto-adds a test binary (plus its ``.db`` fixture) for
any ``<name>.cpp`` that has a matching ``test<name>.cpp`` here — naming it
to match ``timetag.cpp`` (i.e. ``testtimetag.cpp``) is enough for it to
be picked up.

Step 9: The complete extension
------------------------------

Putting Steps 3 through 8 together produces the current
``site/timetag.cpp``:

.. code-block:: c++

    #include <cstring>
    #include <unordered_map>

    #include <dbCommon.h>
    #include <dbStaticLib.h>
    #include <epicsStdlib.h>
    #include <epicsTypes.h>

    #include "dbentry.h"
    #include "sitehooks.h"

    namespace {

    std::unordered_map<dbCommon*, uint32_t>& nsecMaskCache() {
        static std::unordered_map<dbCommon*, uint32_t> s;
        return s;
    }

    void onBeginning() { nsecMaskCache().clear(); }

    void onIocBuilt()
    {
        auto& cache = nsecMaskCache();
        pvxs::ioc::DBEntry ent;
        for (long s = dbFirstRecordType(ent); !s; s = dbNextRecordType(ent)) {
            for (s = dbFirstRecord(ent); !s; s = dbNextRecord(ent)) {
                auto* prec = static_cast<dbCommon*>(ent->precnode->precord);
                const char* val = ent.info("Q:time:tag");
                if (!val || strncmp(val, "nsec:lsb:", 9) != 0)
                    continue;
                epicsInt32 dig = 0;
                if (epicsParseInt32(val + 9, &dig, 10, nullptr) || dig < 1 || dig > 32)
                    continue;
                cache[prec] = (uint32_t(1u) << dig) - 1u;
            }
        }
    }

    void applyTimeTag(dbCommon* prec, pvxs::Value& node)
    {
        auto& cache = nsecMaskCache();
        auto it = cache.find(prec);
        if (it == cache.end())
            return;
        uint32_t nsecMask = it->second;
        uint32_t nsec = prec->time.nsec;
        node["timeStamp.nanoseconds"] = int32_t(nsec & ~nsecMask);
        node["timeStamp.userTag"]     = int32_t(nsec &  nsecMask);
    }

    } // namespace

    namespace pvxs { namespace ioc { namespace site {
    void registerTimetag() {
        addInitHookAtBeginning(onBeginning);
        addInitHookAfterIocBuilt(onIocBuilt);
        addNodePostProcessor(applyTimeTag);
    }
    }}} // pvxs::ioc::site

Because the post-processor hangs off ``IOCSource::get()`` rather than off
the get-handler specifically, it requires no special-casing for monitors:
``testTimeTagGet`` exercises the get path and ``testTimeTagMonitor``
exercises the same masking logic via a subscription, and both pass through
exactly the same ``applyTimeTag`` call.

See ``site/test/testtimetag.cpp`` for the corresponding test suite, and
:ref:`site` for the extension API reference (``addInitHookAtBeginning``,
``addInitHookAfterIocBuilt``, ``addNodePostProcessor``).
