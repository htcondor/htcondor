
#ifdef _MSC_VER // Windows (actually any MS compiler)

#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NONSTDC_NO_DEPRECATE 1
#include <crtdefs.h> // make sure that crtdefs is loaded with global meaning of _DEBUG

// pyconfig.h changes the definition of the PyObject structure if _DEBUG is defined!!!
// they also generate references to pythonNNN_d.lib library instead of pythonNNN.lib
// but they don't actually ship a pythonNNN_d.lib, so we can't afford to let pyconfig.h
// know we have _DEBUG defined.
#pragma push_macro("_DEBUG")
#undef _DEBUG
#endif

// pyconfig.h defines PLATFORM in a way that is incompatibable with HTCondor, (theirs is "win32")
// so push end undef the macro before, and restore our definition after we call pyconfig.h
#pragma push_macro("PLATFORM")
#undef PLATFORM


// Note - pyconfig.h must be included before condor_common to avoid
// re-definition warnings.
#include <pyconfig.h>

#ifdef _MSC_VER //
#pragma pop_macro("PLATFORM")
#pragma pop_macro("_DEBUG")
#endif
