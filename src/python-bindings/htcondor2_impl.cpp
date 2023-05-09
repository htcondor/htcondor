#include "condor_common.h"

#define Py_LIMITED_API
#include <Python.h>

#include "condor_config.h"
#include "condor_version.h"
#include "subsystem_info.h"

static PyObject *
_version( PyObject *, PyObject * ) {
	return PyUnicode_FromString( CondorVersion() );
}


static PyObject *
_platform( PyObject *, PyObject * ) {
	return PyUnicode_FromString( CondorPlatform() );
}


static PyObject *
_set_subsystem( PyObject *, PyObject * args ) {
	const char * subsystem_name = NULL;
	PyObject * py_subsystem_type = NULL;

	if(! PyArg_ParseTuple( args, "s|O", & subsystem_name, &py_subsystem_type )) {
		// PyArg_ParseTuple() has already set an exception for us.
		return NULL;
	}


	// If py_subsystem_type is not NULL, set subsystem_type.
	SubsystemType subsystem_type = SUBSYSTEM_TYPE_AUTO;
	if( py_subsystem_type ) {
		// We can't use a C reference to a global here because the
		// `htcondor.SubsystemType` type was defined by Boost.
		PyObject * py_htcondor_module = PyImport_ImportModule( "htcondor" );
		PyObject * py_subsystemtype_class = PyObject_GetAttrString( py_htcondor_module, "SubsystemType" );
		if(! PyObject_IsInstance( py_subsystem_type, py_subsystemtype_class )) {
			// This is technically an API violation; we should raise an
			// instance of HTCondorTypeError, instead.
			PyErr_SetString( PyExc_TypeError, "daemonType must be of type htcondor.SubsystemType" );
			return NULL;
		}

		// SubsystemType is defined to be a long.
		subsystem_type = (SubsystemType)PyLong_AsLong(py_subsystem_type);
		if( PyErr_Occurred() ) {
			return NULL;
		}
	}


	// I have no idea why we assume `false` here.
	set_mySubSystem( subsystem_name, false, subsystem_type );

	// I have no idea why set `m_trusted` here.
	SubsystemInfo * subsystem = get_mySubSystem();
	if( subsystem->isDaemon() ) { subsystem->setIsTrusted( true ); }

	Py_RETURN_NONE;
}


static PyMethodDef htcondor2_impl_methods[] = {
	{"_version", & _version, METH_VARARGS, R"C0ND0R(
        Returns the version of HTCondor this module is linked against.
    )C0ND0R"},

	{"_platform", & _platform, METH_VARARGS, R"C0ND0R(
        Returns the platform of HTCondor this module was compiled for.
    )C0ND0R"},

	{"_set_subsystem", & _set_subsystem, METH_VARARGS, R"C0ND0R(
        Set the subsystem name for the object.

        The subsystem is primarily used for the parsing of the HTCondor configuration file.

        :param str name: The subsystem name.
        :param daemon_type: The HTCondor daemon type. The default value of :attr:`SubsystemType.Auto` infers the type from the name parameter.
        :type daemon_type: :class:`SubsystemType`
    )C0ND0R"},

	{NULL, NULL, 0, NULL}
};


static struct PyModuleDef htcondor2_impl_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "htcondor2_impl",
	.m_doc = NULL, /* no module documentation */
	.m_size = -1, /* this module has global state */
	.m_methods = htcondor2_impl_methods,
	.m_slots = NULL,
	.m_traverse = NULL,
	.m_clear = NULL,
	.m_free = NULL,
};


PyMODINIT_FUNC
PyInit_htcondor2_impl(void) {
	// Initialization for HTCondor.  *sigh*
	config();
	dprintf_set_tool_debug( "TOOL", 0 );

	return PyModule_Create(& htcondor2_impl_module);
}
