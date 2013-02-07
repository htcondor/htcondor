
#include <condor_adtypes.h>
#include <daemon_types.h>
#include <boost/python.hpp>

using namespace boost::python;

void export_daemon_and_ad_types()
{
    enum_<daemon_t>("DaemonTypes")
        .value("None", DT_NONE)
        .value("Any", DT_ANY)
        .value("Master", DT_MASTER)
        .value("Schedd", DT_SCHEDD)
        .value("Startd", DT_STARTD)
        .value("Collector", DT_COLLECTOR)
        .value("Negotiator", DT_NEGOTIATOR)
        ;

    enum_<AdTypes>("AdTypes")
        .value("None", NO_AD)
        .value("Any", ANY_AD)
        .value("Generic", GENERIC_AD)
        .value("Startd", STARTD_AD)
        .value("Schedd", SCHEDD_AD)
        .value("Master", MASTER_AD)
        .value("Collector", COLLECTOR_AD)
        .value("Negotiator", NEGOTIATOR_AD)
        ;
}
