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
	and appears complete and correct and FALSE otherwise.  The AIX core
	file has several flags in the header which we check, but we also do a
	"sanity" check on the size of the file.  A complete core dump will
	include a header, generally followed by one or more loader modules,
	followed by the user stack, and finally followed by the user data.
	The header gives us the location of the stack, the size of the stack,
	and the size of the data.  The total size of the file should then be
	the location of the stack + the size of the stack + the size of the
	data.  Calculate that and check it against the actual file size.
Portability:
	This code depends upon the core file format for AIX3.2 systems, and
	is not portable to other systems.
******************************************************************/

#include "dir.AIX32.h"

#include "condor_common.h"
#include "condor_xdr.h"
#include "condor_jobqueue.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include <sys/file.h>
#include <sys/user.h>
#include <core.h>
#include "proto.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

int
core_is_valid( char *name )
{
	int		fd;
	off_t	file_size;
	off_t	total_size;
	struct core_dump	header;


	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", name
	);

	if( (fd=open(name,O_RDONLY)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open core file \"%s\"", name );
		return FALSE;
	}

	if( read(fd,&header,sizeof(header)) != sizeof(header) ) {
		(void)close( fd );
		dprintf( D_ALWAYS, "Can't read core file header\n" );
		return FALSE;
	}

	file_size = lseek( fd, (off_t)0, SEEK_END );
	(void)close( fd );

#if 0
	/* Don't need core for checkpointing in V5, so if we get one it's
	a user core.  In that case, the user program may very well not have
	set the special flag requesting a full dump - but might want the
	core anyway. */
	if( !(header.c_flag & FULL_CORE) ) {
		dprintf( D_ALWAYS, "Core not complete - no data\n" );
		return FALSE;
	}
#endif

	if( !(header.c_flag & UBLOCK_VALID) ) {
		dprintf( D_ALWAYS, "Core not valid - no uarea\n" );
		return FALSE;
	}

	if( !(header.c_flag & USTACK_VALID) ) {
		dprintf( D_ALWAYS, "Core not complete - no stack\n" );
		return FALSE;
	}

	if( !(header.c_flag & LE_VALID) ) {
		dprintf( D_ALWAYS, "Core not complete - no loader entries\n" );
		return FALSE;
	}

	if( header.c_flag & CORE_TRUNC ) {
		dprintf( D_ALWAYS, "Core not complete - truncated\n" );
		return FALSE;
	}

	if( (header.c_flag & FULL_CORE) ) {
		total_size = (off_t)header.c_stack + (off_t)header.c_size +
					 (off_t)header.c_u.u_dsize;
	} else {
		total_size = (off_t)header.c_stack + (off_t)header.c_size;
	}

	if( file_size != total_size ) {
		dprintf( D_ALWAYS,
			"file_size should be %d, but is %d\n", total_size, file_size
		);
		return FALSE;
	} else {
		dprintf( D_ALWAYS,
			"Core file_size of %d is correct\n", file_size
		);
		return TRUE;
	}

}
