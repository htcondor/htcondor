/****************************************************************
Author:
	Michael Greger, August 1, 1996

Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.

	This code requires a *correct* libbfd, liberty, bfd.h and ansidecl.h.
	The code has been tested with libbfd.so.2.6.0.12.  Correct versions
	of the heasers and libraries should be included with this distribution.
******************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <sys/file.h>
#include <sys/user.h>
#include <bfd.h>


int
core_is_valid( char *core_fn )
{
	bfd		*bfdp;
	long	storage_needed;


	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", core_fn
	);

	// We only check the header of the core.  Most cores will be complete
	// if they exist at all (unless the disk fills).  In any case, my
	// opinion is that any core is better than nothing!  As long as the
	// header is intact, gdb should try to read it and give the user
	// some feedback. -- Greger
	bfd_init();
	if((bfdp=bfd_openr(core_fn, 0))!=NULL) {
		dprintf(D_ALWAYS, "\n\nbfdopen(%s) succeeded\n", core_fn);
		// Check if it is a core.
		if(bfd_check_format(bfdp, bfd_core)) {
			dprintf(D_ALWAYS, "File type=bfd_core\n");
			bfd_close(bfdp);
			return TRUE;
		} else {
			dprintf(D_ALWAYS, "File %s is not a valid core file\n", core_fn);
			bfd_close(bfdp);
			return FALSE;
		}
	} else {
		dprintf(D_ALWAYS, "bfdopen(%s) failed\n", core_fn);
		return -1;
	}
}



