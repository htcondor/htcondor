/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

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

	if( !(header.c_flag & FULL_CORE) ) {
		dprintf( D_ALWAYS, "Core not complete - no data\n" );
		return FALSE;
	}

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

	total_size = (off_t)header.c_stack + (off_t)header.c_size +
				 (off_t)header.c_u.u_dsize;

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
