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

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <sys/file.h>
#include <a.out.h>

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */


#define COFF_MAGIC MIPSELMAGIC
#define AOUT_MAGIC ZMAGIC

typedef struct filehdr  FILE_HDR;
typedef struct aouthdr  AOUT_HDR;
#define FILE_HDR_SIZ    sizeof(FILE_HDR)
#define AOUT_HDR_SIZ    sizeof(AOUT_HDR)

extern "C" {
	int nlist( char *FileName, struct nlist *N1);
}

int magic_check( char *a_out )
{
	int		exec_fd;
	FILE_HDR	file_hdr;
	AOUT_HDR	aout_hdr;
	HDRR		symbolic_hdr;


	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}

		/* Deal with coff file header */
	if( read(exec_fd,&file_hdr,FILE_HDR_SIZ) != FILE_HDR_SIZ ) {
		dprintf(D_ALWAYS, "read(%d,0x%x,%d)", exec_fd, &file_hdr, FILE_HDR_SIZ);
		return -1;
	}

	if( file_hdr.f_magic != COFF_MAGIC ) {
		dprintf( D_ALWAYS, "BAD MAGIC NUMBER IN COFF HEADER\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "COFF header - magic number OK\n" );


		/* Deal with optional header (a.out header) */
    if( file_hdr.f_opthdr != AOUT_HDR_SIZ ) {
		dprintf( D_ALWAYS, "BAD A.OUT HEADER SIZE IN COFF HEADER\n" );
		return -1;
    }


    if( read(exec_fd,&aout_hdr,AOUT_HDR_SIZ) != AOUT_HDR_SIZ ) {
    	dprintf( D_ALWAYS,"read(%d,0x%x,%d)", exec_fd, &aout_hdr, AOUT_HDR_SIZ);
		return -1;
	}

	if( aout_hdr.magic != AOUT_MAGIC ) {
		dprintf( D_ALWAYS, "BAD MAGIC NUMBER IN A.OUT HEADER\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "AOUT header - magic number OK\n" );

		/* Bring symbol table header into core */
	if( lseek(exec_fd,file_hdr.f_symptr,0) != file_hdr.f_symptr ) {
		dprintf( D_ALWAYS, "lseek(%d,%d,0)", exec_fd, file_hdr.f_symptr );
		return -1;
	}
	if( read(exec_fd,&symbolic_hdr,cbHDRR) != cbHDRR ) {
		dprintf( D_ALWAYS,
			"read(%d,0x%x,%d)", exec_fd, &symbolic_hdr, cbHDRR );
		return -1;
	}

	if( symbolic_hdr.magic != magicSym ) {
		dprintf( D_ALWAYS, "Bad magic number in symbol table header\n" );
		dprintf( D_ALWAYS, " -- expected 0x%x, found 0x%x\n",
			magicSym, symbolic_hdr.magic );
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


	nl[0].n_name = "MAIN";
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
	return (FILE_HDR_SIZ + AOUT_HDR_SIZ + 1023) / 1024;
}

int
calc_text_blocks( char *name )
{
	int		exec_fd;
	FILE_HDR	file_hdr;
	AOUT_HDR	aout_hdr;


	if( (exec_fd=open(name,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", name );
		return -1;
	}

		/* Deal with coff file header */
	if( read(exec_fd,&file_hdr,FILE_HDR_SIZ) != FILE_HDR_SIZ ) {
		dprintf(D_ALWAYS, "read(%d,0x%x,%d)", exec_fd, &file_hdr, FILE_HDR_SIZ);
		return -1;
	}

	if( file_hdr.f_magic != COFF_MAGIC ) {
		dprintf( D_ALWAYS, "BAD MAGIC NUMBER IN COFF HEADER\n" );
		return -1;
	}


		/* Deal with optional header (a.out header) */
    if( file_hdr.f_opthdr != AOUT_HDR_SIZ ) {
		dprintf( D_ALWAYS, "BAD A.OUT HEADER SIZE IN COFF HEADER\n" );
		return -1;
    }


    if( read(exec_fd,&aout_hdr,AOUT_HDR_SIZ) != AOUT_HDR_SIZ ) {
    	dprintf( D_ALWAYS,"read(%d,0x%x,%d)", exec_fd, &aout_hdr, AOUT_HDR_SIZ);
		return -1;
	}

	if( aout_hdr.magic != AOUT_MAGIC ) {
		dprintf( D_ALWAYS, "BAD MAGIC NUMBER IN A.OUT HEADER\n" );
		return -1;
	}

	return aout_hdr.tsize / 1024;
}
