
#include "python_bindings_common.h"

#include "old_boost.h"
#include "export_headers.h"

using namespace boost::python;

BOOST_PYTHON_MODULE(_htcondor)
{
    scope().attr("__doc__") = "Utilities for interacting with the HTCondor system.";

    py_import("classad");

    // TODO: old boost doesn't have this; conditionally compile only one newer systems.
    //docstring_options local_docstring_options(true, false, false);

    export_config();
    export_daemon_and_ad_types();
    export_collector();
    export_negotiator();
    export_schedd();
    export_dc_tool();
    export_secman();
    export_event_log();
	// TODO This is the old API for reading the event log.
	//   We should remove it once users have had enough time to
	//   migrate to the new API.
	export_event_reader();
#if !defined(WIN32)
	// omit for windows
    export_log_reader();
#endif
    export_claim();
    export_startd();
    export_query_iterator();

    def("enable_classad_extensions", enable_classad_extensions, "Register the HTCondor-specific extensions to the ClassAd library.");
}
