#include "python_bindings_common.h"

#include <condor_adtypes.h>
#include <daemon_types.h>

using namespace boost::python;

void export_daemon_and_ad_types()
{
    enum_<daemon_t>("DaemonTypes",
            R"C0ND0R(
            An enumeration of different types of daemons available to HTCondor.

            The values of the enumeration are:

            .. attribute:: Any

                Any type of daemon; useful when specifying queries where all matching
                daemons should be returned.

            .. attribute:: Master

                Ads representing the *condor_master*.

            .. attribute:: Schedd

                Ads representing the *condor_schedd*.

            .. attribute:: Startd

                Ads representing the resources on a worker node.

            .. attribute:: Collector

                Ads representing the *condor_collector*.

            .. attribute:: Negotiator

                Ads representing the *condor_negotiator*.

            .. attribute:: HAD

                Ads representing the high-availability daemons (*condor_had*).

            .. attribute:: Generic

                All other ads that are not categorized as above.

            .. attribute:: Credd
            )C0ND0R")
        .value("None", DT_NONE)
        .value("Any", DT_ANY)
        .value("Master", DT_MASTER)
        .value("Schedd", DT_SCHEDD)
        .value("Startd", DT_STARTD)
        .value("Collector", DT_COLLECTOR)
        .value("Negotiator", DT_NEGOTIATOR)
        .value("HAD", DT_HAD)
        .value("Generic", DT_GENERIC)
        .value("Credd", DT_CREDD)
        ;

    enum_<AdTypes>("AdTypes",
            R"C0ND0R(
            A list of different types of ads that may be kept in the *condor_collector*.

            The values of the enumeration are:

            .. attribute:: Any

                Type representing any matching ad.  Useful for queries that match everything
                in the collector.

            .. attribute:: Generic

                Generic ads, associated with no particular daemon.

            .. attribute:: Slot

                Slot ads produced by the *condor_startd* daemon.  Represents the
                available slots managed by the startd.

            .. attribute:: StartDaemon

                Daemon ads produced by the *condor_startd* daemon.  There is only a single
                daemon ad for each STARTD.  Daemon ads are used for monitoring and location
                requests, but not for running jobs. 

            .. attribute:: Startd

                Ads produced by the *condor_startd* daemon.  Usually represents the
                available slots managed by the startd, but may indicate STARTD daemon ads.
                Use Slot or StartDaemon enum values to be explicit about which type of ads.

            .. attribute:: StartdPrivate

                The "private" ads, containing the claim IDs associated with a particular
                slot.  These require additional authorization to read as the claim ID
                may be used to run jobs on the slot.

            .. attribute:: Schedd

                Schedd ads, produced by the *condor_schedd* daemon.

            .. attribute:: Master

                Master ads, produced by the *condor_master* daemon.

            .. attribute:: Collector

                Ads from the *condor_collector* daemon.

            .. attribute:: Negotiator

                Negotiator ads, produced by the *condor_negotiator* daemon.

            .. attribute:: Submitter

                Ads describing the submitters with available jobs to run; produced by
                the *condor_schedd* and read by the *condor_negotiator* to determine
                which users need a new negotiation cycle.

            .. attribute:: Grid

                Ads associated with the grid universe.

            .. attribute:: HAD

                Ads produced by the *condor_had*.

            .. attribute:: License

                License ads. These do not appear to be used by any modern HTCondor daemon.

            .. attribute:: Credd
            .. attribute:: Defrag
            .. attribute:: Accounting
            )C0ND0R")
        .value("None", NO_AD)
        .value("Any", ANY_AD)
        .value("Generic", GENERIC_AD)
        .value("Slot", SLOT_AD) // Explicitly a Startd slot ad, currently maps to "Machine" instead of "Slot" for backward compat
        .value("StartDaemon", STARTDAEMON_AD) // Explicitly a Startd daemon ad
        .value("Startd", STARTD_AD) // legacy STARTD ad, may be "Machine", "Slot", or STARTDAEMON_AD depending on context
        .value("StartdPrivate", STARTD_PVT_AD)
        .value("Schedd", SCHEDD_AD)
        .value("Master", MASTER_AD)
        .value("Collector", COLLECTOR_AD)
        .value("Negotiator", NEGOTIATOR_AD)
        .value("Submitter", SUBMITTOR_AD)
        .value("Grid", GRID_AD)
        .value("HAD", HAD_AD)
        .value("License", LICENSE_AD)
        .value("Credd", CREDD_AD)
        .value("Defrag", DEFRAG_AD)
        .value("Accounting", ACCOUNTING_AD)
        ;
}
