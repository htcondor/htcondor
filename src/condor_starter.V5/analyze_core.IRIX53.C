/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.  The LINUX
******************************************************************/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <sys/file.h>
#include <sys/user.h>
#include <a.out.h>


int
core_is_valid( char *name )
{

	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness, not yet implemented\n", name
	);

	return FALSE;
}
