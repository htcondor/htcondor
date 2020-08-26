#ifndef   _PYTHON_BINDINGS_EXCEPTION_UTILS_H
#define   _PYTHON_BINDINGS_EXCEPTION_UTILS_H

PyObject * CreateExceptionInModule( const char * qualifiedName,
	const char * name, PyObject * base = NULL );

PyObject * CreateExceptionInModule( const char * qualifiedName,
	const char * name, PyObject * base1, PyObject * base2 );
PyObject * CreateExceptionInModule( const char * qualifiedName,
	const char * name, PyObject * base1, PyObject * base2, PyObject * base3 );
PyObject * CreateExceptionInModule( const char * qualifiedName,
	const char * name, PyObject * base1, PyObject * base2, PyObject * base3,
	PyObject * base4 );


#endif /* _PYTHON_BINDINGS_EXCEPTION_UTILS_H */
