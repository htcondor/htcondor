
#include "python_bindings_common.h"

#include "condor_common.h"
#include "old_boost.h"
#include "export_headers.h"
#include "condor_uid.h"

using namespace boost::python;

BOOST_PYTHON_MODULE(htcondor)
{
    scope().attr("__doc__") = "Utilities for interacting with the HTCondor system.";

    py_import("classad");

    // TODO: old boost doesn't have this; conditionally compile only one newer systems.
    //docstring_options local_docstring_options(true, false, false);

    // Disable all priv switching from Python Bindings, even if the process
    // invoking the bindings has root.  This prevents any possibility of returning
    // to the calling code with a different effective uid than the caller expects.
    set_priv_ignore_all_requests();

    export_config();
    export_daemon_and_ad_types();
    export_collector();
    export_negotiator();
    export_schedd();
    export_dc_tool();
    export_secman();
    export_event_log();
#if !defined(WIN32)
	// omit for windows
    export_log_reader();
#endif
    export_claim();
    export_startd();
    export_query_iterator();

    def("enable_classad_extensions", enable_classad_extensions, "Register the HTCondor-specific extensions to the ClassAd library.");
}
