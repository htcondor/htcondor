/****************************************************************
Purpose:
	Look for a "core" file at a given location.  Return TRUE if it exists
	and appears complete and correct, and FALSE otherwise.  The LINUX
????????????????
******************************************************************/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <sys/file.h>
#include <sys/user.h>
#include <a.out.h>


int
core_is_valid( char *name )
{
	int		fd;
	struct user	core;
	off_t		file_size;
	int		correct_size;


	dprintf( D_ALWAYS,
		"Analyzing core file \"%s\" for existence and completeness\n", name
	);

	if( (fd=open(name,O_RDONLY)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open core file \"%s\"\n", name );
		return FALSE;
	}

        if( read(fd,(char *)&core,sizeof(core)) != sizeof(core) ) {
                dprintf( D_ALWAYS, "Can't read header from core file\n" );
                (void)close( fd );
                return FALSE;
        }
        file_size = lseek( fd, (off_t)0, SEEK_END );
	close( fd );

	if( core.magic != CMAGIC) {
		dprintf( D_ALWAYS, "File %s is not a valid core file\n",
			name);
		return FALSE;
	}

        dprintf( D_ALWAYS,
                "Core file - magic number OK\n"
        );

	correct_size = UPAGES*NBPG + core.u_dsize*NBPG + core.u_ssize*NBPG;
        if( file_size < correct_size ) {
                dprintf( D_ALWAYS,
                        "file_size should be %d, but is %d\n", correct_size, file_size);
                return FALSE;
        } else {
                dprintf( D_ALWAYS,
                        "Core file_size of %d is correct\n", file_size );
                return TRUE;
        }


	return TRUE;
}
