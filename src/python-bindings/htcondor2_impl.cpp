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

	// In C99, we could just leave these off.
	.m_slots = NULL,
	.m_traverse = NULL,
	.m_clear = NULL,
	.m_free = NULL,
};


// We can't statically define Python types from C if Py_LIMITED_API is set,
// which we're not quite ready to give up on yet.  Also, it's a pain to
// define even a single Python type, so the plan was to define that single
// Python type to have a C-side-only void * for whatever nefarious purpose
// the C-side needs, and have the interface types inherit from that type in
// pure Python.

// For now, we're going to try to "dynamically" define that type.  If we
// can't make that work, given that we're declaring a void * here, maybe
// it wouldn't be so bad, or even better, just to declare ints in Python
// that only the C side uses for pointers.


typedef struct {
    PyObject_HEAD
    void * ptr;
} with_one_pointer;


static PyType_Slot with_one_pointer_slots[] = {
    // Optional.
    // {Py_tp_doc, (void *)"private implementation detail"},
    // Optional.  Probably already inherited from Object.
    // {Py_tp_new, (void *)PyType_GenericNew},
    {0, NULL},
};

static PyType_Spec with_one_pointer_spec = {
    .name = "htcondor2_impl.with_one_pointer",
    .basicsize = sizeof(with_one_pointer),
    .itemsize = 0, /* not a variable-sized object */
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_BASETYPE,
    .slots = with_one_pointer_slots,
};


template<class T>
struct DynamicPyType {
    DynamicPyType(const char * module_dot_typename) {
        type_spec.name = module_dot_typename;
    }

    typedef struct {
        PyObject_HEAD
        T t;
    } dynamic_pytype;

    PyType_Spec type_spec = {
        .name = NULL, /* set by constructor */
        .basicsize = sizeof(dynamic_pytype),
        .itemsize = 0, /* not a variable-sized object */
        .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE | Py_TPFLAGS_BASETYPE,
        .slots = dynamic_slots,
    };

    PyType_Slot dynamic_slots[1] = { {0, NULL} };
};


PyMODINIT_FUNC
PyInit_htcondor2_impl(void) {
	// Initialization for HTCondor.  *sigh*
	config();
	dprintf_set_tool_debug( "TOOL", 0 );

	PyObject * the_module = PyModule_Create(& htcondor2_impl_module);

	// Register a "dynamic" type.
	// PyObject * the_type_object = PyType_FromSpec(& with_one_pointer_spec);
	// Py_INCREF(the_type_object);
	// PyModule_AddObject(the_module, "_with_one_pointer", the_type_object);

    DynamicPyType<int> a_py_type("htcondor2_impl._a_py_type");
    PyObject * a_type_object = PyType_FromSpec(& a_py_type.type_spec);
    Py_INCREF(a_type_object);
	PyModule_AddObject(the_module, "_a_py_type", a_type_object);

	return the_module;
}
