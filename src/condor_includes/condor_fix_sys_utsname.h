
#ifndef _CONDOR_FIX_SYS_UTSNAME_H
#define _CONDOR_FIX_SYS_UTSNAME_H

#include "condor_header_features.h"

#if defined(CONDOR_KILLS_UNAME_DEFINITIONS)
	#define uname(a) __condor_hack_uname(a)
	#define _uname(a) __condor_hack__uname(a)
#endif

#include <sys/utsname.h>

#if defined(CONDOR_KILLS_UNAME_DEFINITIONS)
	#undef uname
	#undef _uname

	/* Now, we must provide the protoypes that we lost. */

	BEGIN_C_DECLS
	extern int uname( struct utsname * );
	extern int _uname( struct utsname * );
	END_C_DECLS
#endif

#endif /* _CONDOR_FIX_SYS_UTSNAME_H */
