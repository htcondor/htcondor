/*
Get rid of __GNUC__, otherwise gcc will assume its own library when
generating code for asserts.  This will complicate linking of
non gcc programs (FORTRAN) - especially at sites where gcc isn't
installed!
*/

#if defined(__GNUC__)
#	define __WAS_GNUC__
#	undef __GNUC__
#endif

#include <assert.h>

#if defined(__WAS_GNUC__)
#	define __GNUC__
#endif
