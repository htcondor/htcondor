#ifndef   _PYTHON_BINDINGS_EXCEPTION_UTILS_H
#define   _PYTHON_BINDINGS_EXCEPTION_UTILS_H

PyObject * CreateExceptionInModule(
    const char * qualifiedName, const char * name, boost::python::scope & s,
    PyObject * base = NULL,
    const char * docstring = NULL );

PyObject * CreateExceptionInModule(
    const char * qualifiedName, const char * name, boost::python::scope & s,
    PyObject * base1, PyObject * base2,
    const char * docstring = NULL );

PyObject * CreateExceptionInModule(
    const char * qualifiedName, const char * name, boost::python::scope & s,
    PyObject * base1, PyObject * base2, PyObject * base3,
    const char * docstring = NULL );

PyObject * CreateExceptionInModule(
    const char * qualifiedName, const char * name, boost::python::scope & s,
    PyObject * base1, PyObject * base2, PyObject * base3, PyObject * base4,
    const char * docstring = NULL );

#endif /* _PYTHON_BINDINGS_EXCEPTION_UTILS_H */
