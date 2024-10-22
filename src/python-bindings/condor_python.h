//
// On Linux, we can just include "condor_common.h", define Py_LIMITED_API,
// and then include "Python.h", and everything Just Works(TM).
//
// On Windows and FreeBSD, everything is terrible.
//

#define _CONDOR_COMMON_FIRST

// Cargo-culted over from python_bindings_common.h in version 1.
#if defined(_MSC_VER)
    #define _CRT_SECURE_NO_DEPRECATE 1
    #define _CRT_NONSTDC_NO_DEPRECATE 1

    #include <crtdefs.h>

    #pragma push_macro("PLATFORM")
    #undef PLATFORM

    #pragma push_macro("_DEBUG")
    #undef _DEBUG

    #undef _CONDOR_COMMON_FIRST
#endif /* _MSC_VER */

// Cargo-culted over from python_bindings_common.h in version 1.
#if defined(__FreeBSD__)
    #define profil _hide_profil
    #define dprintf _hide_dprintf

    #undef _CONDOR_COMMON_FIRST
#endif /* __FreeBSD__ */


#if defined(_CONDOR_COMMON_FIRST)
    #include "condor_common.h"
#endif /* _CONDOR_COMMON_FIRST */

// By defining Py_LIMITED_API, we ensure that we see only the symbols that are
// part of the "limited API", which is a strict subset of the "stable ABI", which is in
// turn guaranteed to be compatible between all minor versions of Python 3 after
// and including 3.2.
//
// The version 2 bindings don't need any part of the limited API introduced after
// Python 3.2, so we can define Py_LIMITED_API as 3 (rather than 0x03020000,
// which is the same but makes it look like we really want version 3.2).
//
// See https://docs.python.org/3/c-api/stable.html#stable-application-binary-interface.
//
// This is stupid and broken.  You MUST define this in order to be able to use
// core Python modules in 3.10, 3.11, and 3.22 (specifically, subprocess),
// but if you do so, you MUST define Py_LIMITED_API to be 3.3 or later --
// the 3.2 libraries didn't include the the ssize_t-clean versions of all
// functions, so for "backwards-compatibility", those functions aren't present
// in later versions when PY_SSIZE_T_CLEAN is defined by default.
#define Py_LIMITED_API 0x30300000
#define PY_SSIZE_T_CLEAN
#include <Python.h>


// Cargo-culted over from python_bindings_common.h in version 1.
#if defined(_MSC_VER)
    #pragma pop_macro("_DEBUG")
    #pragma pop_macro("PLATFORM")

    #undef snprintf
    #undef vsnprintf
    #undef S_ISDIR
    #undef S_ISREG

    #include "condor_common.h"
#endif /* _MSC_VER */

// Cargo-culted over from python_bindings_common.h in version 1.
#if defined(__FreeBSD__)
    #undef profil
    #undef dprintf

    #include "condor_common.h"
#endif /* __FreeBSD__ */

