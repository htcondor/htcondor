/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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



