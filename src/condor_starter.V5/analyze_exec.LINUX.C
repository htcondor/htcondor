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
** Author:  Mike Greger
**
*/ 

/****************************************************************
Purpose:
	The checkpoint (a.out) file transferred from the submitting machine
	should be a valid LINUX executable file, and should have been linked
	with the Condor remote execution library.  Here we check the
	magic numbers to determine if we have a valid executable file, and
	also look for a well known symbol from the Condor library (MAIN)
	to ensure proper linking.
Portability:
	This code depends upon the executable format for LINUX 1.1.x systems,
	and is not portable to other systems.
******************************************************************/

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "condor_syscall_mode.h"
#include <sys/file.h>
#include <a.out.h>

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */




	extern "C" {
		int nlist( char *FileName, struct nlist *N1);
	}

int magic_check( char *a_out )
{
	int		exec_fd;
	int		nbytes;
	struct exec 	file_hdr;


	if( (exec_fd=open(a_out,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", a_out );
		return -1;
	}

	errno = 0;
        nbytes = read( exec_fd, (char *)&file_hdr, sizeof(file_hdr) );
        if( nbytes != sizeof(file_hdr) ) {
                dprintf(D_ALWAYS,
                        "Error on read(%d,0x%x,%d), nbytes = %d, errno = %d\n",
                        exec_fd, (char *)&file_hdr, sizeof(file_hdr), nbytes, errno
                );
                close( exec_fd );
                return -1;
        }
        close( exec_fd );

        if( N_MAGIC(file_hdr) != QMAGIC ) {
                dprintf( D_ALWAYS, "\"%s\": BAD MAGIC NUMBER\n", a_out );
                return -1;
        }
        if( N_MACHTYPE(file_hdr) != M_386 ) {
                dprintf( D_ALWAYS, "\"%s\": NOT COMPILED FOR ix86 ARCHITECTURE\n", a_out );
                return -1;
        }

	return 0;
}

/*
  - Check to see that the checkpoint file is linked with the Condor
  - library by looking for the symbol "MAIN".

  As we move into the new checkpointing and remote system call mechanisms
  the use of "MAIN" may dissappear.  It seems what we really want to
  know here is whether the executable is linked for remote system
  calls.  We should therefore look for a symbol which must be present
  if that is the case.
*/
int symbol_main_check( char *name )
{
	int		status;
	struct nlist	nl[2];

	nl[0].n_un.n_name = "MAIN";
	nl[1].n_un.n_name = "";

        status = nlist( name, nl );
        if( status < 0 ) {
                dprintf(D_ALWAYS, "Error: nlist(\"%s\",0x%x) returns %d, errno = %d\n",
			name, nl, status, errno);
                return(0); /* May have been stripped */
        }

        if( nl[0].n_type == 0 ) {
                 dprintf( D_ALWAYS, "No symbol \"MAIN\" in executable(%s)\n", name);
		 return(-1);
        }

        dprintf( D_ALWAYS, "Symbol MAIN check - OK\n" );
	return 0;
}

int
calc_hdr_blocks()
{
	return(sizeof(struct exec));
}

int
calc_text_blocks( char *name )
{
	int		exec_fd;
	struct exec	exec_struct;

	if( (exec_fd=open(name,O_RDONLY,0)) < 0 ) {
		dprintf( D_ALWAYS, "open(%s,O_RDONLY,0)", name );
		return -1;
	}

	return exec_struct.a_text / 1024;
}
