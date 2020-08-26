
#include "python_bindings_common.h"

#include "old_boost.h"
#include "export_headers.h"

#include "htcondor.h"
#include "exception_utils.h"

using namespace boost::python;

PyObject * PyExc_HTCondorException = NULL;

PyObject * PyExc_HTCondorEnumError = NULL;
PyObject * PyExc_HTCondorInternalError = NULL;
PyObject * PyExc_HTCondorIOError = NULL;
PyObject * PyExc_HTCondorLocateError = NULL;
PyObject * PyExc_HTCondorReplyErorr = NULL;
PyObject * PyExc_HTCondorValueError = NULL;
PyObject * PyExc_HTCondorTypeError = NULL;
PyObject * PyExc_HTCondorAssertionError = NULL;
PyObject * PyExc_HTCondorNotImplementedError = NULL;

BOOST_PYTHON_MODULE(htcondor)
{
    scope().attr("__doc__") = "Utilities for interacting with the HTCondor system.";

    py_import("classad");

    // TODO: old boost doesn't have this; conditionally compile only one newer systems.
    //docstring_options local_docstring_options(true, false, false);

    export_config();
    export_daemon_and_ad_types();
    export_daemon_location();
    export_collector();
    export_negotiator();
    export_schedd();
    export_credd();
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

    PyExc_HTCondorException = CreateExceptionInModule(
        "htcondor.HTCondorException", "HTCondorException", PyExc_Exception );

    PyExc_HTCondorEnumError = CreateExceptionInModule(
        "htcondor.HTCondorEnumError", "HTCondorEnumError",
        PyExc_HTCondorException );
    PyExc_HTCondorInternalError = CreateExceptionInModule(
        "htcondor.HTCondorInternalError", "HTCondorInternalError",
        PyExc_HTCondorException );
    PyExc_HTCondorIOError = CreateExceptionInModule(
        "htcondor.HTCondorIOError", "HTCondorIOError",
        PyExc_HTCondorException );
    PyExc_HTCondorLocateError = CreateExceptionInModule(
        "htcondor.HTCondorLocateError", "HTCondorLocateError",
        PyExc_HTCondorException );
    PyExc_HTCondorValueError = CreateExceptionInModule(
        "htcondor.HTCondorReplyError", "HTCondorReplyError",
        PyExc_HTCondorException );
    PyExc_HTCondorValueError = CreateExceptionInModule(
        "htcondor.HTCondorValueError", "HTCondorValueError",
        PyExc_HTCondorException );
    PyExc_HTCondorTypeError = CreateExceptionInModule(
        "htcondor.HTCondorTypeError", "HTCondorTypeError",
        PyExc_HTCondorException );
    PyExc_HTCondorAssertionError = CreateExceptionInModule(
        "htcondor.HTCondorAssertionError", "HTCondorAssertionError",
        PyExc_HTCondorException );
    PyExc_HTCondorNotImplementedError = CreateExceptionInModule(
        "htcondor.HTCondorNotImplementedError", "HTCondorNotImplementedError",
        PyExc_HTCondorException );
}
