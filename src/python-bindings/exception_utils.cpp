#include "python_bindings_common.h"
#include "exception_utils.h"

#include <string>

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name,
  PyObject * base,
  const char * docstring ) {
	// Create the new exception type.
	// I decline to believe that Python /actually/ writes to qualifiedName.
	PyObject * exception = PyErr_NewExceptionWithDoc(
		const_cast<char *>(qualifiedName),
		const_cast<char *>(docstring),
		base, NULL );
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
  PyObject * base1, PyObject * base2,
  const char * docstring ) {
    PyObject * tuple = PyTuple_Pack( 2, base1, base2 );
    PyObject * e = CreateExceptionInModule( qualifiedName, name, tuple, docstring );
    Py_XDECREF(tuple);
    return e;
}

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name,
  PyObject * base1, PyObject * base2, PyObject * base3,
  const char * docstring ) {
    PyObject * tuple = PyTuple_Pack( 3, base1, base2, base3 );
    PyObject * e = CreateExceptionInModule( qualifiedName, name, tuple, docstring );
    Py_XDECREF(tuple);
    return e;
}

PyObject *
CreateExceptionInModule( const char * qualifiedName, const char * name,
  PyObject * base1, PyObject * base2, PyObject * base3, PyObject * base4,
  const char * docstring ) {
    PyObject * tuple = PyTuple_Pack( 4, base1, base2, base3, base4 );
    PyObject * e = CreateExceptionInModule( qualifiedName, name, tuple, docstring );
    Py_XDECREF(tuple);
    return e;
}
