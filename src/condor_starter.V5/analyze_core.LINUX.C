/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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



