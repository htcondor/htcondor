/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.  The ULTRIX
	core consists of the uarea followed by the data and stack segments.
	We check the sizes of the data and stack segments recorded in the
	uarea and calculate the size of the core file.  We then check to
	see that the actual core file size matches our calculation.
Portability:
	This code depends upon the core format for ULTRIX.4.2 systems,
	and is not portable to other systems.
******************************************************************/

#define _ALL_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <sys/file.h>

#undef NULL
#include <sys/param.h>

#undef	JB_S0
#undef	JB_S1
#undef	JB_S2
#undef	JB_S3
#undef	JB_S4
#undef	JB_S5
#undef	JB_S6
#undef	JB_S7
#undef	JB_SP
#undef	JB_S8
#undef	JB_PC
#undef	JB_SR
#undef	NJBREGS
#include <sys/user.h>

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

const int	UAREA_SIZE = UPAGES * NBPG;

int
core_is_valid( char *name )
{
	int		fd;
	char	buf[ UAREA_SIZE ];
	struct user	*u_area = (struct user *)buf;
	off_t	total_size;
	off_t	file_size;

	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", name
	);

	if( (fd=open(name,O_RDONLY)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open core file \"%s\"", name );
		return FALSE;
	}

	if( read(fd,buf,UAREA_SIZE) != UAREA_SIZE ) {
		dprintf( D_ALWAYS, "Can't read uarea from core file\n" );
		(void)close( fd );
		return FALSE;
	}

	dprintf( D_ALWAYS,
		"Data Size = %d kilobytes\n", (u_area->u_dsize * NBPG) / 1024
	);
	dprintf( D_ALWAYS,
		"Stack Size = %d kilobytes\n", (u_area->u_ssize * NBPG) / 1024
	);

	total_size = (off_t)(UPAGES + u_area->u_dsize + u_area->u_ssize) *
															(off_t)NBPG;
	file_size = lseek( fd, (off_t)0, SEEK_END );
	(void)close( fd );

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
