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
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.  The ULTRIX
	core consists of the uarea followed by the data and stack segments.
	We check the sizes of the data and stack segments recorded in the
	uarea and calculate the size of the core file.  We then check to
	see that the actual core file size matches our calculation.
Bugs:
    OSF1 has problems with sys/user.h and the C++ compiler, this is just a 
    dummy.
******************************************************************/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include <sys/file.h>


int
core_is_valid( char *name )
{
	int		fd;

	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", name
	);

	if( (fd=open(name,O_RDONLY)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open core file \"%s\"\n", name );
		return FALSE;
	}

	close( fd );
	return TRUE;
}
