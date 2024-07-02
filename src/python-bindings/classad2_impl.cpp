#include "condor_python.h"

// For module initialization.
#include "condor_config.h"
#include "common2/py_handle.cpp"

// clasas.ClassAdException
PyObject * PyExc_ClassAdException = NULL;

// classad.*
#include "classad/classad.h"
#include "common2/py_util.cpp"
#include "classad2/loose_functions.cpp"

// classad.ClassAd
#include "classad/classadCache.h"
#include "classad2/classad.cpp"
#include "classad2/exprtree.cpp"


static PyMethodDef classad2_impl_methods[] = {
    {"_classad_init", & _classad_init, METH_VARARGS, NULL},
    {"_classad_init_from_dict", & _classad_init_from_dict, METH_VARARGS, NULL},
    {"_classad_init_from_string", & _classad_init_from_string, METH_VARARGS, NULL},
    {"_classad_to_repr", & _classad_to_repr, METH_VARARGS, NULL},
    {"_classad_to_string", & _classad_to_string, METH_VARARGS, NULL},
    {"_classad_get_item", & _classad_get_item, METH_VARARGS, NULL},
    {"_classad_set_item", & _classad_set_item, METH_VARARGS, NULL},
    {"_classad_del_item", & _classad_del_item, METH_VARARGS, NULL},
    {"_classad_size", & _classad_size, METH_VARARGS, NULL},
    {"_classad_keys", & _classad_keys, METH_VARARGS, NULL},
    {"_classad_parse_next", & _classad_parse_next, METH_VARARGS, NULL},
    {"_classad_parse_next_fd", & _classad_parse_next_fd, METH_VARARGS, NULL},
    {"_classad_quote", & _classad_quote, METH_VARARGS, NULL},
    {"_classad_unquote", & _classad_unquote, METH_VARARGS, NULL},
    {"_classad_flatten", & _classad_flatten, METH_VARARGS, NULL},
    {"_classad_external_refs", &_classad_external_refs, METH_VARARGS, NULL},
    {"_classad_internal_refs", &_classad_internal_refs, METH_VARARGS, NULL},
    {"_classad_print_json", &_classad_print_json, METH_VARARGS, NULL},
    {"_classad_print_old", &_classad_print_old, METH_VARARGS, NULL},
    {"_classad_last_error", &_classad_last_error, METH_VARARGS, NULL},
    {"_classad_version", &_classad_version, METH_VARARGS, NULL},

    {"_exprtree_init", & _exprtree_init, METH_VARARGS, NULL},
    {"_exprtree_eq", & _exprtree_eq, METH_VARARGS, NULL},
    {"_exprtree_eval", & _exprtree_eval, METH_VARARGS, NULL},
    {"_exprtree_simplify", & _exprtree_simplify, METH_VARARGS, NULL},

	{NULL, NULL, 0, NULL}
};


static struct PyModuleDef classad2_impl_module = {
	.m_base = PyModuleDef_HEAD_INIT,
	.m_name = "classad2_impl",
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
	PyObject * the_module = PyModule_Create(& classad2_impl_module);

	DynamicPyType_Handle dpt_handle("classad2_impl._handle");
	PyObject * pt_handle_object = PyType_FromSpec(& dpt_handle.type_spec);
	Py_INCREF(pt_handle_object);
	PyModule_AddObject(the_module, "_handle", pt_handle_object);

    // Create the new exception type(s).
    PyExc_ClassAdException = PyErr_NewExceptionWithDoc(
        "classad2_impl.ClassAdException",
        "... the doc string ...",
        NULL, NULL
    );
    PyModule_AddObject(the_module, "ClassAdException", PyExc_ClassAdException);

	return the_module;
}
