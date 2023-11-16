#include "condor_python.h"

// For module initialization.
#include "condor_config.h"
#include "common2/py_handle.cpp"

// classad.ClassAd
#include "classad/classadCache.h"
#include "common2/py_util.cpp"


static PyMethodDef classad2_impl_methods[] = {
	{NULL, NULL, 0, NULL}
};


static struct PyModuleDef classad2_impl_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "classad22_impl",
	.m_doc = NULL, /* no module documentation */
	.m_size = -1, /* this module has global state */
	.m_methods = classad2_impl_methods,

	// In C99, we could just leave these off.
	.m_slots = NULL,
	.m_traverse = NULL,
	.m_clear = NULL,
	.m_free = NULL,
};


PyMODINIT_FUNC
PyInit_classad2_impl(void) {
	// Initialization for HTCondor.  *sigh*
	config();

	// Control HTCondor's stderr verbosity with _CONDOR_TOOL_DEBUG.
	dprintf_set_tool_debug( "TOOL", 0 );

	PyObject * the_module = PyModule_Create(& classad2_impl_module);

	DynamicPyType_Handle dpt_handle("classad2_impl._handle");
	PyObject * pt_handle_object = PyType_FromSpec(& dpt_handle.type_spec);
	Py_INCREF(pt_handle_object);
	PyModule_AddObject(the_module, "_handle", pt_handle_object);

	return the_module;
}
