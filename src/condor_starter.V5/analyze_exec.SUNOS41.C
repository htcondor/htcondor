/****************************************************************
Purpose:
	The checkpoint (a.out) file transferred from the submitting machine
	should be a valid ULTRIX executable file, and should have been linked
	with the Condor remote execution library.  Here we check the
	magic numbers to determine if we have a valid executable file, and
	also look for a well known symbol from the Condor library (MAIN)
	to ensure proper linking.
Portability:
	This code depends upon the executable format for ULTRIX.4.2 systems,
	and is not portable to other systems.
******************************************************************/

#define _ALL_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <sys/file.h>
#include <sys/exec.h>
#include "proto.h"
#include <nlist.h>

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */



extern "C" {
	int nlist( char *FileName, struct nlist *N1);
}

int magic_check( char *a_out )
{
	int		exec_fd;
	struct exec	header;
	int		nbytes;


	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}
	dprintf( D_ALWAYS, "Opened file \"%s\", fd = %d\n", a_out, exec_fd );

	errno = 0;
	nbytes = read( exec_fd, (char *)&header, sizeof(header) );
	if( nbytes != sizeof(header) ) {
		dprintf(D_ALWAYS,
			"Error on read(%d,0x%x,%d), nbytes = %d, errno = %d\n",
			exec_fd, (char *)&header, sizeof(header), nbytes, errno
		);
		close( exec_fd );
		return -1;
	}

	close( exec_fd );

	if( header.a_magic != ZMAGIC ) {
		dprintf( D_ALWAYS, "\"%s\": BAD MAGIC NUMBER\n", a_out );
		return -1;
	}
	if( header.a_machtype != M_SPARC ) {
		dprintf( D_ALWAYS,
			"\"%s\": NOT COMPILED FOR SPARC ARCHITECTURE\n", a_out
		);
		return -1;
	}
	if( header.a_dynamic ) {
		dprintf( D_ALWAYS, "\"%s\": LINKED FOR DYNAMIC LOADING\n", a_out );
		return -1;
	}

	return 0;
}

/*
Check to see that the checkpoint file is linked with the Condor
library by looking for the symbol "MAIN".
*/
int symbol_main_check( char *name )
{
	int	status;
	struct nlist nl[2];


	nl[0].n_name = "_MAIN";
	nl[1].n_name = "";


		/* If nlist fails just return TRUE - executable may be stripped. */
	status = nlist( name, nl );
	if( status < 0 ) {
		dprintf(D_ALWAYS, "Error: nlist(\"%s\",0x%x) returns %d, errno = %d\n",
													name, nl, status, errno);
		return(0);
	}

	if( nl[0].n_type == 0 ) {
		dprintf( D_ALWAYS, "No symbol \"MAIN\" in executable(%s)\n", name );
		return(-1);
	}
	dprintf( D_ALWAYS, "Symbol MAIN check - OK\n" );
	return 0;
}

int
calc_hdr_blocks()
{
	return (sizeof(struct exec) + 1023) / 1024;
}

int
calc_text_blocks( char *a_out )
{
	int		exec_fd;
	struct exec	header;


	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}

	if( read(exec_fd,(char *)&header,sizeof(header)) != sizeof(header) ) {
		dprintf(D_ALWAYS,
			"read(%d,0x%x,%d)", exec_fd, (char *)&header, sizeof(header)
		);
		close( exec_fd );
		return -1;
	}

	close( exec_fd );

	return header.a_text / 1024;
}
