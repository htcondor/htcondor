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
