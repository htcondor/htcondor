/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.
******************************************************************/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
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
