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




int magic_check( char *a_out )
{
	dprintf( D_ALWAYS, "magic_check not yet implemented.\n" );
	return -1;

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
	dprintf( D_ALWAYS, "symbol_main_check not yet implemented\n");
	return -1;
}

int
calc_hdr_blocks()
{
	dprintf( D_ALWAYS, "calc_hdr_blocks not yet implemented\n");
	return(-1);
}

int
calc_text_blocks( char *name )
{
	dprintf( D_ALWAYS, "cal_text_blocks not yet implemented\n");
}
