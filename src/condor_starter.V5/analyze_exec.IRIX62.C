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
	The checkpoint (a.out) file transferred from the submitting machine
	should be a valid IRIX executable file, and should have been linked
	with the Condor remote execution library.  Here we check the
	magic numbers to determine if we have a valid executable file, and
	also look for a well known symbol from the Condor library (MAIN)
	to ensure proper linking.
Portability:
	This code depends upon the executable format for IRIX 5.3 systems,
	and is not portable to other systems.
******************************************************************/

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include <sys/file.h>
#include <a.out.h>
#include <nlist.h>


int magic_check( char *a_out )
{
	int		exec_fd;
	int		nbytes;
	struct filehdr header;

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

	if( header.f_magic != 0x7f45 ) {
		dprintf( D_ALWAYS, "\"%s\": BAD MAGIC NUMBER\n", a_out );
		dprintf( D_ALWAYS, "0x%x != 0x%x\n", header.f_magic, 0x7f45 );
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
	struct nlist nl[3];

	nl[0].n_name = "MAIN";
	nl[1].n_name = "_condor_prestart";
	nl[2].n_name = "";

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

	if( nl[1].n_type == 0 ) {
		dprintf( D_ALWAYS,
				"No symbol \"_condor_prestart\" in executable(%s)\n", name );
		return(-1);
	}

	dprintf( D_ALWAYS, "Symbol MAIN check - OK\n" );
	return 0;
}

int
calc_hdr_blocks()
{
	return (sizeof(struct filehdr) + sizeof(struct aouthdr) +
			sizeof(struct scnhdr)*3 + 1023) / 1024;
}

int
calc_text_blocks( char *a_out )
{
	int		exec_fd;
	int		nbytes;
	struct filehdr header;
	struct aouthdr auxhdr;

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

	errno = 0;
	nbytes = read( exec_fd, (char *)&auxhdr, sizeof(auxhdr) );
	if( nbytes != sizeof(auxhdr) ) {
		dprintf(D_ALWAYS,
			"Error on read(%d,0x%x,%d), nbytes = %d, errno = %d\n",
			exec_fd, (char *)&auxhdr, sizeof(auxhdr), nbytes, errno
		);
		close( exec_fd );
		return -1;
	}

	close( exec_fd );

	return auxhdr.tsize / 1024;
}
