
#ifdef __GNUC__
  #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
    // boost is full of these, gcc 4.8 treats them as warnings.
    #pragma GCC diagnostic ignored "-Wunused-local-typedefs"
  #endif
#endif

#include <boost/python.hpp>

/*
 * This header contains all boost.python constructs missing in
 * older versions of boost.
 *
 * We'll eventually not compile these if the version of boost
 * is sufficiently recent.
 */

inline ssize_t py_len(boost::python::object const& obj)
{
    ssize_t result = PyObject_Length(obj.ptr());
    if (PyErr_Occurred()) boost::python::throw_error_already_set();
    return result;
}

inline boost::python::object py_import(boost::python::str name)
{
  char * n = boost::python::extract<char *>(name);
  boost::python::handle<> module(PyImport_ImportModule(n));
  return boost::python::object(module);
}

inline bool py_hasattr(boost::python::object obj, std::string const &attrName) {
    // Const cast is necessary because python 2.4 (RHEL5) marks the second arg
    // as a (char *), even though it is not modified.
    return PyObject_HasAttrString(obj.ptr(), const_cast<char*>(attrName.c_str()));
}

#define THROW_EX(exception, message) \
    { \
        PyErr_SetString(PyExc_ ##exception, message); \
        boost::python::throw_error_already_set(); \
    }

#define THROW_ERRNO(exception) \
    { \
        PyErr_SetFromErrno(PyExc_ ##exception); \
        boost::python::throw_error_already_set(); \
    }

