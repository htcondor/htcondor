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

Addendum:
	Ripped out libbfd usage. Also, don't even check the core for validity
	beyond its existance. Let the user figure it out when they 
	use gdb on it. -psilord 4/22/2002
******************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"

int
core_is_valid( char *core_fn )
{
	struct stat buf;

	if (stat(core_fn, &buf) < 0)
	{
		return FALSE;
	}

	/* if the OS gave us something, we should give it to the user and let
		THEM figure out if it was corrupt or not. */

	return TRUE;
}



