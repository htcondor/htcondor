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
	and appears complete and correct, and FALSE otherwise.
******************************************************************/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include <sys/file.h>
#include <sys/user.h>
#include <core.out.h>


int
core_is_valid( char *name )
{
	struct coreout header;

	int		fd;
	int		total_size;
	off_t	file_size;

	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", name
	);

	if( (fd=open(name,O_RDONLY)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open core file \"%s\"", name );
		return FALSE;
	}

	if( read(fd,(char *)&header,sizeof(header)) != sizeof(header) ) {
		dprintf( D_ALWAYS, "Can't read header from core file\n" );
		(void)close( fd );
		return FALSE;
	}

	file_size = lseek( fd, (off_t)0, SEEK_END );
	close( fd );

	if( header.c_magic != CORE_MAGIC ) {
		dprintf( D_ALWAYS,
			"Core file - bad magic number (0x%x)\n", header.c_magic
		);
		return FALSE;
	}

	dprintf( D_ALWAYS,
		"Core file - magic number OK\n"
	);

	return TRUE;
}
