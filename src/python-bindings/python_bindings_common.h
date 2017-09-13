
#ifdef _MSC_VER // Windows (actually any MS compiler)
 #define _CRT_SECURE_NO_DEPRECATE 1
 #define _CRT_NONSTDC_NO_DEPRECATE 1
 #include <crtdefs.h> // make sure that crtdefs is loaded with global meaning of _DEBUG

 // pyconfig.h defines PLATFORM in a way that is incompatibable with HTCondor
 // so push end undef the macro before, and restore our definition after we call pyconfig.h
 #pragma push_macro("PLATFORM")
 #undef PLATFORM
#else
 #if defined(__APPLE__)
  // undef HAVE_SSIZE_T to force pyport.h to typedef Py_ssize_t from Py_intptr_t instead of ssize_t
  #undef HAVE_SSIZE_T
 #endif // __APPLE__
 //Redefining dprintf and getline prevents a collision with the definitions in stdio.h
 //which python headers will include, and the condor definitions
 #define dprintf _hide_dprintf
 #define profil _hide_profil
 #if defined(__FreeBSD__)
  #define getline _hide_getline
 #endif
#endif

#ifdef __GNUC__
  #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
    // boost is full of these, gcc 4.8 treats them as warnings.
    #pragma GCC diagnostic ignored "-Wunused-local-typedefs"
  #endif
#endif

// pyconfig.h changes the definition of the PyObject structure if _DEBUG is defined!!!
// On windows it also generates references to pythonNNN_d.lib library instead of pythonNNN.lib
// but the python guys don't actually ship a pythonNNN_d.lib,  we avoid this problem by
// including pyconfig.h via boost/python.hpp
//
#include <boost/python.hpp>

#ifdef _MSC_VER // WINDOWS
 #pragma pop_macro("PLATFORM")
 // remove defines set by python headers that conflict in detail
 // but not in substance with the condor_common defines.
 #undef HAVE_SNPRINTF
 #undef snprintf
 #undef vsnprintf
 #undef S_ISDIR
 #undef S_ISREG
#else
 #undef dprintf
 #undef profil
 #if defined(__FreeBSD__)
  #undef getline
 #endif
 // On Debian 7, pyconfig.h sets this macro. Globus assumes it refers
 // to the Windows-only <io.h>.
 #undef HAVE_IO_H
#endif // _MSC_VER

// http://docs.python.org/3/c-api/apiabiversion.html#apiabiversion
// the "next" method for iterators in python3 is "__next__"
// https://docs.python.org/2/library/functions.html#next
// https://docs.python.org/3/library/functions.html#next
#if PY_MAJOR_VERSION >= 3
   #define PyInt_Check(op)  PyNumber_Check(op)
   #define PyString_Check(op)  PyBytes_Check(op)
   #define NEXT_FN "__next__"
#else
   #define NEXT_FN "next"
#endif

