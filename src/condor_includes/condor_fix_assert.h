/*
Get rid of __GNUC__, otherwise gcc will assume its own library when
generating code for asserts.  This will complicate linking of
non gcc programs (FORTRAN) - especially at sites where gcc isn't
installed!
*/

#if !defined(LINUX)

#if defined(__GNUC__)
#	define CONDOR_HAD_GNUC __GNUC__
#	undef __GNUC__
#endif

#endif /* LINUX */

#include <assert.h>

#if defined(CONDOR_HAD_GNUC)
#	define __GNUC__ CONDOR_HAD_GNUC
#endif


