#include "python_bindings_common.h"
#include "exception_utils.h"

#include <string>

//
// These can't be classad_module.cpp, where you'd expect, because the htcondor
// module depends on an internal library, not the classad module proper.
//

PyObject * PyExc_ClassAdException = NULL;

PyObject * PyExc_ClassAdEnumError = NULL;
PyObject * PyExc_ClassAdEvaluationError = NULL;
PyObject * PyExc_ClassAdInternalError = NULL;
PyObject * PyExc_ClassAdParseError = NULL;
PyObject * PyExc_ClassAdValueError = NULL;
PyObject * PyExc_ClassAdTypeError = NULL;
PyObject * PyExc_ClassAdOSError = NULL;

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name, PyObject * base ) {
	// Create the new exception type.
	// I decline to believe that Python /actually/ writes to qualifiedName.
	PyObject * exception = PyErr_NewException(
		const_cast<char *>(qualifiedName), base, NULL );
	if(! exception) {
		boost::python::throw_error_already_set();
	}

    // Adds the exception to the module, which appears to the convention:
    boost::python::scope().attr( name ) =
        boost::python::handle<>( boost::python::borrowed( exception ) );

	return exception;
}

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name,
  PyObject * base1, PyObject * base2 ) {
    PyObject * tuple = PyTuple_Pack( 2, base1, base2 );
    PyObject * e = CreateExceptionInModule( qualifiedName, name, tuple );
    Py_XDECREF(tuple);
    return e;
}

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name,
  PyObject * base1, PyObject * base2, PyObject * base3 ) {
    PyObject * tuple = PyTuple_Pack( 3, base1, base2, base3 );
    PyObject * e = CreateExceptionInModule( qualifiedName, name, tuple );
    Py_XDECREF(tuple);
    return e;
}

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name,
  PyObject * base1, PyObject * base2, PyObject * base3, PyObject * base4 ) {
    PyObject * tuple = PyTuple_Pack( 4, base1, base2, base3, base4 );
    PyObject * e = CreateExceptionInModule( qualifiedName, name, tuple );
    Py_XDECREF(tuple);
    return e;
}
