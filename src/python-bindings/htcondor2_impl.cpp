#include "condor_python.h"

// We include some .cpp files, which is an awful hack, but I think less
// ugly, overall, than having or avoiding all of the defined-but-not-used
// warnings the compiler would otherwise generate.

// For module initialization.
#include "condor_config.h"
#include "common2/py_handle.cpp"

// htcondor.*
#include "condor_version.h"
#include "subsystem_info.h"
#include "daemon.h"
#include "common2/py_util.cpp"
#include "htcondor2/loose_functions.cpp"

// htcondor.Collector
#include "daemon_list.h"
#include "dc_collector.h"
#include "htcondor2/collector.cpp"

// htcondor.Negotiator
#include "htcondor2/negotiator.cpp"

// htcondor.Startd
#include "dc_startd.h"
#include "htcondor2/startd.cpp"

// htcondor.Credd
#include "store_cred.h"
#include "my_username.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "htcondor2/credd.cpp"

// htcondor.Submit
#include "submit_utils.h"
#include "dagman_utils.h"
#include "MapFile.h"
#include "htcondor2/submit.cpp"

// htcondor.Schedd
#include "condor_q.h"
#include "dc_schedd.h"
#include "condor_qmgr.h"
#include "condor_attributes.h"
#include "htcondor2/schedd.cpp"

// htcondor.JobEventLog
#include "read_user_log.h"
#include "file_modified_trigger.h"
#include "wait_for_user_log.h"
#include "htcondor2/job_event_log.cpp"

// htcondor._Param
#include "param_info.h"
#include "htcondor2/param.cpp"

// htcondor.RemoteParam
#include "htcondor2/remote_param.cpp"


static PyMethodDef htcondor2_impl_methods[] = {
	{"_version", & _version, METH_VARARGS, R"C0ND0R(
	    Returns the version of HTCondor this module is linked against.
	)C0ND0R"},

	{"_platform", & _platform, METH_VARARGS, R"C0ND0R(
	    Returns the platform of HTCondor this module was compiled for.
	)C0ND0R"},

	{"_enable_debug", & _enable_debug, METH_VARARGS, R"C0ND0R(
	    Enable debugging output from HTCondor, where output is sent to
	    ``stderr``.  The logging level is set by the ``TOOL_DEBUG``
	    parameter.
	)C0ND0R"},

	{"_enable_log", & _enable_log, METH_VARARGS, R"C0ND0R(
	    Enable debugging output from HTCondor, where output is sent to
	    a file. The logging level is set by the ``TOOL_DEBUG``
	    parameter, and the file by ``TOOL_LOG``.
	)C0ND0R"},

	{"_set_subsystem", & _set_subsystem, METH_VARARGS, R"C0ND0R(
	    Set the subsystem name for the object.

	    The subsystem is primarily used for the parsing of the HTCondor configuration file.

	    :param str name: The subsystem name.
	    :param daemon_type: The HTCondor daemon type. The default value of :attr:`SubsystemType.Auto` infers the type from the name parameter.
	    :type daemon_type: :class:`SubsystemType`
	)C0ND0R"},

	{"_reload_config", & _reload_config, METH_VARARGS, R"C0ND0R(
	    Reload the HTCondor configuration from disk.
	)C0ND0R"},

	{"_send_command", & _send_command, METH_VARARGS, NULL},
	{"_send_alive", & _send_alive, METH_VARARGS, NULL},

	{"_dprintf_dfulldebug", &_dprintf_dfulldebug, METH_VARARGS, NULL},

	{"_py_dprintf", &_py_dprintf, METH_VARARGS, NULL},


	{"_collector_init", &_collector_init, METH_VARARGS, NULL},
	{"_collector_query", &_collector_query, METH_VARARGS, NULL},
	{"_collector_locate_local", & _collector_locate_local, METH_VARARGS, NULL},
	{"_collector_advertise", & _collector_advertise, METH_VARARGS, NULL},


	{"_negotiator_command", &_negotiator_command, METH_VARARGS, NULL},
	{"_negotiator_command_return", &_negotiator_command_return, METH_VARARGS, NULL},
	{"_negotiator_command_user", &_negotiator_command_user, METH_VARARGS, NULL},
	{"_negotiator_command_user_return", &_negotiator_command_user_return, METH_VARARGS, NULL},
	{"_negotiator_command_user_value", &_negotiator_command_user_value, METH_VARARGS, NULL},


	{"_startd_drain_jobs", &_startd_drain_jobs, METH_VARARGS, NULL},
	{"_startd_cancel_drain_jobs", &_startd_cancel_drain_jobs, METH_VARARGS, NULL},


	{"_credd_do_store_cred", &_credd_do_store_cred, METH_VARARGS, NULL},
	{"_credd_do_check_oauth_creds", &_credd_do_check_oauth_creds, METH_VARARGS, NULL},
	{"_credd_run_credential_producer", &_credd_run_credential_producer, METH_VARARGS, NULL},


	{"_schedd_query", &_schedd_query, METH_VARARGS, NULL},
	{"_schedd_act_on_job_ids", &_schedd_act_on_job_ids, METH_VARARGS, NULL},
	{"_schedd_act_on_job_constraint", &_schedd_act_on_job_constraint, METH_VARARGS, NULL},
	{"_schedd_edit_job_ids", &_schedd_edit_job_ids, METH_VARARGS, NULL},
	{"_schedd_edit_job_constraint", &_schedd_edit_job_constraint, METH_VARARGS, NULL},
	{"_schedd_reschedule", &_schedd_reschedule, METH_VARARGS, NULL},
	{"_schedd_export_job_ids", &_schedd_export_job_ids, METH_VARARGS, NULL},
	{"_schedd_export_job_constraint", &_schedd_export_job_constraint, METH_VARARGS, NULL},
	{"_schedd_import_exported_job_results", &_schedd_import_exported_job_results, METH_VARARGS, NULL},
	{"_schedd_unexport_job_ids", &_schedd_unexport_job_ids, METH_VARARGS, NULL},
	{"_schedd_unexport_job_constraint", &_schedd_unexport_job_constraint, METH_VARARGS, NULL},
	{"_schedd_retrieve_job_ids", &_schedd_retrieve_job_ids, METH_VARARGS, NULL},
	{"_schedd_retrieve_job_constraint", &_schedd_retrieve_job_constraint, METH_VARARGS, NULL},
	{"_schedd_spool", &_schedd_spool, METH_VARARGS, NULL},
	{"_schedd_submit", &_schedd_submit, METH_VARARGS, NULL},


	{"_submit_init", &_submit_init, METH_VARARGS, NULL},
	{"_submit__getitem__", &_submit__getitem__, METH_VARARGS, NULL},
	{"_submit__setitem__", &_submit__setitem__, METH_VARARGS, NULL},
	{"_submit_keys", &_submit_keys, METH_VARARGS, NULL},
	{"_submit_expand", &_submit_expand, METH_VARARGS, NULL},
	{"_submit_getqargs", &_submit_getqargs, METH_VARARGS, NULL},
	{"_submit_setqargs", &_submit_setqargs, METH_VARARGS, NULL},
	{"_submit_from_dag", &_submit_from_dag, METH_VARARGS, NULL},
	{"_display_dag_options", &_display_dag_options, METH_VARARGS, NULL},
	{"_submit_set_submit_method", &_submit_set_submit_method, METH_VARARGS, NULL},
	{"_submit_get_submit_method", &_submit_get_submit_method, METH_VARARGS, NULL},


	{"_job_event_log_init", &_job_event_log_init, METH_VARARGS, NULL},
	{"_job_event_log_next", &_job_event_log_next, METH_VARARGS, NULL},
	{"_job_event_log_close", &_job_event_log_close, METH_VARARGS, NULL},
	{"_job_event_log_get_offset", &_job_event_log_get_offset, METH_VARARGS, NULL},
	{"_job_event_log_set_offset", &_job_event_log_set_offset, METH_VARARGS, NULL},


	{"_param__getitem__", &_param__getitem__, METH_VARARGS, NULL},
	{"_param__setitem__", &_param__setitem__, METH_VARARGS, NULL},
	{"_param__delitem__", &_param__delitem__, METH_VARARGS, NULL},
	{"_param_keys", &_param_keys, METH_VARARGS, NULL},


	{"_remote_param_keys", &_remote_param_keys, METH_VARARGS, NULL},
	{"_remote_param_get", &_remote_param_get, METH_VARARGS, NULL},
	{"_remote_param_set", &_remote_param_set, METH_VARARGS, NULL},


	{"_history_query", &_history_query, METH_VARARGS, NULL},


	{NULL, NULL, 0, NULL}
};


static struct PyModuleDef htcondor2_impl_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "htcondor2_impl",
	.m_doc = NULL, /* no module documentation */
	.m_size = -1, /* this module has global state */
	.m_methods = htcondor2_impl_methods,

	// In C99, we could just leave these off.
	.m_slots = NULL,
	.m_traverse = NULL,
	.m_clear = NULL,
	.m_free = NULL,
};


PyMODINIT_FUNC
PyInit_htcondor2_impl(void) {
	//
	// This is the default initialization from version 1.
	//

	// [export_config()]
	dprintf_make_thread_safe();
	config_ex(CONFIG_OPT_NO_EXIT | CONFIG_OPT_WANT_META);
	param_insert("ENABLE_CLASSAD_CACHING", "false");
	classad::ClassAdSetExpressionCaching(false);

	// [export_dc_tool()]
	if(! has_mySubSystem()) {
		set_mySubSystem("TOOL", false, SUBSYSTEM_TYPE_TOOL);
	}
	dprintf_pause_buffering();


	PyObject * the_module = PyModule_Create(& htcondor2_impl_module);

	DynamicPyType_Handle dpt_handle("htcondor2_impl._handle");
	PyObject * pt_handle_object = PyType_FromSpec(& dpt_handle.type_spec);
	Py_INCREF(pt_handle_object);
	PyModule_AddObject(the_module, "_handle", pt_handle_object);

	return the_module;
}
